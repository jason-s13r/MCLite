#pragma once

#include <lvgl.h>
#include <Arduino.h>

namespace mclite {

// AdminScreen — device info + runtime channel management
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
    static void offgridToggleCb(lv_event_t* e);
    static void addChannelBtnCb(lv_event_t* e);
    static void addChannelConfirmCb(lv_event_t* e);
    static void addChannelCancelCb(lv_event_t* e);
    static void channelDeleteCb(lv_event_t* e);

    // GPS location-advert controls (rebuilt each show())
    static void locationAdvertToggleCb(lv_event_t* e);
    static void locationPrecisionChangedCb(lv_event_t* e);
    static const char* precisionLabel(uint8_t precision);
};

}  // namespace mclite
