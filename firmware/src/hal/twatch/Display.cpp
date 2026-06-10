#include "hal/Display.h"
#include "util/log.h"
#include "hal/boards/board.h"
#include "hal/twatch/Expander.h"
#include "config/defaults.h"
#include "ui/theme.h"
#include <Arduino.h>

namespace mclite {
namespace {

// LovyanGFX device for T-Watch Ultra CO5300 AMOLED on QSPI.
class TWatchDisplay : public lgfx::LGFX_Device {
public:
    lgfx::Panel_CO5300 _panel;
    lgfx::Bus_SPI      _bus;

    TWatchDisplay() {
        auto busConfig = _bus.config();
        busConfig.spi_host   = SPI3_HOST;
        busConfig.spi_mode   = 0;
        busConfig.freq_write = 40000000;  // 80 MHz produced flickering color bars
        busConfig.freq_read  = 16000000;
        busConfig.pin_sclk   = TWATCH_DISP_SCK;
        busConfig.pin_io0    = TWATCH_DISP_D0;
        busConfig.pin_io1    = TWATCH_DISP_D1;
        busConfig.pin_io2    = TWATCH_DISP_D2;
        busConfig.pin_io3    = TWATCH_DISP_D3;
        busConfig.spi_3wire  = false;
        busConfig.use_lock   = true;
        _bus.config(busConfig);
        _panel.setBus(&_bus);

        auto panelConfig = _panel.config();
        panelConfig.pin_cs       = TWATCH_DISP_CS;
        panelConfig.pin_rst      = TWATCH_DISP_RST;
        // CO5300 init cmd 0x35 0x00 enables TE-on-V-sync output (GPIO 6).
        // Wire it as the "busy" pin so LovyanGFX waits for vertical blank
        // before each transfer — eliminates the colored-bar tearing artifacts.
        panelConfig.pin_busy     = TWATCH_DISP_TE;
        // CO5300 is wired portrait-native: the panel init sequence addresses a
        // 410-column x 502-row area (column range 22..431). Override LovyanGFX's
        // landscape-oriented defaults so setRotation(0) presents that area as-is.
        // offset_x=22 accounts for the panel's 22-column hidden left margin —
        // without it, the rightmost 22 columns aren't written and show as
        // uninitialized panel memory.
        panelConfig.memory_width  = 410;
        panelConfig.memory_height = 502;
        panelConfig.panel_width   = 410;
        panelConfig.panel_height  = 502;
        panelConfig.offset_x      = 22;
        panelConfig.offset_y      = 0;
        _panel.config(panelConfig);
        setPanel(&_panel);
    }
};

// CO5300 wants column/row-start addresses to be even AND width/height to be
// even. Panel init uses SC=22, EC=431 (width 410): start even, end odd.
// offset_x=22 (even) preserves parity, so we need LVGL coords: x1 even,
// width even (=> x2 odd). Same for Y.
//
// R4/R5 wrongly forced x2 even and produced odd width — and also fed odd
// height to LVGL's get_max_row calibration, yielding odd max_row=39 and
// sub-area y1 values like 39, 78, 117 (all odd) → panel rejected most chunks
// → near-black screen. R6 returns early when the area is already aligned (so
// the LVGL probe area 0..0, 0..39 and full-screen invalidates pass through
// untouched, keeping max_row=40 and the natural chunking), and only fixes
// small dirty regions by snapping start UP to even and trimming end to make
// width/height even.
static void co5300RounderCb(lv_disp_drv_t* /*drv*/, lv_area_t* area) {
    // X axis
    lv_coord_t w = area->x2 - area->x1 + 1;
    if ((area->x1 & 1) || (w & 1)) {
        lv_coord_t nx1 = (area->x1 + 1) & ~0x1;
        lv_coord_t nx2 = area->x2;
        if (nx1 <= nx2) {
            if ((nx2 - nx1 + 1) & 1) nx2 -= 1;
            if (nx1 <= nx2) {
                area->x1 = nx1;
                area->x2 = nx2;
            }
        }
    }

    // Y axis
    lv_coord_t h = area->y2 - area->y1 + 1;
    if ((area->y1 & 1) || (h & 1)) {
        lv_coord_t ny1 = (area->y1 + 1) & ~0x1;
        lv_coord_t ny2 = area->y2;
        if (ny1 <= ny2) {
            if ((ny2 - ny1 + 1) & 1) ny2 -= 1;
            if (ny1 <= ny2) {
                area->y1 = ny1;
                area->y2 = ny2;
            }
        }
    }
}

}  // anonymous namespace

lv_disp_draw_buf_t Display::_drawBuf;
lv_disp_drv_t      Display::_dispDrv;
lv_color_t*        Display::_buf1 = nullptr;
lv_color_t*        Display::_buf2 = nullptr;

Display& Display::instance() {
    static Display inst;
    return inst;
}

bool Display::init() {
    static TWatchDisplay lgfx;
    _lgfx_dev = &lgfx;

    _lgfx_dev->init();
    _lgfx_dev->setRotation(0);  // Panel is natively 410W x 502H portrait
    setBrightness(180);

    lv_init();

    // Single 40-row PSRAM draw buffer. Smallest flicker observed with this
    // configuration; bigger chunks make boundary artifacts more visible.
    // Phase 4d / 5 investigation continues for a real fix.
    const size_t bufSize = Display::width() * 40;
    _buf1 = (lv_color_t*)ps_malloc(bufSize * sizeof(lv_color_t));
    _buf2 = nullptr;
    if (!_buf1) {
        LOGLN("[Display] PSRAM alloc failed, falling back to DRAM");
        _buf1 = (lv_color_t*)malloc(bufSize * sizeof(lv_color_t));
    }
    if (!_buf1) {
        LOGLN("[Display] Draw buffer allocation failed");
        return false;
    }

    lv_disp_draw_buf_init(&_drawBuf, _buf1, _buf2, bufSize);

    lv_disp_drv_init(&_dispDrv);
    _dispDrv.hor_res  = Display::width();
    _dispDrv.ver_res  = Display::height();
    _dispDrv.flush_cb = flushCb;
    _dispDrv.rounder_cb = co5300RounderCb;
    _dispDrv.draw_buf = &_drawBuf;
    _dispDrv.user_data = this;
    lv_disp_drv_register(&_dispDrv);

    return true;
}

void Display::setBrightness(uint8_t level) {
    if (_lgfx_dev) _lgfx_dev->setBrightness(level);
}

void Display::flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    _lgfx_dev->startWrite();
    _lgfx_dev->setAddrWindow(area->x1, area->y1, w, h);
    _lgfx_dev->writePixels((uint16_t*)buf, w * h);
    _lgfx_dev->endWrite();
    lv_disp_flush_ready(drv);
}

