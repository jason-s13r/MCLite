#pragma once

#include <cstdint>

namespace mclite {

// AXP2101 PMU wrapper for T-Watch Ultra. init() enables the rail map verbatim
// from LilyGoLib's LilyGoWatchUltra.cpp::initPMU():
//   ALDO1 3.3V -> SD card
//   ALDO2 3.3V -> Display
//   ALDO3 3.3V -> LoRa SX1262
//   ALDO4 1.8V -> Sensors (BHI260AP)
//   BLDO1 3.3V -> GPS
//   BLDO2 3.3V -> Speaker
//   DLDO1       -> NFC (enabled but unused for v1)
//   DC2-5 + CPUSLDO explicitly disabled (default-on in some silicon revs)
// Wire.begin() must be called before init().
class Pmu {
public:
    static Pmu& instance();

    bool init();
    uint16_t batteryMilliVolts();
    uint8_t  batteryPercent();
    bool isCharging();

    // Returns true once per short press of the upper power (PEK) button.
    // Long press is still routed to hardware shutdown by the AXP2101.
    bool consumeShortPress();

    bool isReady() const { return _ready; }

private:
    Pmu() = default;
    bool _ready = false;
};

}  // namespace mclite
