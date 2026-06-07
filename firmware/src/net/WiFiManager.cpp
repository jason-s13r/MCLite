#include "WiFiManager.h"

#include <WiFi.h>
#include <algorithm>

namespace mclite {

WiFiManager& WiFiManager::instance() {
    static WiFiManager inst;
    return inst;
}

int WiFiManager::scan(ScannedNetwork* out, int maxOut) {
    if (!out || maxOut <= 0) return 0;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);  // clear stale state
    delay(100);

    int n = WiFi.scanNetworks();   // blocking
    if (n <= 0) { WiFi.scanDelete(); return 0; }

    int count = 0;
    for (int i = 0; i < n && count < maxOut; ++i) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;            // skip hidden / unnamed
        bool dup = false;                          // collapse duplicate SSIDs (keep strongest)
        for (int j = 0; j < count; ++j) {
            if (out[j].ssid == s) { dup = true; break; }
        }
        if (dup) continue;
        out[count].ssid = s;
        out[count].rssi = WiFi.RSSI(i);
        out[count].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        count++;
    }
    WiFi.scanDelete();

    std::sort(out, out + count, [](const ScannedNetwork& a, const ScannedNetwork& b) {
        return a.rssi > b.rssi;  // strongest first
    });
    return count;
}

bool WiFiManager::connect(const String& ssid, const String& password, uint32_t timeoutMs) {
    if (ssid.length() == 0) return false;

    // Start from a clean radio state — a prior scan / failed attempt can leave
    // STA half-open so WiFi.begin() never associates.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(true);
    delay(200);

    Serial.printf("[WiFi] connecting to '%s' (password %d chars)\n",
                  ssid.c_str(), (int)password.length());
    WiFi.begin(ssid.c_str(), password.length() ? password.c_str() : (const char*)nullptr);

    uint32_t start = millis();
    int8_t lastStatus = -1;
    while (millis() - start < timeoutMs) {
        wl_status_t st = WiFi.status();
        if (st != lastStatus) {                       // log only on change
            Serial.printf("[WiFi]  status=%d (%lus)\n", (int)st, (millis() - start) / 1000);
            lastStatus = st;
        }
        if (st == WL_CONNECTED) {
            _lastStatus = (int)st;
            Serial.printf("[WiFi] connected: ip=%s rssi=%d\n",
                          WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
            return true;
        }
        delay(150);
        yield();
    }
    _lastStatus = (int)WiFi.status();
    Serial.printf("[WiFi] connect timed out, final status=%d "
                  "(1=no-SSID, 4=connect-failed/bad-password, 6=disconnected)\n",
                  _lastStatus);
    return false;
}

void WiFiManager::setPersistent(bool on) {
    _wantOn = on;
    WiFi.setAutoReconnect(on);   // stack reconnects in the background after a drop
}

void WiFiManager::disconnect() {
    _wantOn = false;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);   // disconnect + turn off the radio
    WiFi.mode(WIFI_OFF);
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::connectedSsid() {
    return WiFi.isConnected() ? WiFi.SSID() : String();
}

String WiFiManager::localIp() {
    return WiFi.isConnected() ? WiFi.localIP().toString() : String();
}

}  // namespace mclite
