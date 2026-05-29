#include "hal/twatch/Rtc.h"
#include "hal/boards/board.h"
#include "util/epoch.h"
#include <Arduino.h>
#include <Wire.h>
#include <time.h>

namespace mclite {
namespace {

// PCF85063A register map (NXP datasheet). All time/date registers are BCD.
constexpr uint8_t REG_SECONDS = 0x04;  // bit 7 = OS (oscillator-stopped flag)

inline uint8_t bcdToDec(uint8_t bcd) { return ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F); }
inline uint8_t decToBcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

}  // namespace

Rtc& Rtc::instance() {
    static Rtc inst;
    return inst;
}

bool Rtc::init() {
    // Probe: try to read Control_1.
    Wire.beginTransmission(TWATCH_PCF85063_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        Serial.println("[Rtc] PCF85063A not responding");
        return false;
    }
    if (Wire.requestFrom(TWATCH_PCF85063_ADDR, (uint8_t)1) != 1) {
        Serial.println("[Rtc] PCF85063A short read");
        return false;
    }
    (void)Wire.read();
    _ready = true;
    return true;
}

bool Rtc::isValid() {
    if (!_ready) return false;
    Wire.beginTransmission(TWATCH_PCF85063_ADDR);
    Wire.write(REG_SECONDS);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(TWATCH_PCF85063_ADDR, (uint8_t)7) != 7) return false;
    uint8_t sec_raw = Wire.read();
    Wire.read();                       // min — not needed for the OS check
    Wire.read();                       // hr
    Wire.read();                       // day
    Wire.read();                       // weekday
    Wire.read();                       // month
    uint8_t yr_raw = Wire.read();
    if (sec_raw & 0x80) return false;  // OS bit set → oscillator was stopped
    return bcdToDec(yr_raw) + 2000 >= 2024;
}

uint32_t Rtc::getEpoch() {
    if (!_ready) return 0;
    Wire.beginTransmission(TWATCH_PCF85063_ADDR);
    Wire.write(REG_SECONDS);
    if (Wire.endTransmission(false) != 0) return 0;
    if (Wire.requestFrom(TWATCH_PCF85063_ADDR, (uint8_t)7) != 7) return 0;
    uint8_t sec_raw = Wire.read();
    uint8_t min_raw = Wire.read();
    uint8_t hr_raw  = Wire.read();
    uint8_t day_raw = Wire.read();
    Wire.read();                       // weekday — unused
    uint8_t mon_raw = Wire.read();
    uint8_t yr_raw  = Wire.read();

    if (sec_raw & 0x80) return 0;      // OS bit set → time invalid

    uint16_t year  = 2000 + bcdToDec(yr_raw);
    uint8_t  month = bcdToDec(mon_raw & 0x1F);
    uint8_t  day   = bcdToDec(day_raw & 0x3F);
    uint8_t  hour  = bcdToDec(hr_raw  & 0x3F);
    uint8_t  minute = bcdToDec(min_raw & 0x7F);
    uint8_t  second = bcdToDec(sec_raw & 0x7F);

    return dateToEpoch(year, month, day, hour, minute, second);
}

bool Rtc::setEpoch(uint32_t utcEpoch) {
    if (!_ready) return false;
    time_t raw = (time_t)utcEpoch;
    struct tm utc;
    gmtime_r(&raw, &utc);

    Wire.beginTransmission(TWATCH_PCF85063_ADDR);
    Wire.write(REG_SECONDS);
    Wire.write(decToBcd(utc.tm_sec)  & 0x7F);   // bit 7 = 0 clears the OS flag
    Wire.write(decToBcd(utc.tm_min)  & 0x7F);
    Wire.write(decToBcd(utc.tm_hour) & 0x3F);
    Wire.write(decToBcd(utc.tm_mday) & 0x3F);
    Wire.write(decToBcd(utc.tm_wday) & 0x07);   // 0 = Sunday, matches POSIX
    Wire.write(decToBcd(utc.tm_mon + 1) & 0x1F);
    Wire.write(decToBcd(utc.tm_year % 100));
    return Wire.endTransmission() == 0;
}

}  // namespace mclite
