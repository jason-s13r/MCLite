#pragma once

#include <lvgl.h>
#include <Arduino.h>

namespace mclite {

class DeviceSettingsScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();

    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen   = nullptr;
    lv_obj_t* _content  = nullptr;

    // Boot text modal
    lv_obj_t* _bootTextModal    = nullptr;
    lv_obj_t* _bootTextTextarea = nullptr;

    void showBootTextModal(const String& current);
    void hideBootTextModal();

    static void backBtnCb(lv_event_t* e);
    static void brightnessChangedCb(lv_event_t* e);
    static void kbdBacklightChangedCb(lv_event_t* e);
    static void bootTextRowCb(lv_event_t* e);
    static void bootTextConfirmCb(lv_event_t* e);
    static void bootTextCancelCb(lv_event_t* e);
};

}  // namespace mclite
