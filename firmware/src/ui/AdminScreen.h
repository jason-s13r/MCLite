#pragma once

#include <lvgl.h>
#include <Arduino.h>

namespace mclite {

// AdminScreen — device info + shortcuts to sub-screens
class AdminScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();
    void tick();  // refresh dynamic counts (heard-adverts) while screen is visible

    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen   = nullptr;
    lv_obj_t* _content  = nullptr;

    // Dynamic-count row state. Reset every show() since the row is rebuilt.
    lv_obj_t* _heardCountLabel = nullptr;
    uint32_t  _heardCacheVersion = 0;

    static void backBtnCb(lv_event_t* e);
    static void navRadioGpsCb(lv_event_t* e);
    static void navDisplaySoundBatteryCb(lv_event_t* e);
    static void navMessagingContactsChannelsRoomsCb(lv_event_t* e);
};

}  // namespace mclite
