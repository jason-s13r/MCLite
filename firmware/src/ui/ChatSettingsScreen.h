#pragma once

#include <lvgl.h>
#include <Arduino.h>

namespace mclite {

class ChatSettingsScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();

    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen   = nullptr;
    lv_obj_t* _content  = nullptr;

    // Add-channel modal state
    lv_obj_t* _addChannelModal   = nullptr;
    lv_obj_t* _addChannelTextarea = nullptr;
    lv_obj_t* _swSendSos        = nullptr;
    lv_obj_t* _swRecvSos        = nullptr;
    lv_obj_t* _swReadOnly       = nullptr;
#ifdef PLATFORM_TWATCH
    lv_obj_t* _addChannelKbd     = nullptr;
#endif

    void showAddChannelModal();
    void hideAddChannelModal();

    static void backBtnCb(lv_event_t* e);
    static void addChannelBtnCb(lv_event_t* e);
    static void addChannelConfirmCb(lv_event_t* e);
    static void addChannelCancelCb(lv_event_t* e);
    static void channelDeleteCb(lv_event_t* e);
    static void allowMuteToggleCb(lv_event_t* e);
};

}  // namespace mclite
