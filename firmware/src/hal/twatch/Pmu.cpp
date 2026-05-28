#include "hal/twatch/Pmu.h"
#include "hal/boards/board.h"
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

namespace mclite {
namespace {
XPowersAXP2101 g_axp;
}

Pmu& Pmu::instance() {
    static Pmu inst;
    return inst;
}

bool Pmu::init() {
    if (!g_axp.init(Wire, TWATCH_I2C_SDA, TWATCH_I2C_SCL, TWATCH_AXP2101_ADDR)) {
        Serial.println("[Pmu] AXP2101 init failed");
        _ready = false;
        return false;
    }

    // Rail map verbatim from LilyGoWatchUltra.cpp::initPMU()
    g_axp.setALDO1Voltage(3300); g_axp.enableALDO1();   // SD card
    g_axp.setALDO2Voltage(3300); g_axp.enableALDO2();   // Display
    g_axp.setALDO3Voltage(3300); g_axp.enableALDO3();   // LoRa
    g_axp.setALDO4Voltage(1800); g_axp.enableALDO4();   // Sensors (BHI260)
    g_axp.setBLDO1Voltage(3300); g_axp.enableBLDO1();   // GPS
    g_axp.setBLDO2Voltage(3300); g_axp.enableBLDO2();   // Speaker
    g_axp.enableDLDO1();                                // NFC (unused v1, harmless)

    // Disable rails we don't use — default-on in some silicon revs.
    g_axp.disableDC2();
    g_axp.disableDC3();
    g_axp.disableDC4();
    g_axp.disableDC5();

    g_axp.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // PEK short-press IRQ — polled by consumeShortPress() each loop iter.
    // Long press remains a hardware shutdown via the AXP2101 itself.
    g_axp.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    g_axp.clearIrqStatus();
    g_axp.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);

    _ready = true;
    return true;
}

uint16_t Pmu::batteryMilliVolts() {
    if (!_ready) return 0;
    return g_axp.getBattVoltage();
}

uint8_t Pmu::batteryPercent() {
    if (!_ready) return 0;
    int p = g_axp.getBatteryPercent();
    if (p < 0) return 0;
    if (p > 100) return 100;
    return (uint8_t)p;
}

bool Pmu::isCharging() {
    if (!_ready) return false;
    return g_axp.isCharging();
}

bool Pmu::consumeShortPress() {
    if (!_ready) return false;
    g_axp.getIrqStatus();
    if (g_axp.isPekeyShortPressIrq()) {
        g_axp.clearIrqStatus();
        return true;
    }
    return false;
}

}  // namespace mclite
