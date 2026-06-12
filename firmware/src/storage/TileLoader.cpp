#include "TileLoader.h"
#include "util/log.h"
#include "SDCard.h"
#include "../util/slippy.h"
#include <PNGdec.h>
#include <SD.h>
#include <Arduino.h>
#include <algorithm>
#include <new>

namespace mclite {

namespace {

// Context passed to the PNGdec draw callback: destination canvas + where this
// tile should land within it.
struct DrawCtx {
    lv_color_t* buf;
    int bufW;
    int bufH;
    int dstX;   // top-left x in canvas for this tile
    int dstY;   // top-left y in canvas for this tile
};

// PNGdec file callbacks: wrap an Arduino SD `File` via PNGFILE::fHandle.
void* pngOpenCb(const char* filename, int32_t* size) {
    File* f = new File(SD.open(filename, FILE_READ));
    if (!f || !*f) {
        delete f;
        return nullptr;
    }
    *size = f->size();
    return f;
}

void pngCloseCb(void* handle) {
    File* f = static_cast<File*>(handle);
    if (f) {
        f->close();
        delete f;
    }
}

int32_t pngReadCb(PNGFILE* pFile, uint8_t* pBuf, int32_t iLen) {
    File* f = static_cast<File*>(pFile->fHandle);
    if (!f) return 0;
    return f->read(pBuf, iLen);
}

int32_t pngSeekCb(PNGFILE* pFile, int32_t iPosition) {
    File* f = static_cast<File*>(pFile->fHandle);
    if (!f) return 0;
    return f->seek(iPosition) ? iPosition : 0;
}

}  // namespace

// File-scope pointer used by the PNGdec draw callback to call back into
// getLineAsRGB565(). PNGdec's draw-callback signature has no room for a
// PNG* parameter, so we stash the current instance here during decode().
static PNG* _pngCurrent = nullptr;

namespace {

// Scanline callback: convert to RGB565, blit into the canvas buffer with
// per-pixel clipping. LVGL is configured with LV_COLOR_16_SWAP=1, so we ask
// PNGdec for big-endian RGB565 to match.
int pngDrawCb(PNGDRAW* pDraw) {
    DrawCtx* ctx = static_cast<DrawCtx*>(pDraw->pUser);
    if (!ctx || !ctx->buf || !_pngCurrent) return 0;

    static uint16_t lineBuf[256];  // one scanline of a 256-wide tile
    // Defense in depth: decodeInto() already rejects tiles wider than 256px,
    // but a stray wide scanline here would overflow lineBuf — abort the decode.
    if (pDraw->iWidth > 256) return 0;
    _pngCurrent->getLineAsRGB565(pDraw, lineBuf, PNG_RGB565_BIG_ENDIAN, 0);

    const int y = ctx->dstY + pDraw->y;
    if (y < 0 || y >= ctx->bufH) return 1;  // row off-canvas; keep decoding

    const int w = pDraw->iWidth;
    lv_color_t* dstRow = ctx->buf + y * ctx->bufW;
    for (int i = 0; i < w; i++) {
        const int x = ctx->dstX + i;
        if (x < 0 || x >= ctx->bufW) continue;
        // lineBuf holds RGB565 big-endian; lv_color_t at 16-bit with
        // LV_COLOR_16_SWAP=1 stores the same byte-swapped layout.
        lv_color_t c;
        c.full = lineBuf[i];
        dstRow[x] = c;
    }
    return 1;
}

// Fill a 256x256 area (clipped to canvas) with mid-grey as a placeholder for
// a missing tile.
void fillGrey(lv_color_t* buf, int bufW, int bufH, int dstX, int dstY) {
    const int tile = 256;
    const lv_color_t grey = lv_color_make(0x60, 0x60, 0x60);
    const int x0 = std::max(0, dstX);
    const int y0 = std::max(0, dstY);
    const int x1 = std::min(bufW, dstX + tile);
    const int y1 = std::min(bufH, dstY + tile);
    for (int y = y0; y < y1; y++) {
        lv_color_t* row = buf + y * bufW;
        for (int x = x0; x < x1; x++) row[x] = grey;
    }
}

// True if (z, tx, ty) is a valid slippy-tile coordinate: z in [0,19] and the
// tile indices within [0, 2^z) for that zoom. Guards against negative/overflow
// indices producing nonsense SD paths.
bool tileCoordValid(uint8_t z, int tx, int ty) {
    if (z > 19) return false;
    const int n = 1 << z;  // tiles per axis at this zoom
    return tx >= 0 && tx < n && ty >= 0 && ty < n;
}

}  // namespace

TileLoader& TileLoader::instance() {
    static TileLoader inst;
    return inst;
}

void TileLoader::init() {
    if (_initialised) return;
    scan();
    _initialised = true;
}

void TileLoader::scan() {
    _zooms.clear();
    _present = false;

    auto& sd = SDCard::instance();
    if (!sd.isMounted()) return;
    if (!sd.dirExists("/tiles")) return;

    File root = SD.open("/tiles");
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return;
    }
    File entry = root.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String name = entry.name();
            // Arduino SD may return full path or bare name; take last segment.
            int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            if (!name.startsWith("._") && name.length() > 0 && name.length() <= 2) {
                bool numeric = true;
                for (size_t i = 0; i < name.length(); i++) {
                    if (!isdigit((unsigned char)name[i])) { numeric = false; break; }
                }
                if (numeric) {
                    int z = name.toInt();
                    if (z >= 0 && z <= 19) _zooms.push_back((uint8_t)z);
                }
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    std::sort(_zooms.begin(), _zooms.end());
    _zooms.erase(std::unique(_zooms.begin(), _zooms.end()), _zooms.end());
    _present = !_zooms.empty();

    LOGF("[TileLoader] /tiles: %d zoom levels (%u..%u)\n",
                  (int)_zooms.size(),
                  _zooms.empty() ? 0u : (unsigned)_zooms.front(),
                  _zooms.empty() ? 0u : (unsigned)_zooms.back());
}

bool TileLoader::tilesAvailable() {
    if (!_initialised) init();
    return _present;
}

const std::vector<uint8_t>& TileLoader::availableZooms() {
    if (!_initialised) init();
    return _zooms;
}

bool TileLoader::tileExists(uint8_t z, int tx, int ty) {
    if (!tilesAvailable()) return false;
    if (!tileCoordValid(z, tx, ty)) return false;
    char path[48];
    snprintf(path, sizeof(path), "/tiles/%u/%d/%d.png", (unsigned)z, tx, ty);
    return SDCard::instance().fileExists(path);
}

bool TileLoader::computeCenterFromTiles(double& lat, double& lon) {
    if (!tilesAvailable() || _zooms.empty()) return false;

    // Pick a zoom level for scanning: prefer a mid-range zoom (6-10) which has
    // far fewer tiles than high zooms (e.g. 19). This avoids freezing when
    // the user has deep tile pyramids. Fall back to lowest zoom if no mid.
    uint8_t z = _zooms.front();
    for (uint8_t candidate : _zooms) {
        if (candidate >= 6 && candidate <= 10) {
            z = candidate;
            break;
        }
    }

    char zPath[24];
    snprintf(zPath, sizeof(zPath), "/tiles/%u", (unsigned)z);

    auto& sd = SDCard::instance();
    if (!sd.isMounted() || !sd.dirExists(zPath)) return false;

    File zRoot = SD.open(zPath);
    if (!zRoot || !zRoot.isDirectory()) {
        if (zRoot) zRoot.close();
        return false;
    }

    int minX = INT_MAX, maxX = INT_MIN;
    int minY = INT_MAX, maxY = INT_MIN;
    uint32_t count = 0;
    constexpr uint32_t MAX_SCAN = 2000;  // hard cap to prevent UI freeze

    File xDir = zRoot.openNextFile();
    while (xDir) {
        if (xDir.isDirectory()) {
            String xName = xDir.name();
            int slash = xName.lastIndexOf('/');
            if (slash >= 0) xName = xName.substring(slash + 1);
            if (!xName.startsWith("._")) {
                int tx = xName.toInt();
                if (tx >= 0 || xName == "0") {
                    File yFile = xDir.openNextFile();
                    while (yFile) {
                        if (count >= MAX_SCAN) {
                            yFile.close();
                            break;
                        }
                        String yName = yFile.name();
                        int ySlash = yName.lastIndexOf('/');
                        if (ySlash >= 0) yName = yName.substring(ySlash + 1);
                        if (!yName.startsWith("._") && yName.endsWith(".png")) {
                            String core = yName.substring(0, yName.length() - 4);
                            int ty = core.toInt();
                            if (ty >= 0 || core == "0") {
                                if (tx < minX) minX = tx;
                                if (tx > maxX) maxX = tx;
                                if (ty < minY) minY = ty;
                                if (ty > maxY) maxY = ty;
                                count++;
                            }
                        }
                        yFile.close();
                        yFile = xDir.openNextFile();
                    }
                }
            }
        }
        xDir.close();
        if (count >= MAX_SCAN) break;
        xDir = zRoot.openNextFile();
    }
    zRoot.close();

    if (count == 0) return false;

    // Use the bounding-box center (more robust than mean against outliers)
    double cx = (minX + maxX) / 2.0 + 0.5;  // +0.5 for tile center
    double cy = (minY + maxY) / 2.0 + 0.5;
    tileXYToLatLon(cx, cy, z, lat, lon);

    LOGF("[TileLoader] tile center: z=%u tiles=%u (capped=%d) bbox(%d,%d)-(%d,%d) -> %.5f,%.5f\n",
                  (unsigned)z, count, (int)(count >= MAX_SCAN), minX, minY, maxX, maxY, lat, lon);
    return true;
}

bool TileLoader::ensurePngWorkspace() {
    if (_pngStorage) return true;
    _pngStorage = ps_malloc(sizeof(PNG));
    if (!_pngStorage) _pngStorage = malloc(sizeof(PNG));  // PSRAM exhausted: try DRAM
    return _pngStorage != nullptr;
}

bool TileLoader::decodeInto(lv_color_t* buf, int bufW, int bufH,
                            int dstX, int dstY, uint8_t z, int tx, int ty) {
    if (!buf) return false;

    if (!tileCoordValid(z, tx, ty)) {
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }

    char path[48];
    snprintf(path, sizeof(path), "/tiles/%u/%d/%d.png", (unsigned)z, tx, ty);

    if (!SDCard::instance().fileExists(path)) {
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }

    if (!ensurePngWorkspace()) {
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }

    PNG* png = new (_pngStorage) PNG();
    int rc = png->open(path, pngOpenCb, pngCloseCb, pngReadCb, pngSeekCb, pngDrawCb);
    if (rc != PNG_SUCCESS) {
        LOGF("[TileLoader] open failed: %s (rc=%d)\n", path, rc);
        png->~PNG();
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }

    // Reject any tile that isn't a standard <=256px slippy tile. The scanline
    // callback decodes into a fixed 256-wide buffer, so a wider PNG (corrupt or
    // non-standard) would overflow it. Grey-fill and bail before decoding.
    if (png->getWidth() > 256 || png->getHeight() > 256) {
        LOGF("[TileLoader] tile too large: %s (%dx%d)\n",
             path, png->getWidth(), png->getHeight());
        png->close();
        png->~PNG();
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }

    DrawCtx ctx{buf, bufW, bufH, dstX, dstY};
    _pngCurrent = png;
    rc = png->decode(&ctx, 0);
    _pngCurrent = nullptr;
    png->close();
    png->~PNG();

    if (rc != PNG_SUCCESS) {
        LOGF("[TileLoader] decode failed: %s (rc=%d)\n", path, rc);
        fillGrey(buf, bufW, bufH, dstX, dstY);
        return false;
    }
    return true;
}

}  // namespace mclite