void Display::flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    Display* self = (Display*)drv->user_data;
    self->flush(drv, area, buf);
}

// MCLite mark glyph — same construction as T-Deck.
static void buildBootLogo(lv_obj_t* parent) {
    constexpr int     kSize    = 64;
    constexpr int     kStroke  = 4;
    constexpr int     kDot     = 10;
    const lv_color_t  color    = theme::TEXT_PRIMARY;

    lv_obj_t* logo = lv_obj_create(parent);
    lv_obj_set_size(logo, kSize, kSize);
    lv_obj_set_style_bg_opa(logo, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(logo, 0, 0);
    lv_obj_set_style_pad_all(logo, 0, 0);
    lv_obj_set_style_pad_bottom(logo, 8, 0);
    lv_obj_clear_flag(logo, LV_OBJ_FLAG_SCROLLABLE);

    static lv_point_t pL1[] = { { 5, 56}, { 5,  8} };
    static lv_point_t pL2[] = { { 5,  8}, {32, 35} };
    static lv_point_t pL3[] = { {32, 35}, {59,  8} };
    static lv_point_t pL4[] = { {59,  8}, {59, 56} };

    static lv_style_t lineStyle;
    static bool       lineStyleInited = false;
    if (!lineStyleInited) {
        lv_style_init(&lineStyle);
        lv_style_set_line_width(&lineStyle, kStroke);
        lv_style_set_line_rounded(&lineStyle, true);
        lineStyleInited = true;
    }
    lv_style_set_line_color(&lineStyle, color);

    auto addLine = [&](lv_point_t* pts) {
        lv_obj_t* l = lv_line_create(logo);
        lv_line_set_points(l, pts, 2);
        lv_obj_add_style(l, &lineStyle, 0);
    };
    addLine(pL1);
    addLine(pL2);
    addLine(pL3);
    addLine(pL4);

    auto addDot = [&](int cx, int cy) {
        lv_obj_t* d = lv_obj_create(logo);
        lv_obj_set_size(d, kDot, kDot);
        lv_obj_set_pos(d, cx - kDot / 2, cy - kDot / 2);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, color, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    };
    addDot( 5, 56);
    addDot( 5,  8);
    addDot(32, 35);
    addDot(59,  8);
    addDot(59, 56);
}

void Display::showBootScreen(const char* bootText) {
    _bootScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_bootScreen, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_bootScreen, LV_OPA_COVER, 0);

    lv_obj_t* container = lv_obj_create(_bootScreen);
    lv_obj_set_size(container, Display::width(), Display::height());
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_center(container);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    buildBootLogo(container);

    lv_obj_t* title = lv_label_create(container);
    lv_label_set_text(title, "MCLite");
    lv_obj_set_style_text_font(title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);

    lv_obj_t* version = lv_label_create(container);
    lv_label_set_text_fmt(version, "v%s", MCLITE_VERSION);
    lv_obj_set_style_text_font(version, FONT_SMALL, 0);
    lv_obj_set_style_text_color(version, theme::TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_top(version, 4, 0);

    _bootSubtitle = lv_label_create(container);
    lv_obj_set_style_text_font(_bootSubtitle, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(_bootSubtitle, theme::ACCENT, 0);
    lv_obj_set_style_pad_top(_bootSubtitle, 12, 0);
    if (bootText && bootText[0] != '\0') {
        lv_label_set_text(_bootSubtitle, bootText);
    } else {
        lv_obj_add_flag(_bootSubtitle, LV_OBJ_FLAG_HIDDEN);
    }

    _bootStatus = lv_label_create(_bootScreen);
    lv_label_set_text(_bootStatus, "Starting...");
    lv_obj_set_style_text_font(_bootStatus, FONT_SMALL, 0);
    lv_obj_set_style_text_color(_bootStatus, theme::TEXT_TIMESTAMP, 0);
    lv_obj_align(_bootStatus, LV_ALIGN_BOTTOM_MID, 0, -16);

    lv_scr_load(_bootScreen);
    lv_timer_handler();
}

void Display::setBootText(const char* text) {
    if (!_bootSubtitle) return;
    if (text && text[0] != '\0') {
        lv_label_set_text(_bootSubtitle, text);
        lv_obj_clear_flag(_bootSubtitle, LV_OBJ_FLAG_HIDDEN);
        lv_timer_handler();
    }
}

void Display::setBootStatus(const char* status) {
    if (!_bootStatus) return;
    lv_label_set_text(_bootStatus, status);
    lv_timer_handler();
}

void Display::hideBootScreen() {
    if (_bootScreen && _bootScreen != lv_scr_act()) {
        lv_obj_del(_bootScreen);
        _bootScreen = nullptr;
        _bootStatus = nullptr;
        _bootSubtitle = nullptr;
    }
}

}  // namespace mclite
