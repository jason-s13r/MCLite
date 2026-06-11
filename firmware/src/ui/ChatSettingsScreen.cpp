#include "ChatSettingsScreen.h"
#include <Arduino.h>
#include <time.h>
#include "UIManager.h"
#include "theme.h"
#include "../config/ConfigManager.h"
#include "../hal/Display.h"
#include "../mesh/ContactStore.h"
#include "../mesh/ChannelStore.h"
#include "../mesh/MeshManager.h"
#include "../storage/MessageStore.h"
#include "../i18n/I18n.h"

namespace mclite {

void ChatSettingsScreen::create(lv_obj_t* parent) {
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
    lv_obj_t* title = lv_win_add_title(_screen, t("chat_settings_screen_title"));
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

void ChatSettingsScreen::show() {
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

    // --- Messaging ---
    addSection(t("sec_messaging"));
    addRow("History", cfg.messaging.saveHistory ? t("enabled") : t("disabled"));
    addRow("Max Per Chat", String(cfg.messaging.maxHistoryPerChat));
    addRow("Max Retries", String(cfg.messaging.maxRetries));
    addRow("Req. Telemetry", cfg.messaging.requestTelemetry ? t("enabled") : t("disabled"));
    addRow("Telemetry Badges", cfg.messaging.showTelemetry);

    // Allow Mute switch
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
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(lbl, t("allow_mute"));

        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_set_size(sw, 40, 20);
        if (cfg.messaging.allowMute) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }

        lv_obj_add_event_cb(sw, allowMuteToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    // --- Contacts ---
    auto& contacts = ContactStore::instance();
    char secContactsBuf[32];
    snprintf(secContactsBuf, sizeof(secContactsBuf), t("sec_contacts"), (int)contacts.count());
    addSection(secContactsBuf);
    for (size_t i = 0; i < contacts.count(); i++) {
        const Contact* c = contacts.findByIndex(i);
        if (!c) continue;
        String info = c->name;
        if (c->sendSos) info += " [SOS]";
        if (c->allowTelemetry && c->allowLocation) info += " [GPS]";
        addRow(("  " + String((int)(i + 1))).c_str(), info);
    }

    // --- Channels ---
    auto& channels = ChannelStore::instance();
    char secChannelsBuf[32];
    snprintf(secChannelsBuf, sizeof(secChannelsBuf), t("sec_channels"), (int)channels.count());
    addSection(secChannelsBuf);

    for (const auto& ch : channels.all()) {
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
        lv_obj_set_style_bg_color(row, theme::ACCENT, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_40, LV_STATE_FOCUSED);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
        const char* prefix = ch.isPrivate() ? "  *" : "  #";
        String info = ch.name;
        if (ch.readOnly) info += " [read-only]";
        if (ch.sendSos) info += " [SOS]";
        if (ch.scope.length() > 0) info += " [scope:" + ch.scope + "]";
        lv_label_set_text(lbl, (String(prefix) + " " + info).c_str());
        lv_obj_set_flex_grow(lbl, 1);

        if (ch.custom) {
            lv_obj_t* trashBtn = lv_btn_create(row);
            lv_obj_set_style_bg_opa(trashBtn, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(trashBtn, 0, 0);
            lv_obj_set_style_shadow_width(trashBtn, 0, 0);
            lv_obj_set_style_pad_all(trashBtn, 0, 0);
            lv_obj_add_event_cb(trashBtn, channelDeleteCb, LV_EVENT_CLICKED, this);

            lv_obj_t* trashIcon = lv_label_create(trashBtn);
            lv_obj_set_style_text_font(trashIcon, FONT_BODY, 0);
            lv_obj_set_style_text_color(trashIcon, theme::BATTERY_LOW, 0);
            lv_label_set_text(trashIcon, LV_SYMBOL_TRASH);
            lv_obj_center(trashIcon);

            String* nameCopy = new String(ch.name);
            lv_obj_set_user_data(trashBtn, nameCopy);
            lv_obj_add_event_cb(trashBtn, [](lv_event_t* e) {
                String* name = (String*)lv_obj_get_user_data(lv_event_get_target(e));
                delete name;
            }, LV_EVENT_DELETE, nullptr);
        }
    }

    // "Add channel" button row
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
        lv_obj_set_style_bg_color(row, theme::ACCENT, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_40, LV_STATE_FOCUSED);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::ACCENT, 0);
        lv_label_set_text(lbl, (String(LV_SYMBOL_PLUS " ") + t("add_channel_btn")).c_str());

        lv_obj_add_event_cb(row, addChannelBtnCb, LV_EVENT_CLICKED, this);
    }

    // --- Rooms ---
    {
        const auto& rooms = ConfigManager::instance().config().roomServers;
        char secRoomsBuf[32];
        snprintf(secRoomsBuf, sizeof(secRoomsBuf), t("sec_rooms"), (int)rooms.size());
        addSection(secRoomsBuf);
        auto& store = MessageStore::instance();
        for (size_t i = 0; i < rooms.size(); i++) {
            String info = rooms[i].name;
            info += UIManager::instance().isRoomLoggedIn(i) ? " [online]" : " [offline]";
            if (rooms[i].publicKey.length() == 64) {
                String shortId = rooms[i].publicKey.substring(0, 16);
                ConvoId rid { ConvoId::ROOM, shortId };
                if (Conversation* convo = store.getConversation(rid)) {
                    if (convo->syncSince > 0) {
                        char ts[24];
                        time_t t = (time_t)convo->syncSince;
                        struct tm* tm_info = gmtime(&t);
                        if (tm_info) {
                            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", tm_info);
                            info += " ";
                            info += ts;
                        }
                    }
                }
            }
            addRow("  R", info);
        }
    }

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _content);
        lv_group_focus_obj(_content);
        lv_group_set_editing(grp, true);
    }

    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void ChatSettingsScreen::hide() {
    if (_screen) {
        hideAddChannelModal();
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_remove_obj(_content);
        }
        lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void ChatSettingsScreen::backBtnCb(lv_event_t* e) {
    UIManager::instance().popScreen();
}

void ChatSettingsScreen::allowMuteToggleCb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    auto& mgr = ConfigManager::instance();
    mgr.config().messaging.allowMute = enabled;
    mgr.save();
    UIManager::instance().showToast(enabled ? t("enabled") : t("disabled"));
}

