#pragma once

#include <cstdint>

namespace mclite {

// PCF85063A I2C RTC on T-Watch Ultra (0x51 on the shared Wire bus).
// Battery-backed via the watch's coin cell, so it keeps time across reboots
// and through GPS-cold periods. Wire.begin() must be called before init().
//
// Usage:
//   at boot, call init() once after Wire.begin().
//   if isValid() returns true, the RTC has a plausible time we can trust:
//     epoch = getEpoch(); → seed TimeHelper::syncSystemClock().
//   when GPS later gets its first lock, call setEpoch(currentGpsEpoch) so the
//     RTC stays accurate against the GPS reference.
//
// Time format is Unix UTC epoch seconds, matching the rest of MCLite.
class Rtc {
public:
    static Rtc& instance();

    bool init();
    bool isReady() const { return _ready; }

    // True iff the oscillator is running AND the year read back is plausible
    // (>= 2024). When the coin cell is missing or first-power-on, the OS
    // (oscillator stopped) flag is set and we treat the time as invalid.
    bool isValid();

    // 0 if the RTC is not valid (read failure / OS bit set / pre-2024 year).
    uint32_t getEpoch();

    // Writes the epoch into the RTC and clears the OS flag.
    bool setEpoch(uint32_t utcEpoch);

private:
    Rtc() = default;
    bool _ready = false;
};

}  // namespace mclite
