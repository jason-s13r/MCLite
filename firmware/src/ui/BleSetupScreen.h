#pragma once

#include <lvgl.h>

namespace mclite {

// Dedicated screen for BLE companion mode: a switch, the pairing PIN (shown so the
// phone can enter it), and live status. Bridges the radio to the official MeshCore
// mobile apps over BLE (mutually exclusive with WiFi/USB companion). Reached from
// the admin screen.
class BleSetupScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();
    void tick();   // live-refresh when a client attaches/detaches
    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen      = nullptr;
    lv_obj_t* _switch      = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    lv_obj_t* _pinLabel    = nullptr;
    lv_obj_t* _backBtn     = nullptr;
    bool _lastClient = false;

    void updateUi();
    static void switchCb(lv_event_t* e);
    static void backBtnCb(lv_event_t* e);
};

}  // namespace mclite