// ---- Add channel modal ----

void ChatSettingsScreen::showAddChannelModal() {
    if (_addChannelModal) hideAddChannelModal();

    _addChannelModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_addChannelModal, Display::width(), Display::height());
    lv_obj_set_pos(_addChannelModal, 0, 0);
    lv_obj_set_style_bg_color(_addChannelModal, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_addChannelModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_addChannelModal, 0, 0);
    lv_obj_set_style_radius(_addChannelModal, 0, 0);
    lv_obj_set_style_pad_all(_addChannelModal, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_addChannelModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_addChannelModal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_addChannelModal, theme::PAD_MEDIUM, 0);
    lv_obj_clear_flag(_addChannelModal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_addChannelModal);
    lv_obj_set_style_text_font(title, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(title, t("add_channel_title"));

    _addChannelTextarea = lv_textarea_create(_addChannelModal);
    lv_obj_set_width(_addChannelTextarea, LV_PCT(80));
    lv_obj_set_height(_addChannelTextarea, 40);
    lv_textarea_set_one_line(_addChannelTextarea, true);
    lv_textarea_set_max_length(_addChannelTextarea, 32);
    lv_textarea_set_placeholder_text(_addChannelTextarea, t("add_channel_hint"));
    lv_obj_set_style_text_font(_addChannelTextarea, FONT_BODY, 0);
    lv_obj_set_style_text_color(_addChannelTextarea, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(_addChannelTextarea, theme::BG_SECONDARY, 0);
    lv_obj_set_style_border_color(_addChannelTextarea, theme::ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(_addChannelTextarea, 1, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(_addChannelTextarea, addChannelConfirmCb, LV_EVENT_READY, this);

    // Switch row: Send SOS
    {
        lv_obj_t* row = lv_obj_create(_addChannelModal);
        lv_obj_set_size(row, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, t("channel_send_sos"));
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);

        _swSendSos = lv_switch_create(row);
        lv_obj_set_size(_swSendSos, 40, 20);
    }

    // Switch row: Receive SOS
    {
        lv_obj_t* row = lv_obj_create(_addChannelModal);
        lv_obj_set_size(row, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, t("channel_recv_sos"));
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);

        _swRecvSos = lv_switch_create(row);
        lv_obj_set_size(_swRecvSos, 40, 20);
    }

    // Switch row: Read only
    {
        lv_obj_t* row = lv_obj_create(_addChannelModal);
        lv_obj_set_size(row, LV_PCT(80), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, t("channel_read_only"));
        lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);

        _swReadOnly = lv_switch_create(row);
        lv_obj_set_size(_swReadOnly, 40, 20);
    }

#ifdef PLATFORM_TWATCH
    _addChannelKbd = lv_keyboard_create(_addChannelModal);
    lv_obj_set_size(_addChannelKbd, Display::width(), 200);
    lv_obj_align(_addChannelKbd, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(_addChannelKbd, _addChannelTextarea);
    lv_keyboard_set_popovers(_addChannelKbd, true);
    lv_btnmatrix_set_btn_ctrl_all(_addChannelKbd, LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_obj_set_style_text_font(_addChannelKbd, FONT_BODY, LV_PART_ITEMS);
    lv_obj_set_style_text_font(_addChannelKbd, FONT_TITLE, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(_addChannelKbd, theme::ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(_addChannelKbd, lv_color_white(), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_radius(_addChannelKbd, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(_addChannelKbd, 2, 0);
    lv_obj_set_style_pad_gap(_addChannelKbd, 2, 0);
#endif

    // Button row: Cancel + Add
    lv_obj_t* btnRow = lv_obj_create(_addChannelModal);
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
    lv_obj_add_event_cb(cancelBtn, addChannelCancelCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, t("btn_cancel"));
    lv_obj_set_style_text_font(cancelLbl, FONT_BODY, 0);
    lv_obj_center(cancelLbl);

    lv_obj_t* addBtn = lv_btn_create(btnRow);
    lv_obj_set_size(addBtn, 80, 36);
    lv_obj_set_style_bg_color(addBtn, theme::ACCENT, 0);
    lv_obj_set_style_radius(addBtn, 4, 0);
    lv_obj_add_event_cb(addBtn, addChannelConfirmCb, LV_EVENT_CLICKED, this);
    lv_obj_t* addLbl = lv_label_create(addBtn);
    lv_label_set_text(addLbl, t("add_channel_btn"));
    lv_obj_set_style_text_font(addLbl, FONT_BODY, 0);
    lv_obj_center(addLbl);

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _addChannelTextarea);
        lv_group_focus_obj(_addChannelTextarea);
        lv_group_set_editing(grp, true);
    }
}

void ChatSettingsScreen::hideAddChannelModal() {
    if (!_addChannelModal) return;

    if (_addChannelTextarea) lv_group_remove_obj(_addChannelTextarea);
#ifdef PLATFORM_TWATCH
    if (_addChannelKbd) lv_group_remove_obj(_addChannelKbd);
#endif

    lv_obj_del(_addChannelModal);
    _addChannelModal = nullptr;
    _addChannelTextarea = nullptr;
    _swSendSos = nullptr;
    _swRecvSos = nullptr;
    _swReadOnly = nullptr;
#ifdef PLATFORM_TWATCH
    _addChannelKbd = nullptr;
#endif
}

void ChatSettingsScreen::addChannelBtnCb(lv_event_t* e) {
    ChatSettingsScreen* self = static_cast<ChatSettingsScreen*>(lv_event_get_user_data(e));
    if (self) self->showAddChannelModal();
}

void ChatSettingsScreen::addChannelConfirmCb(lv_event_t* e) {
    ChatSettingsScreen* self = static_cast<ChatSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_addChannelTextarea) return;

    const char* raw = lv_textarea_get_text(self->_addChannelTextarea);
    String name(raw);
    name.trim();

    if (name.length() == 0 || name[0] != '#') {
        UIManager::instance().showToast(t("channel_invalid"));
        return;
    }

    bool sendSos  = self->_swSendSos  ? lv_obj_has_state(self->_swSendSos,  LV_STATE_CHECKED) : false;
    bool recvSos  = self->_swRecvSos  ? lv_obj_has_state(self->_swRecvSos,  LV_STATE_CHECKED) : false;
    bool readOnly = self->_swReadOnly ? lv_obj_has_state(self->_swReadOnly, LV_STATE_CHECKED) : false;

    Channel* ch = ChannelStore::instance().deriveHashtagChannel(name, recvSos, sendSos, readOnly);
    if (!ch) {
        UIManager::instance().showToast(t("channel_exists"));
        return;
    }

    if (!ChannelStore::instance().saveCustomChannels()) {
        ChannelStore::instance().removeChannelByName(ch->name);
        UIManager::instance().showToast(t("channel_save_fail"));
        return;
    }

    ConvoId cid{ConvoId::CHANNEL, ch->name};
    MessageStore::instance().ensureConversation(cid, ch->name, ch->isPrivate(), ch->readOnly);
    MessageStore::instance().loadHistory(cid);

    int meshIdx = MeshManager::instance().addChannel(ch->name.c_str(), ch->pskB64.c_str());
    if (meshIdx < 0) {
        UIManager::instance().showToast(t("channel_save_fail"));
        return;
    }

    UIManager::instance().showToast(t("channel_added"));
    self->hideAddChannelModal();
    self->show();
}

void ChatSettingsScreen::addChannelCancelCb(lv_event_t* e) {
    ChatSettingsScreen* self = static_cast<ChatSettingsScreen*>(lv_event_get_user_data(e));
    if (self) self->hideAddChannelModal();
}

void ChatSettingsScreen::channelDeleteCb(lv_event_t* e) {
    ChatSettingsScreen* self = static_cast<ChatSettingsScreen*>(lv_event_get_user_data(e));
    String* name = (String*)lv_obj_get_user_data(lv_event_get_target(e));
    if (!self || !name) return;

    static char bodyBuf[128];
    snprintf(bodyBuf, sizeof(bodyBuf), t("confirm_delete_body"), name->c_str());
    static const char* btns[3];
    btns[0] = t("btn_cancel");
    btns[1] = t("btn_delete");
    btns[2] = "";

    lv_obj_t* msgbox = lv_msgbox_create(NULL, t("confirm_delete_title"), bodyBuf, btns, false);
    lv_obj_center(msgbox);
    lv_obj_set_style_bg_color(msgbox, theme::BG_SECONDARY, 0);
    lv_obj_set_style_text_color(msgbox, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(msgbox, FONT_HEADING, 0);

    lv_obj_t* btnm = lv_msgbox_get_btns(msgbox);
    if (btnm) UIManager::instance().switchToModalGroup(btnm);

    struct DeleteCtx {
        String name;
        ChatSettingsScreen* screen;
    };
    DeleteCtx* ctx = new DeleteCtx{*name, self};
    lv_obj_set_user_data(msgbox, ctx);
    lv_obj_add_event_cb(msgbox, [](lv_event_t* ev) {
        DeleteCtx* c = (DeleteCtx*)lv_obj_get_user_data(lv_event_get_current_target(ev));
        lv_obj_t* mbox = lv_event_get_current_target(ev);
        uint16_t btnIdx = lv_msgbox_get_active_btn(mbox);
        if (btnIdx == 1 && c) {
            ChannelStore::instance().removeChannelByName(c->name);
            ChannelStore::instance().saveCustomChannels();
            ConvoId cid{ConvoId::CHANNEL, c->name};
            MessageStore::instance().removeConversation(cid);
            UIManager::instance().showToast(t("channel_deleted"));
            if (c->screen) c->screen->show();
        }
        UIManager::instance().restoreFromModalGroup();
        lv_msgbox_close(mbox);
        delete c;
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

}  // namespace mclite
