#pragma once

#include <LovyanGFX.hpp>
#include <lvgl.h>

#include "boards/board.h"

namespace mclite {

class Display {
public:
    static constexpr lv_coord_t width()  { return BOARD_DISP_W; }
    static constexpr lv_coord_t height() { return BOARD_DISP_H; }

    bool init();
    void setBrightness(uint8_t level);  // 0-255
    void flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf);

    // LVGL boot screen — call after init(), updates with lv_timer_handler()
    void showBootScreen(const char* bootText = nullptr);
    void setBootText(const char* text);      // Set/show boot subtitle after config load
    void setBootStatus(const char* status);  // Update progress text
    void hideBootScreen();                    // Remove boot screen, show normal UI

    static Display& instance();

private:
    Display() = default;
    // Concrete LGFX device class is per-board, defined in hal/<board>/Display.cpp.
    lgfx::LGFX_Device* _lgfx_dev = nullptr;
    static lv_disp_draw_buf_t _drawBuf;
    static lv_disp_drv_t      _dispDrv;
    static lv_color_t*        _buf1;
    static lv_color_t*        _buf2;

    // Boot screen LVGL objects
    lv_obj_t* _bootScreen   = nullptr;
    lv_obj_t* _bootSubtitle = nullptr;
    lv_obj_t* _bootStatus   = nullptr;

    static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf);
};

}  // namespace mclite
