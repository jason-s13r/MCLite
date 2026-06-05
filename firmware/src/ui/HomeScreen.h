#pragma once

#include <lvgl.h>
#include <functional>

namespace mclite {

class HomeScreen {
public:
    void create(lv_obj_t* parent);
    void show();
    void hide();

    void onChat(std::function<void()> cb) { _onChat = cb; }
    void onMap(std::function<void()> cb)  { _onMap = cb; }

    lv_obj_t* obj() { return _screen; }

private:
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _grid   = nullptr;

    std::function<void()> _onChat;
    std::function<void()> _onMap;

    static void chatBtnCb(lv_event_t* e);
    static void mapBtnCb(lv_event_t* e);
};

}  // namespace mclite
