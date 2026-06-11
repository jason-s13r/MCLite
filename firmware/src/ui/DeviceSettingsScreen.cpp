#include "DeviceSettingsScreen.h"
#include <Arduino.h>
#include "UIManager.h"
#include "theme.h"
#include "../config/ConfigManager.h"
#include "../hal/Display.h"
#include "../hal/Battery.h"
#include "../hal/Speaker.h"
#include "../hal/IInput.h"
#include "../i18n/I18n.h"

namespace mclite {

void DeviceSettingsScreen::create(lv_obj_t* parent) {
    _screen = lv_win_create(parent, theme::CHAT_HEADER_HEIGHT);
    lv_obj_set_size(_screen, Display::width(),
                    Display::height() - theme::STATUS_BAR_HEIGHT - theme::FOOTER_HEIGHT);
    lv_obj_align(_screen, LV_ALIGN_BOTTOM_MID, 0, -theme::FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(_screen, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_screen, 0, 0);
    lv_obj_set_style_radius(_screen, 0, 0);
    lv_obj_set_style_pad_all(_screen, 0, 0);
    lv_obj_set_style_pad_row(_screen, 0, 0);

#ifdef PLATFORM_TWATCH
    lv_obj_set_style_pad_hor(_screen, theme::SAFE_AREA_LEFT, 0);
    lv_obj_set_style_pad_ver(_screen, 0, 0);
    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(_screen, [](lv_event_t* e) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_RIGHT) UIManager::instance().popScreen();
    }, LV_EVENT_GESTURE, nullptr);
#endif

    // Style the header
    lv_obj_t* header = lv_win_get_header(_screen);
    lv_obj_set_style_bg_color(header, theme::BG_STATUS_BAR, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, theme::PAD_SMALL, 0);
    lv_obj_set_style_pad_hor(header, theme::CHAT_HEADER_PAD_HOR, 0);

    // Back button
    lv_obj_t* backBtn = lv_win_add_btn(_screen, LV_SYMBOL_LEFT, theme::BTN_HEADER_BACK_W);
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_set_style_border_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, backBtnCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* backLbl = lv_obj_get_child(backBtn, 0);
    lv_obj_set_style_text_font(backLbl, FONT_HEADING, 0);
    lv_obj_set_style_text_color(backLbl, theme::ACCENT, 0);

    // Title
    lv_obj_t* title = lv_win_add_title(_screen, t("device_settings_screen_title"));
    lv_obj_set_style_text_font(title, FONT_HEADING, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);

