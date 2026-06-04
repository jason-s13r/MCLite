#include "MapScreen.h"
#include "UIManager.h"
#include "theme.h"
#include "../storage/TileLoader.h"
#include "../util/slippy.h"
#include "../hal/GPS.h"
#ifdef PLATFORM_TDECK
#include "../input/Keyboard.h"
#include "../input/Trackball.h"
#endif
#include <math.h>
#include <algorithm>
#include <esp_heap_caps.h>

namespace mclite {

static constexpr int TILE = SLIPPY_TILE_SIZE;  // 256

// Touch-button sizing: finger-friendly on T-Watch, compact on T-Deck.
#ifdef PLATFORM_TWATCH
static constexpr int MAP_BTN          = 56;
static constexpr int MAP_CORNER_INSET = 36;
#define MAP_BTN_FONT FONT_HEADING
#else
static constexpr int MAP_BTN          = 32;
static constexpr int MAP_CORNER_INSET = 0;
#define MAP_BTN_FONT FONT_NORMAL
#endif

void MapScreen::create(lv_obj_t* parent) {
    _parent = parent;
    // Widgets are built lazily in open() because the title depends on _contactName.
}

void MapScreen::show() {
    if (_screen) {
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void MapScreen::hide() {
    if (_screen) {
        lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void MapScreen::open(double contactLat, double contactLon, const String& contactName) {
    if (_screen) {
        // Already created — just update data and show
        _contactLat = contactLat;
        _contactLon = contactLon;
        _centerLat  = contactLat;
        _centerLon  = contactLon;
        _contactName = contactName;

        _zooms = TileLoader::instance().availableZooms();
        if (_zooms.empty()) {
            Serial.println("[MapScreen] no tiles available; aborting open");
            return;
        }
        pickInitialZoom();

        // Update title
        if (_titleLabel) {
            lv_label_set_text(_titleLabel, _contactName.c_str());
        }

        show();
        render();
        return;
    }

    _contactLat = contactLat;
    _contactLon = contactLon;
    _centerLat  = contactLat;
    _centerLon  = contactLon;
    _contactName = contactName;

    _zooms = TileLoader::instance().availableZooms();
    if (_zooms.empty()) {
        Serial.println("[MapScreen] no tiles available; aborting open");
        return;
    }
    pickInitialZoom();

    // Compute canvas size: full content area between status bar and footer
    _canvasW = Display::width();
    _canvasH = Display::height() - theme::STATUS_BAR_HEIGHT - theme::FOOTER_HEIGHT;

    // Canvas buffer lives in PSRAM and is reserved ONCE per device lifetime.
    // We allocate for the full display size so the buffer is reusable even
    // if dimensions change, but we only use _canvasW × _canvasH of it.
    static lv_color_t* s_cbuf = nullptr;
    const size_t bytes = (size_t)Display::width() * (size_t)Display::height() * sizeof(lv_color_t);
    if (!s_cbuf) {
        s_cbuf = (lv_color_t*)heap_caps_malloc(bytes,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_cbuf) {
        Serial.printf("[MapScreen] PSRAM alloc failed (%u B); free SPIRAM=%u "
                      "largest=%u\n", (unsigned)bytes,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        return;
    }
    _cbuf = s_cbuf;

    buildWidgets();

#ifdef PLATFORM_TDECK
    _prevGroup = UIManager::instance().inputGroup();
    _mapGroup = lv_group_create();
    lv_group_add_obj(_mapGroup, _backBtn);
    lv_group_add_obj(_mapGroup, _zoomInBtn);
    lv_group_add_obj(_mapGroup, _centerBtn);
    lv_group_add_obj(_mapGroup, _zoomOutBtn);
    lv_group_focus_obj(_backBtn);
    if (Keyboard::instance().indev())
        lv_indev_set_group(Keyboard::instance().indev(), _mapGroup);
    if (Trackball::instance().indev())
        lv_indev_set_group(Trackball::instance().indev(), _mapGroup);
#endif

    show();
    render();
}

void MapScreen::close() {
    if (!_screen) return;

#ifdef PLATFORM_TDECK
    if (_prevGroup) {
        if (Keyboard::instance().indev())
            lv_indev_set_group(Keyboard::instance().indev(), _prevGroup);
        if (Trackball::instance().indev())
            lv_indev_set_group(Trackball::instance().indev(), _prevGroup);
    }
    if (_mapGroup) {
        lv_group_del(_mapGroup);
        _mapGroup = nullptr;
    }
#endif

    hide();

    if (_onBack) _onBack();

    _prevGroup = nullptr;
    Serial.println("[MapScreen] closed");
}

void MapScreen::pickInitialZoom() {
    auto& loader = TileLoader::instance();
    int chosen = (int)_zooms.size() - 1;
    for (int i = (int)_zooms.size() - 1; i >= 0; i--) {
        uint8_t z = _zooms[i];
        TileFrac f = latLonToTileXY(_contactLat, _contactLon, z);
        int tx = (int)floor(f.x);
        int ty = (int)floor(f.y);
        if (loader.tileExists(z, tx, ty)) { chosen = i; break; }
    }
    _zoomIdx = chosen;
    _zoom = _zooms[_zoomIdx];
}

void MapScreen::buildWidgets() {
    _screen = lv_obj_create(_parent);
    lv_obj_set_size(_screen, Display::width(),
                    Display::height() - theme::STATUS_BAR_HEIGHT - theme::FOOTER_HEIGHT);
    lv_obj_align(_screen, LV_ALIGN_BOTTOM_MID, 0, -theme::FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_outline_width(_screen, 0, 0);
    lv_obj_set_style_shadow_width(_screen, 0, 0);
    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);  // start hidden

    _win = lv_win_create(_screen, theme::CHAT_HEADER_HEIGHT);
    lv_obj_set_size(_win, Display::width(),
                    Display::height() - theme::STATUS_BAR_HEIGHT - theme::FOOTER_HEIGHT);
    lv_obj_align(_win, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_win, lv_color_black(), 0);
    lv_obj_set_style_border_width(_win, 0, 0);
    lv_obj_set_style_outline_width(_win, 0, 0);
    lv_obj_set_style_shadow_width(_win, 0, 0);
    lv_obj_clear_flag(_win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_win, 0, 0);

    // Style the header
    lv_obj_t* header = lv_win_get_header(_win);
    lv_obj_set_style_bg_color(header, theme::BG_STATUS_BAR, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, theme::PAD_SMALL, 0);
    lv_obj_set_style_pad_hor(header, theme::CHAT_HEADER_PAD_HOR, 0);

    // Back button
    _backBtn = lv_win_add_btn(_win, LV_SYMBOL_LEFT, theme::BTN_HEADER_BACK_W);
    lv_obj_set_style_bg_opa(_backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(_backBtn, 0, 0);
    lv_obj_set_style_border_width(_backBtn, 0, 0);
    lv_obj_add_event_cb(_backBtn, &MapScreen::backBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* backLbl = lv_obj_get_child(_backBtn, 0);
    lv_obj_set_style_text_font(backLbl, FONT_HEADING, 0);
    lv_obj_set_style_text_color(backLbl, theme::ACCENT, 0);

    // Title
    lv_obj_t* title = lv_win_add_title(_win, _contactName.c_str());
    lv_obj_set_style_text_font(title, FONT_HEADING, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    _titleLabel = title;

    // Content area
    lv_obj_t* content = lv_win_get_content(_win);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Canvas
    _canvas = lv_canvas_create(content);
    lv_canvas_set_buffer(_canvas, _cbuf, _canvasW, _canvasH, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(_canvas, 0, 0);
    lv_obj_add_flag(_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_canvas, &MapScreen::panPressedCb,  LV_EVENT_PRESSED,  this);
    lv_obj_add_event_cb(_canvas, &MapScreen::panPressingCb, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(_canvas, &MapScreen::panReleasedCb, LV_EVENT_RELEASED, this);

    auto styleBtn = [](lv_obj_t* b) {
        lv_obj_set_size(b, MAP_BTN, MAP_BTN);
        lv_obj_set_style_radius(b, MAP_BTN / 2, 0);
        lv_obj_set_style_bg_color(b, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_70, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, theme::TEXT_SECONDARY, 0);
        lv_obj_set_style_pad_all(b, 0, 0);
#ifdef PLATFORM_TWATCH
        lv_obj_set_ext_click_area(b, 8);
#endif
    };

    auto styleLbl = [](lv_obj_t* lbl) {
        lv_obj_set_style_text_font(lbl, MAP_BTN_FONT, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
        lv_obj_center(lbl);
    };

    // Right-edge button stack
    _zoomInBtn = lv_btn_create(content);
    styleBtn(_zoomInBtn);
    lv_obj_align(_zoomInBtn, LV_ALIGN_RIGHT_MID,
                 -(theme::SAFE_AREA_RIGHT + theme::PAD_SMALL),
                 -(MAP_BTN + theme::PAD_SMALL));
    {
        lv_obj_t* lbl = lv_label_create(_zoomInBtn);
        lv_label_set_text(lbl, LV_SYMBOL_PLUS);
        styleLbl(lbl);
    }
    lv_obj_add_event_cb(_zoomInBtn, &MapScreen::zoomInCb, LV_EVENT_CLICKED, this);

    _centerBtn = lv_btn_create(content);
    styleBtn(_centerBtn);
    lv_obj_align(_centerBtn, LV_ALIGN_RIGHT_MID,
                 -(theme::SAFE_AREA_RIGHT + theme::PAD_SMALL),
                 0);
    {
        lv_obj_t* lbl = lv_label_create(_centerBtn);
        lv_label_set_text(lbl, LV_SYMBOL_GPS);
        styleLbl(lbl);
    }
    lv_obj_add_event_cb(_centerBtn, &MapScreen::centerBtnCb, LV_EVENT_CLICKED, this);

    _zoomOutBtn = lv_btn_create(content);
    styleBtn(_zoomOutBtn);
    lv_obj_align(_zoomOutBtn, LV_ALIGN_RIGHT_MID,
                 -(theme::SAFE_AREA_RIGHT + theme::PAD_SMALL),
                  (MAP_BTN + theme::PAD_SMALL));
    {
        lv_obj_t* lbl = lv_label_create(_zoomOutBtn);
        lv_label_set_text(lbl, LV_SYMBOL_MINUS);
        styleLbl(lbl);
    }
    lv_obj_add_event_cb(_zoomOutBtn, &MapScreen::zoomOutCb, LV_EVENT_CLICKED, this);

    // Info label (bottom-left)
    _infoLabel = lv_label_create(content);
    lv_obj_set_style_text_font(_infoLabel, FONT_BODY, 0);
    lv_obj_set_style_text_color(_infoLabel, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(_infoLabel, theme::BG_SECONDARY, 0);
    lv_obj_set_style_bg_opa(_infoLabel, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(_infoLabel, 3, 0);
    lv_obj_align(_infoLabel, LV_ALIGN_BOTTOM_LEFT,
                  (MAP_CORNER_INSET + theme::PAD_SMALL),
                 -(MAP_CORNER_INSET + theme::PAD_SMALL));

    // Screen-level key handler for Esc / +/- shortcuts (T-Deck keyboard).
    lv_obj_add_event_cb(_screen, &MapScreen::screenKeyCb, LV_EVENT_KEY, this);
}

void MapScreen::destroyWidgets() {
    if (_screen) {
        lv_obj_del(_screen);
        _screen = nullptr;
    }
    _win = _canvas = _backBtn = _zoomInBtn = _zoomOutBtn = _centerBtn = _infoLabel = _titleLabel = nullptr;
}

void MapScreen::render() {
    if (!_canvas || !_cbuf) return;
    renderTiles();
    drawOwnMarker();
    drawContactMarker();
    drawCrosshair();
    drawScaleBar();
    lv_obj_invalidate(_canvas);
    updateZoomButtons();
    updateInfoLabel();
    _lastRenderMs = millis();
}

void MapScreen::renderTiles() {
    TileFrac f = latLonToTileXY(_centerLat, _centerLon, _zoom);
    const double cpx = f.x * (double)TILE;
    const double cpy = f.y * (double)TILE;
    const double topLeftX = cpx - _canvasW / 2.0;
    const double topLeftY = cpy - _canvasH / 2.0;

    const int txMin = (int)floor(topLeftX / (double)TILE);
    const int tyMin = (int)floor(topLeftY / (double)TILE);
    const int txMax = (int)floor((topLeftX + _canvasW - 1) / (double)TILE);
    const int tyMax = (int)floor((topLeftY + _canvasH - 1) / (double)TILE);

    const int n = slippyTileCount(_zoom);
    auto& loader = TileLoader::instance();

    for (int ty = tyMin; ty <= tyMax; ty++) {
        for (int tx = txMin; tx <= txMax; tx++) {
            const int dstX = (int)(tx * TILE - topLeftX);
            const int dstY = (int)(ty * TILE - topLeftY);

            if (ty < 0 || ty >= n) continue;
            int wrappedTx = tx % n;
            if (wrappedTx < 0) wrappedTx += n;
            loader.decodeInto(_cbuf, _canvasW, _canvasH, dstX, dstY, _zoom, wrappedTx, ty);
        }
    }
}

static void drawDot(lv_color_t* buf, int bufW, int bufH, int cx, int cy, int r, lv_color_t c, lv_color_t outline) {
    for (int dy = -r - 1; dy <= r + 1; dy++) {
        const int y = cy + dy;
        if (y < 0 || y >= bufH) continue;
        for (int dx = -r - 1; dx <= r + 1; dx++) {
            const int x = cx + dx;
            if (x < 0 || x >= bufW) continue;
            const int d2 = dx * dx + dy * dy;
            if (d2 <= r * r) {
                buf[y * bufW + x] = c;
            } else if (d2 <= (r + 1) * (r + 1)) {
                buf[y * bufW + x] = outline;
            }
        }
    }
}

static void drawRect(lv_color_t* buf, int bufW, int bufH, int x0, int y0, int w, int h, lv_color_t c) {
    for (int y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= bufH) continue;
        for (int x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= bufW) continue;
            buf[y * bufW + x] = c;
        }
    }
}

void MapScreen::drawContactMarker() {
    TileFrac fk = latLonToTileXY(_contactLat, _contactLon, _zoom);
    TileFrac fc = latLonToTileXY(_centerLat,  _centerLon,  _zoom);
    const int dx = (int)((fk.x - fc.x) * (double)TILE);
    const int dy = (int)((fk.y - fc.y) * (double)TILE);
    const int px = _canvasW / 2 + dx;
    const int py = _canvasH / 2 + dy;
    if (px < -6 || px >= _canvasW + 6 || py < -6 || py >= _canvasH + 6) return;
    drawDot(_cbuf, _canvasW, _canvasH, px, py, 5,
            theme::ACCENT, lv_color_white());
}

void MapScreen::drawOwnMarker() {
    auto& gps = GPS::instance();
    FixStatus fs = gps.fixStatus();
    if (fs == FixStatus::NO_FIX) return;

    double olat, olon;
    if (fs == FixStatus::LIVE) { olat = gps.lat();       olon = gps.lon(); }
    else                       { olat = gps.cachedLat(); olon = gps.cachedLon(); }

    TileFrac fo = latLonToTileXY(olat, olon, _zoom);
    TileFrac fc = latLonToTileXY(_centerLat, _centerLon, _zoom);
    const int dx = (int)((fo.x - fc.x) * (double)TILE);
    const int dy = (int)((fo.y - fc.y) * (double)TILE);
    const int px = _canvasW / 2 + dx;
    const int py = _canvasH / 2 + dy;
    if (px < -6 || px >= _canvasW + 6 || py < -6 || py >= _canvasH + 6) return;

    lv_color_t c = (fs == FixStatus::LIVE) ? theme::ONLINE_DOT : theme::GPS_LAST_KNOWN;
    drawDot(_cbuf, _canvasW, _canvasH, px, py, 4, c, lv_color_black());
}

void MapScreen::drawCrosshair() {
    const int cx = _canvasW / 2;
    const int cy = _canvasH / 2;
    const lv_color_t c = lv_color_white();
    for (int i = 3; i <= 10; i++) {
        if (cx - i >= 0)        _cbuf[cy * _canvasW + (cx - i)] = c;
        if (cx + i < _canvasW)  _cbuf[cy * _canvasW + (cx + i)] = c;
        if (cy - i >= 0)        _cbuf[(cy - i) * _canvasW + cx] = c;
        if (cy + i < _canvasH)  _cbuf[(cy + i) * _canvasW + cx] = c;
    }
}

void MapScreen::drawScaleBar() {
    const int barLen = 50;
    const int barX   = 4;
    const int barY   = _canvasH - 20;
    drawRect(_cbuf, _canvasW, _canvasH, barX, barY, barLen, 2, lv_color_white());
    drawRect(_cbuf, _canvasW, _canvasH, barX,              barY - 3, 1, 8, lv_color_white());
    drawRect(_cbuf, _canvasW, _canvasH, barX + barLen - 1, barY - 3, 1, 8, lv_color_white());
}

void MapScreen::updateInfoLabel() {
    const double mpp = metersPerPixel(_centerLat, _zoom);
    const double metersPer50 = mpp * 50.0;
    char buf[48];
    if (metersPer50 >= 1000.0) {
        snprintf(buf, sizeof(buf), "z=%u  %.1f km", (unsigned)_zoom, metersPer50 / 1000.0);
    } else {
        snprintf(buf, sizeof(buf), "z=%u  %d m", (unsigned)_zoom, (int)(metersPer50 + 0.5));
    }
    lv_label_set_text(_infoLabel, buf);
    lv_obj_align(_infoLabel, LV_ALIGN_BOTTOM_LEFT,
                  (MAP_CORNER_INSET + theme::PAD_SMALL),
                 -(MAP_CORNER_INSET + theme::PAD_SMALL));
}

void MapScreen::updateZoomButtons() {
    const bool canIn  = (_zoomIdx + 1) < (int)_zooms.size();
    const bool canOut = _zoomIdx > 0;
    if (canIn)  lv_obj_clear_state(_zoomInBtn,  LV_STATE_DISABLED);
    else        lv_obj_add_state(_zoomInBtn,    LV_STATE_DISABLED);
    if (canOut) lv_obj_clear_state(_zoomOutBtn, LV_STATE_DISABLED);
    else        lv_obj_add_state(_zoomOutBtn,   LV_STATE_DISABLED);
}

// --- callbacks ---

void MapScreen::backBtnCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (self) self->close();
}

void MapScreen::zoomInCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_zoomIdx + 1 < (int)self->_zooms.size()) {
        self->_zoomIdx++;
        self->_zoom = self->_zooms[self->_zoomIdx];
        self->render();
    }
}

void MapScreen::zoomOutCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_zoomIdx > 0) {
        self->_zoomIdx--;
        self->_zoom = self->_zooms[self->_zoomIdx];
        self->render();
    }
}

void MapScreen::centerBtnCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    self->_centerLat = self->_contactLat;
    self->_centerLon = self->_contactLon;
    self->render();
}

void MapScreen::screenKeyCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    uint32_t key = lv_event_get_key(e);
    switch (key) {
        case LV_KEY_ESC:
            self->close();
            break;
        case '+': case '=':
            MapScreen::zoomInCb(e);
            break;
        case '-': case '_':
            MapScreen::zoomOutCb(e);
            break;
        default:
            break;
    }
}

// --- pan handlers ---

void MapScreen::panPressedCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_indev_get_point(indev, &self->_panLast);
    self->_panActive = true;
}

void MapScreen::panPressingCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_panActive) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t now;
    lv_indev_get_point(indev, &now);

    int dxPx = now.x - self->_panLast.x;
    int dyPx = now.y - self->_panLast.y;
    if (dxPx == 0 && dyPx == 0) return;

    const double s = 360.0 / (256.0 * (double)(1 << self->_zoom));
    const double cosLat = cos(self->_centerLat * M_PI / 180.0);
    self->_centerLon += -(double)dxPx * s;
    self->_centerLat += (double)dyPx * s * cosLat;

    if (self->_centerLat >  SLIPPY_LAT_MAX) self->_centerLat =  SLIPPY_LAT_MAX;
    if (self->_centerLat < -SLIPPY_LAT_MAX) self->_centerLat = -SLIPPY_LAT_MAX;
    while (self->_centerLon >= 180.0) self->_centerLon -= 360.0;
    while (self->_centerLon < -180.0) self->_centerLon += 360.0;

    self->_panLast = now;

    if (millis() - self->_lastRenderMs >= 30) {
        self->render();
    }
}

void MapScreen::panReleasedCb(lv_event_t* e) {
    MapScreen* self = static_cast<MapScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_panActive) return;
    self->_panActive = false;
    self->render();
}

}  // namespace mclite
