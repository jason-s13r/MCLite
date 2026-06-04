#pragma once

#include <lvgl.h>
#include <functional>
#include <memory>
#include "../storage/MessageStore.h"

namespace mclite {

using OnSendCallback  = std::function<void(const ConvoId& id, const String& text)>;
using OnBackCallback  = std::function<void()>;
using OnInfoCallback  = std::function<void(const ConvoId& id)>;
using OnRetryCallback = std::function<void(const ConvoId& id, const String& text, uint32_t oldPacketId)>;
using OnMuteCallback  = std::function<void(const ConvoId& id, bool muted)>;
using OnRefreshCallback = std::function<void(const ConvoId& id)>;
using OnMapCallback   = std::function<void(const ConvoId& id)>;

class ChatScreen {
public:
    void create(lv_obj_t* parent);
    void open(const ConvoId& id);
    void close();
    void show();
    void hide();

    void addMessageToView(const Message& msg);
    void refresh();  // Reload from MessageStore
    void refreshMapButtonVisibility(const String& shortId);

    void onSend(OnSendCallback cb)   { _onSend = cb; }
    void onBack(OnBackCallback cb)   { _onBack = cb; }
    void onInfo(OnInfoCallback cb)   { _onInfo = cb; }
    void onRetry(OnRetryCallback cb) { _onRetry = cb; }
    void onMute(OnMuteCallback cb)  { _onMute = cb; }
    void onRefresh(OnRefreshCallback cb) { _onRefresh = cb; }
    void onMap(OnMapCallback cb)     { _onMap = cb; }

    const ConvoId* currentConvo() const { return _currentConvo.get(); }
    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen    = nullptr;
    lv_obj_t* _header    = nullptr;
    lv_obj_t* _chatArea  = nullptr;
    lv_obj_t* _inputBar  = nullptr;
    lv_obj_t* _textarea  = nullptr;
    lv_obj_t* _sendBtn   = nullptr;
    lv_obj_t* _gpsBtn    = nullptr;
    lv_obj_t* _cannedBtn     = nullptr;
    lv_obj_t* _cannedBtnm    = nullptr;  // btnmatrix picker overlay
    lv_obj_t* _cannedOverlay = nullptr;
    lv_obj_t* _emojiBtn      = nullptr;
    lv_obj_t* _emojiBtnm     = nullptr;  // emoji grid picker overlay
    lv_obj_t* _emojiOverlay  = nullptr;
    lv_obj_t* _headerName = nullptr;
    lv_obj_t* _muteIcon = nullptr;  // Mute indicator in header
    lv_obj_t* _refreshBtn = nullptr;  // Refresh telemetry button (DMs only)
    lv_obj_t* _mapBtn = nullptr;      // Open map button (DMs only)
    lv_obj_t* _infoBtn = nullptr;   // Info button in header (DMs only)
#ifdef PLATFORM_TWATCH
    lv_obj_t* _kbd        = nullptr;  // T-Watch only: on-screen keyboard
#endif

    std::unique_ptr<ConvoId> _currentConvo;

    OnSendCallback  _onSend;
    OnBackCallback  _onBack;
    OnInfoCallback  _onInfo;
    OnRetryCallback _onRetry;
    OnMuteCallback  _onMute;
    OnRefreshCallback _onRefresh;
    OnMapCallback   _onMap;

    void createHeader();
    void createChatArea();
    void createInputBar();
    void updateGpsButtonColor();
    void updateMapButtonVisibility(const String& shortId);
    void showCannedPicker();
    void showEmojiPicker();
#ifdef PLATFORM_TWATCH
    void showKeyboard();
    void hideKeyboard();
#endif
    void hideCannedPicker();
    void hideEmojiPicker();
    void updateMuteIndicator();

    void addBubble(const Message& msg);
    void scrollToBottom();

    static void sendBtnCb(lv_event_t* e);
    static void gpsBtnCb(lv_event_t* e);
    static void backBtnCb(lv_event_t* e);
    static void textareaCb(lv_event_t* e);
    static void infoBtnCb(lv_event_t* e);
    static void refreshBtnCb(lv_event_t* e);
    static void mapBtnCb(lv_event_t* e);
    static void retryBtnCb(lv_event_t* e);
    static void senderNameCb(lv_event_t* e);
    static void cannedBtnCb(lv_event_t* e);
    static void cannedBtnmCb(lv_event_t* e);
    static void emojiBtnCb(lv_event_t* e);
    static void emojiBtnmCb(lv_event_t* e);
    static void muteIconCb(lv_event_t* e);
};

}  // namespace mclite
