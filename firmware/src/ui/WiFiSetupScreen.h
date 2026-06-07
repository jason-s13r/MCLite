#pragma once

#include <lvgl.h>
#include "../net/WiFiManager.h"

namespace mclite {

// On-device WiFi setup. A "WiFi" switch connects to the saved/selected network
// and STAYS connected (honest status + a status-bar icon); the scan list lets
// you pick/change networks; a "Check for updates" button (shown only while
// connected) runs the GitHub check on demand. Modeled on HeardAdvertsScreen.
class WiFiSetupScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();
    void tick();   // live-refresh status when connection state changes (call from UIManager::update)
    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen      = nullptr;
    lv_obj_t* _switch      = nullptr;   // WiFi on/off
    lv_obj_t* _statusLabel = nullptr;   // Off / Connecting… / Connected: SSID  IP
    lv_obj_t* _checkBtn    = nullptr;   // "Check for updates" (visible only when connected)
    lv_obj_t* _list        = nullptr;   // scanned networks (shown when not connected)
    lv_obj_t* _closeBtn    = nullptr;

    // Password-entry overlay
    lv_obj_t*   _pwOverlay  = nullptr;
    lv_obj_t*   _pwTextarea = nullptr;
    lv_group_t* _pwGroup    = nullptr;
#ifdef PLATFORM_TWATCH
    lv_obj_t*   _pwKbd      = nullptr;
#endif
    String _selSsid;
    String _selPassword;
    bool   _lastConnected = false;   // for tick() change detection

    static constexpr int MAX_NETS = 20;
    ScannedNetwork _nets[MAX_NETS];
    int _netCount = 0;

    void updateStatusUi();              // sync switch/status/check-button/list to real state
    void rebuildList();                 // scan + populate (only when disconnected)
    void clearList();
    void openPasswordEntry(const String& ssid);
    void closePasswordEntry();
    void doConnect(const String& ssid, const String& password);  // connect + stay
    void doDisconnect();
    void checkUpdatesNow();

    static void closeBtnCb(lv_event_t* e);
    static void switchCb(lv_event_t* e);
    static void checkBtnCb(lv_event_t* e);
    static void rowClickCb(lv_event_t* e);
    static void pwReadyCb(lv_event_t* e);    // Enter / keyboard OK
    static void connectAsyncCb(void* user);  // deferred connect (outside the event)
    static void closePwAsyncCb(void* user);  // deferred password-entry close
    static void checkAsyncCb(void* user);    // deferred update check
};

}  // namespace mclite
