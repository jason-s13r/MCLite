#include "HomeScreen.h"
#include "theme.h"
#include "../hal/Display.h"
#include "../i18n/I18n.h"

namespace mclite {

void HomeScreen::create(lv_obj_t* parent) {
    _screen = lv_obj_create(parent);
    lv_obj_set_size(_screen, Display::width(),
                    Display::height() - theme::STATUS_BAR_HEIGHT - theme::FOOTER_HEIGHT);
    lv_obj_align(_screen, LV_ALIGN_BOTTOM_MID, 0, -theme::FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(_screen, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_radius(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, theme::PAD_LARGE, 0);
    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Grid container: 2 columns, centered
    _grid = lv_obj_create(_screen);
    lv_obj_set_size(_grid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(_grid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_grid, 0, 0);
    lv_obj_set_style_pad_all(_grid, 0, 0);
    lv_obj_set_style_pad_row(_grid, theme::PAD_LARGE, 0);
    lv_obj_set_style_pad_column(_grid, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_grid, LV_OBJ_FLAG_SCROLLABLE);

    auto addBtn = [this](const char* label, const char* icon,
                         lv_event_cb_t cb) -> lv_obj_t* {
        lv_obj_t* btn = lv_btn_create(_grid);
#ifdef PLATFORM_TWATCH
        lv_obj_set_size(btn, 120, 120);
        lv_obj_set_style_radius(btn, 16, 0);
#else
        lv_obj_set_size(btn, 80, 80);
        lv_obj_set_style_radius(btn, 8, 0);
#endif
        lv_obj_set_style_bg_color(btn, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, theme::ACCENT, 0);
        lv_obj_set_style_pad_all(btn, theme::PAD_MEDIUM, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        // Icon
        lv_obj_t* iconLbl = lv_label_create(btn);
        lv_obj_set_style_text_font(iconLbl, FONT_HEADING, 0);
        lv_obj_set_style_text_color(iconLbl, theme::ACCENT, 0);
        lv_label_set_text(iconLbl, icon);

        // Label
        lv_obj_t* textLbl = lv_label_create(btn);
        lv_obj_set_style_text_font(textLbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(textLbl, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(textLbl, label);

        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);

        // Add to input group for trackball/keyboard navigation
        lv_group_t* grp = lv_group_get_default();
        if (grp) lv_group_add_obj(grp, btn);

        return btn;
    };

    addBtn(t("btn_chat"), LV_SYMBOL_LIST, chatBtnCb);
    addBtn(t("btn_map"),  LV_SYMBOL_GPS,  mapBtnCb);

    lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void HomeScreen::show() {
    if (_screen) lv_obj_clear_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void HomeScreen::hide() {
    if (_screen) lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void HomeScreen::chatBtnCb(lv_event_t* e) {
    HomeScreen* self = static_cast<HomeScreen*>(lv_event_get_user_data(e));
    if (self && self->_onChat) self->_onChat();
}

void HomeScreen::mapBtnCb(lv_event_t* e) {
    HomeScreen* self = static_cast<HomeScreen*>(lv_event_get_user_data(e));
    if (self && self->_onMap) self->_onMap();
}

}  // namespace mclite
