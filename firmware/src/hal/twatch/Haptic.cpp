#include "hal/twatch/Haptic.h"
#include "util/log.h"
#include "hal/twatch/Expander.h"
#include "hal/boards/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <HapticDrivers.hpp>

namespace mclite {
namespace {
HapticDriver_DRV2605 g_drv;
}  // namespace

Haptic& Haptic::instance() {
    static Haptic inst;
    return inst;
}

bool Haptic::init() {
    // DRV2605 EN pin is on XL9555 port 6 — must be HIGH before the chip
    // responds on I2C.
    Expander::instance().writePin(TWATCH_EXP_DRV_EN, true);
    delay(2);

    if (!g_drv.begin(Wire, TWATCH_DRV2605_ADDR, TWATCH_I2C_SDA, TWATCH_I2C_SCL)) {
        LOGLN("[Haptic] DRV2605 begin failed");
        return false;
    }
    g_drv.setActuatorType(HapticActuatorType::ERM);
    g_drv.selectLibrary(1);
    g_drv.setMode(HapticMode::INTERNAL_TRIGGER);
    _ready = true;
    return true;
}

void Haptic::playMessage() {
    if (!_ready) return;
    g_drv.playEffectAsync(10);  // Double Click 100%
}

void Haptic::playButton() {
    if (!_ready) return;
    g_drv.playEffectAsync(7);   // Soft Bump 100%
}

void Haptic::playSos() {
    if (!_ready) return;
    g_drv.playEffectAsync(16);  // Alert 1000 ms 100%
}

void Haptic::stop() {
    if (!_ready) return;
    g_drv.stop();
}

}  // namespace mclite
