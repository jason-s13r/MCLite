#include "hal/twatch/TouchTWatch.h"
#include "util/log.h"
#include "hal/Display.h"
#include "hal/boards/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <touch/TouchDrvCST92xx.h>

namespace mclite {
namespace {
TouchDrvCST92xx g_touch;
}

TouchTWatch& TouchTWatch::instance() {
    static TouchTWatch inst;
    return inst;
}

bool TouchTWatch::init() {
    g_touch.setPins(-1, TWATCH_TOUCH_INT);  // reset is on XL9555, not a direct GPIO
    if (!g_touch.begin(Wire, TWATCH_TOUCH_I2C_ADDR, TWATCH_I2C_SDA, TWATCH_I2C_SCL)) {
        LOGLN("[Touch] CST92xx begin failed");
        return false;
    }

    lv_indev_drv_init(&_drv);
    _drv.type      = LV_INDEV_TYPE_POINTER;
    _drv.read_cb   = readCb;
    _drv.user_data = this;
    _indev = lv_indev_drv_register(&_drv);
    return true;
}

void TouchTWatch::readCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    TouchTWatch* self = static_cast<TouchTWatch*>(drv->user_data);

    int16_t x_arr[1] = {0};
    int16_t y_arr[1] = {0};
    uint8_t points = g_touch.getPoint(x_arr, y_arr, 1);

    self->_touched = (points > 0);

    if (points > 0) {
        // Clamp to display bounds in case of edge artifacts
        int16_t x = x_arr[0];
        int16_t y = y_arr[0];
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > Display::width() - 1)  x = Display::width() - 1;
        if (y > Display::height() - 1) y = Display::height() - 1;
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

}  // namespace mclite
