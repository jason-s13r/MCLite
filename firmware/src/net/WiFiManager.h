#pragma once

#include <Arduino.h>

// On-demand WiFi (STA) helper for the firmware auto-update feature. WiFi is
// brought up only for a scan / update-check / download and torn down after, to
// avoid battery drain and CPU/RAM contention with the LoRa mesh. WiFi (on-chip
// 2.4 GHz) does not share the SPI bus with the SX1262 LoRa radio.

namespace mclite {

struct ScannedNetwork {
    String  ssid;
    int32_t rssi = 0;
    bool    open = false;  // true = no password required
};

class WiFiManager {
public:
    static WiFiManager& instance();

    // Blocking scan (~2-4 s). Fills up to maxOut entries (named, de-duplicated,
    // sorted by signal strength). Returns the number written.
    int scan(ScannedNetwork* out, int maxOut);

    // Blocking connect with timeout. Returns true once associated (WL_CONNECTED).
    bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 15000);
    void disconnect();   // drop the link and power the radio off

    bool   isConnected();
    String connectedSsid();
    String localIp();

    // "Persistent" intent: when on, keep WiFi up and let the ESP32 stack
    // auto-reconnect in the background after a drop (non-blocking). Set true
    // after the user toggles WiFi on; cleared by disconnect().
    void   setPersistent(bool on);
    bool   wantsConnection() const { return _wantOn; }

    // Last wl_status_t from connect() (e.g. 1=no SSID, 4=auth/bad password, 6=disconnected).
    int    lastStatus() const { return _lastStatus; }

private:
    WiFiManager() = default;
    int  _lastStatus = 0;
    bool _wantOn = false;
};

}  // namespace mclite