    // Content area styling
    _content = lv_win_get_content(_screen);
    lv_obj_set_style_bg_opa(_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_style_pad_all(_content, theme::PAD_MEDIUM, 0);
    lv_obj_set_flex_flow(_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_content, theme::PAD_SMALL, 0);
    lv_obj_add_flag(_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_content, LV_DIR_VER);

    lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void DeviceSettingsScreen::show() {
    if (!_screen) return;

    uint32_t childCount = lv_obj_get_child_cnt(_content);
    while (childCount > 0) {
        lv_obj_del(lv_obj_get_child(_content, childCount - 1));
        childCount--;
    }

    const auto& cfg = ConfigManager::instance().config();

    auto addRow = [this](const char* label, const String& value) {
        lv_obj_t* row = lv_obj_create(_content);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, theme::PAD_SMALL, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(lbl, label);

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_font(val, FONT_BODY, 0);
        lv_obj_set_style_text_color(val, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(val, value.c_str());
    };

    auto addSection = [this](const char* title) {
        lv_obj_t* lbl = lv_label_create(_content);
        lv_obj_set_style_text_font(lbl, FONT_HEADING, 0);
        lv_obj_set_style_text_color(lbl, theme::ACCENT, 0);
        lv_obj_set_style_pad_top(lbl, theme::PAD_MEDIUM, 0);
        lv_label_set_text(lbl, title);
    };

    // --- Display ---
    addSection(t("sec_display"));

    // Brightness slider
    {
        lv_obj_t* row = lv_obj_create(_content);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, theme::PAD_SMALL, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(row, 2, 0);

        lv_obj_t* labelRow = lv_obj_create(row);
        lv_obj_set_size(labelRow, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(labelRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(labelRow, 0, 0);
        lv_obj_set_style_pad_all(labelRow, 0, 0);
        lv_obj_clear_flag(labelRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(labelRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(labelRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(labelRow);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(lbl, t("device_brightness"));

        lv_obj_t* valLbl = lv_label_create(labelRow);
        lv_obj_set_style_text_font(valLbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(valLbl, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(valLbl, String(cfg.display.brightness).c_str());

        lv_obj_t* slider = lv_slider_create(row);
        lv_obj_set_width(slider, LV_PCT(100));
        lv_slider_set_range(slider, 0, 255);
        lv_slider_set_value(slider, cfg.display.brightness, LV_ANIM_OFF);

        lv_obj_add_event_cb(slider, brightnessChangedCb, LV_EVENT_VALUE_CHANGED, valLbl);
    }

    addRow("Auto-Dim", cfg.display.autoDimSeconds > 0
        ? String(cfg.display.autoDimSeconds) + "s" : String(t("off")));
    addRow("Dim Brightness", cfg.display.dimBrightness > 0
        ? String(cfg.display.dimBrightness) : String(t("off")));

    // Keyboard backlight slider
    {
        lv_obj_t* row = lv_obj_create(_content);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, theme::PAD_SMALL, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(row, 2, 0);

        lv_obj_t* labelRow = lv_obj_create(row);
        lv_obj_set_size(labelRow, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(labelRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(labelRow, 0, 0);
        lv_obj_set_style_pad_all(labelRow, 0, 0);
        lv_obj_clear_flag(labelRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(labelRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(labelRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(labelRow);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(lbl, t("device_kbd_backlight"));

        lv_obj_t* valLbl = lv_label_create(labelRow);
        lv_obj_set_style_text_font(valLbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(valLbl, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(valLbl, cfg.display.kbdBacklight ? String(cfg.display.kbdBrightness).c_str() : t("off"));

        lv_obj_t* slider = lv_slider_create(row);
        lv_obj_set_width(slider, LV_PCT(100));
        lv_slider_set_range(slider, 0, 255);
        lv_slider_set_value(slider, cfg.display.kbdBacklight ? cfg.display.kbdBrightness : 0, LV_ANIM_OFF);

        lv_obj_add_event_cb(slider, kbdBacklightChangedCb, LV_EVENT_VALUE_CHANGED, valLbl);
    }

    // Boot text row (clickable)
    {
        lv_obj_t* row = lv_obj_create(_content);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, theme::BG_SECONDARY, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_all(row, theme::PAD_SMALL, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(lbl, t("device_boot_text"));

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_font(val, FONT_BODY, 0);
        lv_obj_set_style_text_color(val, theme::TEXT_PRIMARY, 0);
        String bt = cfg.display.bootText;
        if (bt.length() == 0) bt = t("off");
        lv_label_set_text(val, bt.c_str());

        lv_obj_add_event_cb(row, bootTextRowCb, LV_EVENT_CLICKED, this);
    }

    // --- Sound ---
    addSection(t("sec_sound"));
    addRow("Sound", Speaker::instance().isMuted() ? t("muted") : t("on"));
    addRow("SOS Keyword", cfg.sosKeyword);
    addRow("SOS Repeat", String(cfg.sosRepeat));

    // --- Battery ---
    addSection(t("sec_battery"));
    addRow("Level", String(Battery::instance().percent()) + "%");
    if (cfg.battery.lowAlertEnabled) {
        addRow("Low Alert", String(cfg.battery.lowAlertThreshold) + "%");
    } else {
        addRow("Low Alert", t("off"));
    }

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _content);
        lv_group_focus_obj(_content);
        lv_group_set_editing(grp, true);
    }

    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void DeviceSettingsScreen::hide() {
    if (_screen) {
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_remove_obj(_content);
        }
        lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void DeviceSettingsScreen::backBtnCb(lv_event_t* e) {
    UIManager::instance().popScreen();
}

void DeviceSettingsScreen::brightnessChangedCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    lv_obj_t* valLbl = (lv_obj_t*)lv_event_get_user_data(e);
    int v = lv_slider_get_value(slider);
    auto& cfg = ConfigManager::instance().config();
    cfg.display.brightness = (uint8_t)v;
    ConfigManager::instance().save();
    Display::instance().setBrightness(cfg.display.brightness);
    if (valLbl) {
        lv_label_set_text(valLbl, String(v).c_str());
    }
}

void DeviceSettingsScreen::kbdBacklightChangedCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    lv_obj_t* valLbl = (lv_obj_t*)lv_event_get_user_data(e);
    int v = lv_slider_get_value(slider);
    auto& cfg = ConfigManager::instance().config();
    cfg.display.kbdBacklight = (v > 0);
    cfg.display.kbdBrightness = (uint8_t)constrain(v, 1, 255);
    ConfigManager::instance().save();
    IInput::instance().setBacklight(cfg.display.kbdBacklight ? cfg.display.kbdBrightness : 0);
    if (valLbl) {
        lv_label_set_text(valLbl, v > 0 ? String(v).c_str() : t("off"));
    }
}

void DeviceSettingsScreen::showBootTextModal(const String& current) {
    if (_bootTextModal) hideBootTextModal();

    _bootTextModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_bootTextModal, Display::width(), Display::height());
    lv_obj_set_pos(_bootTextModal, 0, 0);
    lv_obj_set_style_bg_color(_bootTextModal, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_bootTextModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_bootTextModal, 0, 0);
    lv_obj_set_style_radius(_bootTextModal, 0, 0);
    lv_obj_set_style_pad_all(_bootTextModal, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_bootTextModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_bootTextModal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_bootTextModal, theme::PAD_MEDIUM, 0);
    lv_obj_clear_flag(_bootTextModal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_bootTextModal);
    lv_obj_set_style_text_font(title, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(title, t("device_edit_boot_text"));

    _bootTextTextarea = lv_textarea_create(_bootTextModal);
    lv_obj_set_width(_bootTextTextarea, LV_PCT(80));
    lv_obj_set_height(_bootTextTextarea, 40);
    lv_textarea_set_one_line(_bootTextTextarea, true);
    lv_textarea_set_max_length(_bootTextTextarea, 64);
    lv_textarea_set_text(_bootTextTextarea, current.c_str());
    lv_obj_set_style_text_font(_bootTextTextarea, FONT_BODY, 0);
    lv_obj_set_style_text_color(_bootTextTextarea, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(_bootTextTextarea, theme::BG_SECONDARY, 0);
    lv_obj_set_style_border_color(_bootTextTextarea, theme::ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(_bootTextTextarea, 1, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(_bootTextTextarea, bootTextConfirmCb, LV_EVENT_READY, this);

    lv_obj_t* btnRow = lv_obj_create(_bootTextModal);
    lv_obj_set_size(btnRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btnRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btnRow, 0, 0);
    lv_obj_set_style_pad_all(btnRow, 0, 0);
    lv_obj_clear_flag(btnRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btnRow, theme::PAD_MEDIUM, 0);

    lv_obj_t* cancelBtn = lv_btn_create(btnRow);
    lv_obj_set_size(cancelBtn, 80, 36);
    lv_obj_set_style_bg_color(cancelBtn, theme::BG_SECONDARY, 0);
    lv_obj_set_style_radius(cancelBtn, 4, 0);
    lv_obj_add_event_cb(cancelBtn, bootTextCancelCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, t("btn_cancel"));
    lv_obj_set_style_text_font(cancelLbl, FONT_BODY, 0);
    lv_obj_center(cancelLbl);

    lv_obj_t* saveBtn = lv_btn_create(btnRow);
    lv_obj_set_size(saveBtn, 80, 36);
    lv_obj_set_style_bg_color(saveBtn, theme::ACCENT, 0);
    lv_obj_set_style_radius(saveBtn, 4, 0);
    lv_obj_add_event_cb(saveBtn, bootTextConfirmCb, LV_EVENT_CLICKED, this);
    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, t("btn_save"));
    lv_obj_set_style_text_font(saveLbl, FONT_BODY, 0);
    lv_obj_center(saveLbl);

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _bootTextTextarea);
        lv_group_focus_obj(_bootTextTextarea);
        lv_group_set_editing(grp, true);
    }
}

void DeviceSettingsScreen::hideBootTextModal() {
    if (!_bootTextModal) return;
    if (_bootTextTextarea) {
        lv_group_t* grp = lv_group_get_default();
        if (grp) lv_group_remove_obj(_bootTextTextarea);
    }
    lv_obj_del(_bootTextModal);
    _bootTextModal = nullptr;
    _bootTextTextarea = nullptr;
}

void DeviceSettingsScreen::bootTextRowCb(lv_event_t* e) {
    DeviceSettingsScreen* self = static_cast<DeviceSettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    const auto& cfg = ConfigManager::instance().config();
    self->showBootTextModal(cfg.display.bootText);
}

void DeviceSettingsScreen::bootTextConfirmCb(lv_event_t* e) {
    DeviceSettingsScreen* self = static_cast<DeviceSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_bootTextTextarea) return;

    const char* raw = lv_textarea_get_text(self->_bootTextTextarea);
    String val(raw);
    val.trim();

    auto& cfg = ConfigManager::instance().config();
    cfg.display.bootText = val;
    ConfigManager::instance().save();
    Display::instance().setBootText(val.c_str());

    self->hideBootTextModal();
    self->show();
}

void DeviceSettingsScreen::bootTextCancelCb(lv_event_t* e) {
    DeviceSettingsScreen* self = static_cast<DeviceSettingsScreen*>(lv_event_get_user_data(e));
    if (self) self->hideBootTextModal();
}

}  // namespace mclite
