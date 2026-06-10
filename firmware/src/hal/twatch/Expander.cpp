#include "hal/twatch/Expander.h"
#include "util/log.h"
#include "hal/boards/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <IoExpanderXL9555.hpp>

namespace mclite {
namespace {
IoExpanderXL9555 g_xl9555;
}

Expander& Expander::instance() {
    static Expander inst;
    return inst;
}

bool Expander::init() {
    if (!g_xl9555.begin(Wire, TWATCH_XL9555_ADDR, TWATCH_I2C_SDA, TWATCH_I2C_SCL)) {
        LOGLN("[Expander] XL9555 begin failed");
        _ready = false;
        return false;
    }
    g_xl9555.pinMode(TWATCH_EXP_DRV_EN,     OUTPUT);
    g_xl9555.pinMode(TWATCH_EXP_DISP_EN,    OUTPUT);
    g_xl9555.pinMode(TWATCH_EXP_TOUCH_RST,  OUTPUT);
    g_xl9555.pinMode(TWATCH_EXP_LORA_RF_SW, OUTPUT);
    g_xl9555.pinMode(TWATCH_EXP_SD_DET,     INPUT);
    _ready = true;
    return true;
}

void Expander::writePin(uint8_t pin, bool high) {
    if (!_ready) return;
    g_xl9555.digitalWrite(pin, high ? HIGH : LOW);
}

bool Expander::readPin(uint8_t pin) {
    if (!_ready) return false;
    return g_xl9555.digitalRead(pin) == HIGH;
}

}  // namespace mclite
