#include "RadioGpsScreen.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "UIManager.h"
#include "theme.h"
#include "../config/ConfigManager.h"
#include "../hal/Display.h"
#include "../hal/GPS.h"
#include "../mesh/MeshManager.h"
#include "../config/defaults.h"
#include "../i18n/I18n.h"
#include "../util/TimeHelper.h"
#include "../util/offgrid.h"
#include "../storage/SDCard.h"

namespace mclite {

void RadioGpsScreen::create(lv_obj_t* parent) {
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
    lv_obj_t* title = lv_win_add_title(_screen, t("radio_gps_title"));
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

void RadioGpsScreen::show() {
    if (!_screen) return;

    // Clear old content
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

    // --- Radio ---
    addSection(t("sec_radio"));

    // Off-grid Repeater Mode toggle
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
        lv_label_set_text(lbl, t("offgrid_repeater_mode"));

        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_set_size(sw, 40, 20);
        if (cfg.offgrid.enabled) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }

        lv_obj_add_event_cb(sw, offgridToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    // Presets button
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
        lv_label_set_text(lbl, t("radio_presets"));

        lv_obj_t* chevron = lv_label_create(row);
        lv_obj_set_style_text_font(chevron, FONT_BODY, 0);
        lv_obj_set_style_text_color(chevron, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(chevron, LV_SYMBOL_RIGHT);

        lv_obj_add_event_cb(row, presetBtnCb, LV_EVENT_CLICKED, this);
    }

    auto addEditableRow = [this](const char* label, const String& value, const char* field) {
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
        lv_label_set_text(lbl, label);

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_font(val, FONT_BODY, 0);
        lv_obj_set_style_text_color(val, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(val, value.c_str());

        EditRowCtx* ctx = new EditRowCtx{label, field, value};
        lv_obj_set_user_data(row, ctx);
        lv_obj_add_event_cb(row, editRowCb, LV_EVENT_CLICKED, this);
    };

    {
        float activeFreq = cfg.radio.frequency;
        String freqSuffix = " MHz";
        if (cfg.offgrid.enabled) {
            activeFreq = mclite::offgridFreqFor(cfg.radio.frequency);
            freqSuffix += " (offgrid)";
        }
        addEditableRow(t("radio_edit_freq"), String(activeFreq, 3) + freqSuffix, "freq");
    }
    addEditableRow(t("radio_edit_sf"), String(cfg.radio.spreadingFactor), "sf");
    addEditableRow(t("radio_edit_bw"), String(cfg.radio.bandwidth, 1), "bw");
    addEditableRow(t("radio_edit_cr"), String(cfg.radio.codingRate), "cr");
    addEditableRow(t("radio_edit_txp"), String(cfg.radio.txPower), "txp");
    addEditableRow(t("radio_edit_scope"), cfg.radio.scope, "scope");
    {
        char phBuf[16];
        snprintf(phBuf, sizeof(phBuf), "%u B/hop", (unsigned)(cfg.radio.pathHashMode + 1));
        String phLabel = phBuf;
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
        lv_label_set_text(lbl, t("radio_edit_path"));

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_font(val, FONT_BODY, 0);
        lv_obj_set_style_text_color(val, theme::TEXT_PRIMARY, 0);
        lv_label_set_text(val, phLabel.c_str());

        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
            if (self) self->showPathModal();
        }, LV_EVENT_CLICKED, this);
    }
    addRow("Status", MeshManager::instance().isRadioReady() ? t("ready") : t("error"));

    if (MeshManager::instance().isRadioReady()) {
        float duty = MeshManager::instance().getTxDutyCyclePercent();
        char utilBuf[32];
        if (MeshManager::instance().isEURegion()) {
            snprintf(utilBuf, sizeof(utilBuf), "%.2f%% (10%% limit)", duty);
        } else {
            snprintf(utilBuf, sizeof(utilBuf), "%.2f%%", duty);
        }
        addRow(t("ch_util"), utilBuf);
    }

    // --- GPS ---
    addSection(t("sec_gps"));

    // GPS enable/disable toggle
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
        lv_label_set_text(lbl, "GPS");

        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_set_size(sw, 40, 20);
        if (cfg.gpsEnabled) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }

        lv_obj_add_event_cb(sw, gpsToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);
    }

    if (cfg.gpsEnabled) {
        auto& gps = GPS::instance();
        FixStatus fs = gps.fixStatus();
        switch (fs) {
            case FixStatus::LIVE: {
                addRow(t("gps_fix_status"), t("gps_live"));
                addRow(t("gps_coords"), gps.formatLocation());
                addRow(t("gps_satellites"), String(gps.satellites()));
                char hdopBuf[8];
                snprintf(hdopBuf, sizeof(hdopBuf), "%.1f", gps.hdop());
                addRow("HDOP", String(hdopBuf));
                break;
            }
            case FixStatus::LAST_KNOWN: {
                uint32_t age = gps.fixAgeSeconds();
                char ageBuf[32];
                if (age < 60)
                    snprintf(ageBuf, sizeof(ageBuf), t("gps_last_known_s"), (int)age);
                else if (age < 3600)
                    snprintf(ageBuf, sizeof(ageBuf), t("gps_last_known_m"), (int)(age / 60));
                else
                    snprintf(ageBuf, sizeof(ageBuf), t("gps_last_known_h"), (int)(age / 3600));
                addRow(t("gps_fix_status"), String(ageBuf));
                addRow(t("gps_coords"), gps.formatLocation());
                break;
            }
            case FixStatus::NO_FIX:
                addRow(t("gps_fix_status"), t("searching"));
                break;
        }
        addRow(t("gps_coord_format"), cfg.messaging.locationFormat);
        addRow("Last Known Max", String(cfg.gpsLastKnownMaxAge) + "s");
        if (cfg.gpsTimezone.length() > 0 && TimeHelper::isValidPosixTz(cfg.gpsTimezone)) {
            String abbr;
            for (size_t i = 0; i < cfg.gpsTimezone.length(); i++) {
                char c = cfg.gpsTimezone[i];
                if (c == '-' || c == '+' || (c >= '0' && c <= '9')) break;
                abbr += c;
            }
            addRow("Timezone", abbr + " (auto-DST)");
        } else if (cfg.gpsClockOffset != 0) {
            addRow("Clock Offset", String(cfg.gpsClockOffset) + "h");
        }

        // Location advert toggle
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
            lv_label_set_text(lbl, t("gps_loc_advert"));

            lv_obj_t* sw = lv_switch_create(row);
            lv_obj_set_size(sw, 40, 20);
            if (cfg.locationAdvertEnabled) {
                lv_obj_add_state(sw, LV_STATE_CHECKED);
            }

            lv_obj_add_event_cb(sw, locationAdvertToggleCb, LV_EVENT_VALUE_CHANGED, nullptr);
        }

        // Location precision slider
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
            lv_label_set_text(lbl, t("gps_loc_precision"));

            lv_obj_t* valLbl = lv_label_create(labelRow);
            lv_obj_set_style_text_font(valLbl, FONT_BODY, 0);
            lv_obj_set_style_text_color(valLbl, theme::TEXT_SECONDARY, 0);
            lv_label_set_text(valLbl, precisionLabel(cfg.locationPrecision));

            lv_obj_t* slider = lv_slider_create(row);
            lv_obj_set_width(slider, LV_PCT(100));
            lv_slider_set_range(slider, 0, 12);
            int sliderVal = 0;
            if (cfg.locationPrecision == 32) {
                sliderVal = 12;
            } else if (cfg.locationPrecision >= 10 && cfg.locationPrecision <= 19) {
                sliderVal = cfg.locationPrecision - 9;
            }
            lv_slider_set_value(slider, sliderVal, LV_ANIM_OFF);

            lv_obj_add_event_cb(slider, locationPrecisionChangedCb, LV_EVENT_VALUE_CHANGED, valLbl);
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

void RadioGpsScreen::hide() {
    if (_screen) {
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_remove_obj(_content);
        }
        lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void RadioGpsScreen::backBtnCb(lv_event_t* e) {
    UIManager::instance().popScreen();
}

void RadioGpsScreen::offgridToggleCb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool enabling = lv_obj_has_state(sw, LV_STATE_CHECKED);
    const auto& cfg = ConfigManager::instance().config();
    float og = mclite::offgridFreqFor(cfg.radio.frequency);

    static char bodyBuf[192];
    if (enabling) {
        snprintf(bodyBuf, sizeof(bodyBuf), t("offgrid_confirm_on_body"), (int)og);
    } else {
        snprintf(bodyBuf, sizeof(bodyBuf), t("offgrid_confirm_off_body"), cfg.radio.frequency);
    }

    static const char* btns[3];
    btns[0] = t("btn_cancel");
    btns[1] = t("reboot_now");
    btns[2] = "";

    const char* title = enabling ? t("offgrid_confirm_on_title") : t("offgrid_confirm_off_title");

    lv_obj_t* msgbox = lv_msgbox_create(NULL, title, bodyBuf, btns, false);
    lv_obj_center(msgbox);
    lv_obj_set_style_bg_color(msgbox, theme::BG_SECONDARY, 0);
    lv_obj_set_style_text_color(msgbox, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(msgbox, FONT_HEADING, 0);

    lv_obj_t* btnm = lv_msgbox_get_btns(msgbox);
    if (btnm) UIManager::instance().switchToModalGroup(btnm);

    lv_obj_add_event_cb(msgbox, [](lv_event_t* ev) {
        lv_obj_t* mbox = lv_event_get_current_target(ev);
        uint16_t btnIdx = lv_msgbox_get_active_btn(mbox);
        if (btnIdx == LV_BTNMATRIX_BTN_NONE) return;

        if (btnIdx == 1) {
            auto& mgr = ConfigManager::instance();
            mgr.config().offgrid.enabled = !mgr.config().offgrid.enabled;
            mgr.save();
            UIManager::instance().restoreFromModalGroup();
            lv_msgbox_close(mbox);
            delay(200);
            ESP.restart();
            return;
        }

        UIManager::instance().restoreFromModalGroup();
        lv_msgbox_close(mbox);
    }, LV_EVENT_VALUE_CHANGED, NULL);
}

const char* RadioGpsScreen::precisionLabel(uint8_t precision) {
    if (precision == 0) return t("gps_prec_off");
    if (precision == 32) return t("gps_prec_full");
    switch (precision) {
        case 10: return "~23 km";
        case 11: return "~12 km";
        case 12: return "~5.8 km";
        case 13: return "~2.9 km";
        case 14: return "~1.5 km";
        case 15: return "~729 m";
        case 16: return "~364 m";
        case 17: return "~182 m";
        case 18: return "~91 m";
        case 19: return "~45 m";
        default: return t("gps_prec_full");
    }
}

void RadioGpsScreen::gpsToggleCb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    auto& mgr = ConfigManager::instance();
    mgr.config().gpsEnabled = enabled;
    mgr.save();
    UIManager::instance().showToast(enabled ? t("enabled") : t("disabled"));
}

void RadioGpsScreen::locationAdvertToggleCb(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    auto& mgr = ConfigManager::instance();
    mgr.config().locationAdvertEnabled = enabled;
    mgr.save();
    UIManager::instance().showToast(enabled ? t("enabled") : t("disabled"));
}

void RadioGpsScreen::locationPrecisionChangedCb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    lv_obj_t* valLbl = (lv_obj_t*)lv_event_get_user_data(e);
    int v = lv_slider_get_value(slider);
    uint8_t precision = 0;
    if (v == 0) {
        precision = 0;
    } else if (v == 12) {
        precision = 32;
    } else {
        precision = v + 9;
    }
    auto& mgr = ConfigManager::instance();
    mgr.config().locationPrecision = precision;
    mgr.save();
    if (valLbl) {
        lv_label_set_text(valLbl, precisionLabel(precision));
    }
}

// ---- Edit modal ----

void RadioGpsScreen::showEditModal(const char* title, const String& currentValue, const char* field) {
    if (_editModal) hideEditModal();

    _editField = field;

    _editModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_editModal, Display::width(), Display::height());
    lv_obj_set_pos(_editModal, 0, 0);
    lv_obj_set_style_bg_color(_editModal, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_editModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_editModal, 0, 0);
    lv_obj_set_style_radius(_editModal, 0, 0);
    lv_obj_set_style_pad_all(_editModal, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_editModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_editModal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_editModal, theme::PAD_MEDIUM, 0);
    lv_obj_clear_flag(_editModal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(_editModal);
    lv_obj_set_style_text_font(lbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(lbl, title);

    _editTextarea = lv_textarea_create(_editModal);
    lv_obj_set_width(_editTextarea, LV_PCT(80));
    lv_obj_set_height(_editTextarea, 40);
    lv_textarea_set_one_line(_editTextarea, true);
    lv_textarea_set_max_length(_editTextarea, 32);
    lv_textarea_set_text(_editTextarea, currentValue.c_str());
    lv_obj_set_style_text_font(_editTextarea, FONT_BODY, 0);
    lv_obj_set_style_text_color(_editTextarea, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_bg_color(_editTextarea, theme::BG_SECONDARY, 0);
    lv_obj_set_style_border_color(_editTextarea, theme::ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(_editTextarea, 1, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(_editTextarea, editConfirmCb, LV_EVENT_READY, this);

    lv_obj_t* btnRow = lv_obj_create(_editModal);
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
    lv_obj_add_event_cb(cancelBtn, editCancelCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, t("btn_cancel"));
    lv_obj_set_style_text_font(cancelLbl, FONT_BODY, 0);
    lv_obj_center(cancelLbl);

    lv_obj_t* saveBtn = lv_btn_create(btnRow);
    lv_obj_set_size(saveBtn, 80, 36);
    lv_obj_set_style_bg_color(saveBtn, theme::ACCENT, 0);
    lv_obj_set_style_radius(saveBtn, 4, 0);
    lv_obj_add_event_cb(saveBtn, editConfirmCb, LV_EVENT_CLICKED, this);
    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, t("btn_save"));
    lv_obj_set_style_text_font(saveLbl, FONT_BODY, 0);
    lv_obj_center(saveLbl);

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _editTextarea);
        lv_group_focus_obj(_editTextarea);
        lv_group_set_editing(grp, true);
    }
}

void RadioGpsScreen::hideEditModal() {
    if (!_editModal) return;
    if (_editTextarea) {
        lv_group_t* grp = lv_group_get_default();
        if (grp) lv_group_remove_obj(_editTextarea);
    }
    lv_obj_del(_editModal);
    _editModal = nullptr;
    _editTextarea = nullptr;
    _editField = "";
}

void RadioGpsScreen::editRowCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* row = lv_event_get_target(e);
    if (!self || !row) return;

    EditRowCtx* ctx = (EditRowCtx*)lv_obj_get_user_data(row);
    if (!ctx) return;

    char titleBuf[48];
    snprintf(titleBuf, sizeof(titleBuf), t("radio_edit_title"), ctx->label);
    self->showEditModal(titleBuf, ctx->value, ctx->field);
}

void RadioGpsScreen::editConfirmCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_editTextarea) return;

    const char* raw = lv_textarea_get_text(self->_editTextarea);
    String val(raw);
    val.trim();

    auto& cfg = ConfigManager::instance().config();
    bool changed = false;

    if (self->_editField == "freq") {
        float f = val.toFloat();
        if (f > 400.0f && f < 1000.0f) {
            cfg.radio.frequency = f;
            changed = true;
        } else {
            UIManager::instance().showToast(t("invalid_value"));
            return;
        }
    } else if (self->_editField == "sf") {
        int v = val.toInt();
        if (v >= 5 && v <= 12) {
            cfg.radio.spreadingFactor = (uint8_t)v;
            changed = true;
        } else {
            UIManager::instance().showToast(t("invalid_value"));
            return;
        }
    } else if (self->_editField == "bw") {
        float f = val.toFloat();
        if (f >= 7.8f && f <= 500.0f) {
            cfg.radio.bandwidth = f;
            changed = true;
        } else {
            UIManager::instance().showToast(t("invalid_value"));
            return;
        }
    } else if (self->_editField == "cr") {
        int v = val.toInt();
        if (v >= 5 && v <= 8) {
            cfg.radio.codingRate = (uint8_t)v;
            changed = true;
        } else {
            UIManager::instance().showToast(t("invalid_value"));
            return;
        }
    } else if (self->_editField == "txp") {
        int v = val.toInt();
        if (v >= -9 && v <= 22) {
            cfg.radio.txPower = (int8_t)v;
            changed = true;
        } else {
            UIManager::instance().showToast(t("invalid_value"));
            return;
        }
    } else if (self->_editField == "scope") {
        cfg.radio.scope = val;
        changed = true;
    }

    self->hideEditModal();

    if (changed) {
        ConfigManager::instance().save();
        self->show();
        self->saveRadioAndReboot();
    }
}

void RadioGpsScreen::editCancelCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    if (self) self->hideEditModal();
}

void RadioGpsScreen::saveRadioAndReboot() {
    static const char* btns[3];
    btns[0] = t("reboot_later");
    btns[1] = t("reboot_now");
    btns[2] = "";

    lv_obj_t* msgbox = lv_msgbox_create(NULL, t("radio_edit_title"), t("reboot_to_apply"), btns, false);
    lv_obj_center(msgbox);
    lv_obj_set_style_bg_color(msgbox, theme::BG_SECONDARY, 0);
    lv_obj_set_style_text_color(msgbox, theme::TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(msgbox, FONT_HEADING, 0);

    lv_obj_t* btnm = lv_msgbox_get_btns(msgbox);
    if (btnm) UIManager::instance().switchToModalGroup(btnm);

    lv_obj_add_event_cb(msgbox, [](lv_event_t* ev) {
        lv_obj_t* mbox = lv_event_get_current_target(ev);
        uint16_t btnIdx = lv_msgbox_get_active_btn(mbox);
        if (btnIdx == LV_BTNMATRIX_BTN_NONE) return;

        if (btnIdx == 1) {
            UIManager::instance().restoreFromModalGroup();
            lv_msgbox_close(mbox);
            delay(200);
            ESP.restart();
            return;
        }

        UIManager::instance().restoreFromModalGroup();
        lv_msgbox_close(mbox);
    }, LV_EVENT_VALUE_CHANGED, NULL);
}

// ---- Path hash modal ----

void RadioGpsScreen::showPathModal() {
    if (_pathModal) hidePathModal();

    _pathModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_pathModal, Display::width(), Display::height());
    lv_obj_set_pos(_pathModal, 0, 0);
    lv_obj_set_style_bg_color(_pathModal, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_pathModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_pathModal, 0, 0);
    lv_obj_set_style_radius(_pathModal, 0, 0);
    lv_obj_set_style_pad_all(_pathModal, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_pathModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_pathModal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_pathModal, theme::PAD_MEDIUM, 0);
    lv_obj_clear_flag(_pathModal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_pathModal);
    lv_obj_set_style_text_font(title, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(title, t("radio_edit_path"));

    const char* labels[] = {"1 B/hop", "2 B/hop", "3 B/hop"};
    for (uint8_t i = 0; i < 3; i++) {
        lv_obj_t* row = lv_obj_create(_pathModal);
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
        lv_label_set_text(lbl, labels[i]);

        if (ConfigManager::instance().config().radio.pathHashMode == i) {
            lv_obj_t* check = lv_label_create(row);
            lv_obj_set_style_text_font(check, FONT_BODY, 0);
            lv_obj_set_style_text_color(check, theme::ACCENT, 0);
            lv_label_set_text(check, LV_SYMBOL_OK);
        }

        lv_obj_set_user_data(row, (void*)(size_t)i);
        lv_obj_add_event_cb(row, [](lv_event_t* ev) {
            RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(ev));
            lv_obj_t* r = lv_event_get_target(ev);
            if (!self || !r) return;
            uint8_t mode = (uint8_t)(size_t)lv_obj_get_user_data(r);
            auto& cfg = ConfigManager::instance().config();
            if (cfg.radio.pathHashMode != mode) {
                cfg.radio.pathHashMode = mode;
                ConfigManager::instance().save();
                self->show();
                self->saveRadioAndReboot();
            }
            self->hidePathModal();
        }, LV_EVENT_SHORT_CLICKED, this);
    }

    lv_obj_t* closeBtn = lv_btn_create(_pathModal);
    lv_obj_set_size(closeBtn, 120, 36);
    lv_obj_set_style_bg_color(closeBtn, theme::BG_SECONDARY, 0);
    lv_obj_set_style_radius(closeBtn, 4, 0);
    lv_obj_add_event_cb(closeBtn, [](lv_event_t* ev) {
        RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(ev));
        if (self) self->hidePathModal();
    }, LV_EVENT_CLICKED, this);
    lv_obj_t* closeLbl = lv_label_create(closeBtn);
    lv_label_set_text(closeLbl, t("btn_close"));
    lv_obj_set_style_text_font(closeLbl, FONT_BODY, 0);
    lv_obj_center(closeLbl);

    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        uint32_t cnt = lv_obj_get_child_cnt(_pathModal);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t* child = lv_obj_get_child(_pathModal, i);
            if (child && lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE)) {
                lv_group_add_obj(grp, child);
            }
        }
        if (cnt > 1) lv_group_focus_obj(lv_obj_get_child(_pathModal, 1));
        lv_group_set_editing(grp, true);
    }
}

void RadioGpsScreen::hidePathModal() {
    if (!_pathModal) return;
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        uint32_t cnt = lv_obj_get_child_cnt(_pathModal);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t* child = lv_obj_get_child(_pathModal, i);
            if (child) lv_group_remove_obj(child);
        }
    }
    lv_obj_del(_pathModal);
    _pathModal = nullptr;
}

// ---- Preset modal ----

void RadioGpsScreen::loadPresets() {
    _presets.clear();
    auto& sd = SDCard::instance();
    if (!sd.isMounted() || !sd.fileExists("/mclite/radio_presets.json")) {
        return;
    }

    String json = sd.readFile("/mclite/radio_presets.json");
    if (json.isEmpty()) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        RadioPreset p;
        p.title = obj["title"] | "";
        p.description = obj["description"] | "";
        p.frequency = obj["frequency"] | 0.0f;
        p.spreadingFactor = obj["spreading_factor"] | 0;
        p.bandwidth = obj["bandwidth"] | 0.0f;
        p.codingRate = obj["coding_rate"] | 0;
        if (p.title.length() > 0 && p.frequency > 0) {
            _presets.push_back(p);
        }
    }
}

void RadioGpsScreen::showPresetModal() {
    if (_presetModal) hidePresetModal();
    loadPresets();

    _presetModal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_presetModal, Display::width(), Display::height());
    lv_obj_set_pos(_presetModal, 0, 0);
    lv_obj_set_style_bg_color(_presetModal, theme::BG_PRIMARY, 0);
    lv_obj_set_style_bg_opa(_presetModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_presetModal, 0, 0);
    lv_obj_set_style_radius(_presetModal, 0, 0);
    lv_obj_set_style_pad_all(_presetModal, theme::PAD_LARGE, 0);
    lv_obj_set_flex_flow(_presetModal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_presetModal, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_presetModal, theme::PAD_MEDIUM, 0);
    lv_obj_clear_flag(_presetModal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(_presetModal);
    lv_obj_set_style_text_font(title, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(title, t("radio_preset_title"));

    if (_presets.empty()) {
        lv_obj_t* empty = lv_label_create(_presetModal);
        lv_obj_set_style_text_font(empty, FONT_BODY, 0);
        lv_obj_set_style_text_color(empty, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(empty, t("radio_preset_empty"));
    } else {
        _presetList = lv_obj_create(_presetModal);
        lv_obj_set_width(_presetList, LV_PCT(100));
        lv_obj_set_height(_presetList, Display::height() - 140);
        lv_obj_set_style_bg_opa(_presetList, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_presetList, 0, 0);
        lv_obj_set_style_pad_all(_presetList, 0, 0);
        lv_obj_set_flex_flow(_presetList, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(_presetList, theme::PAD_SMALL, 0);
        lv_obj_add_flag(_presetList, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(_presetList, LV_DIR_VER);

        for (size_t i = 0; i < _presets.size(); i++) {
            lv_obj_t* row = lv_obj_create(_presetList);
            lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(row, theme::BG_SECONDARY, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_style_radius(row, 4, 0);
            lv_obj_set_style_pad_all(row, theme::PAD_SMALL, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

            lv_obj_t* lbl = lv_label_create(row);
            lv_obj_set_style_text_font(lbl, FONT_BODY, 0);
            lv_obj_set_style_text_color(lbl, theme::TEXT_PRIMARY, 0);
            lv_label_set_text(lbl, _presets[i].title.c_str());

            if (_presets[i].description.length() > 0) {
                lv_obj_t* desc = lv_label_create(row);
                lv_obj_set_style_text_font(desc, FONT_SMALL, 0);
                lv_obj_set_style_text_color(desc, theme::TEXT_SECONDARY, 0);
                lv_label_set_text(desc, _presets[i].description.c_str());
            }

            lv_obj_set_user_data(row, (void*)i);
            lv_obj_add_event_cb(row, presetRowCb, LV_EVENT_SHORT_CLICKED, this);
        }
    }

    lv_obj_t* closeBtn = lv_btn_create(_presetModal);
    lv_obj_set_size(closeBtn, 120, 36);
    lv_obj_set_style_bg_color(closeBtn, theme::BG_SECONDARY, 0);
    lv_obj_set_style_radius(closeBtn, 4, 0);
    lv_obj_add_event_cb(closeBtn, presetCloseCb, LV_EVENT_CLICKED, this);
    lv_obj_t* closeLbl = lv_label_create(closeBtn);
    lv_label_set_text(closeLbl, t("btn_close"));
    lv_obj_set_style_text_font(closeLbl, FONT_BODY, 0);
    lv_obj_center(closeLbl);

    lv_group_t* grp = lv_group_get_default();
    if (grp && _presetList) {
        uint32_t cnt = lv_obj_get_child_cnt(_presetList);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_group_add_obj(grp, lv_obj_get_child(_presetList, i));
        }
        if (cnt > 0) lv_group_focus_obj(lv_obj_get_child(_presetList, 0));
        lv_group_set_editing(grp, true);
    } else if (grp && closeBtn) {
        lv_group_add_obj(grp, closeBtn);
        lv_group_focus_obj(closeBtn);
    }
}

void RadioGpsScreen::hidePresetModal() {
    if (!_presetModal) return;
    lv_group_t* grp = lv_group_get_default();
    if (grp && _presetList) {
        uint32_t cnt = lv_obj_get_child_cnt(_presetList);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_group_remove_obj(lv_obj_get_child(_presetList, i));
        }
    }
    if (grp) {
        uint32_t cnt = lv_obj_get_child_cnt(_presetModal);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t* child = lv_obj_get_child(_presetModal, i);
            if (child) lv_group_remove_obj(child);
        }
    }
    lv_obj_del(_presetModal);
    _presetModal = nullptr;
    _presetList = nullptr;
}

void RadioGpsScreen::applyPreset(const RadioPreset& preset) {
    auto& cfg = ConfigManager::instance().config();
    cfg.radio.frequency = preset.frequency;
    cfg.radio.spreadingFactor = preset.spreadingFactor;
    cfg.radio.bandwidth = preset.bandwidth;
    cfg.radio.codingRate = preset.codingRate;
    ConfigManager::instance().save();
    show();
    saveRadioAndReboot();
}

void RadioGpsScreen::presetBtnCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    if (self) self->showPresetModal();
}

void RadioGpsScreen::presetRowCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    lv_obj_t* row = lv_event_get_target(e);
    if (!self || !row) return;

    size_t idx = (size_t)lv_obj_get_user_data(row);
    if (idx < self->_presets.size()) {
        self->hidePresetModal();
        self->applyPreset(self->_presets[idx]);
    }
}

void RadioGpsScreen::presetCloseCb(lv_event_t* e) {
    RadioGpsScreen* self = static_cast<RadioGpsScreen*>(lv_event_get_user_data(e));
    if (self) self->hidePresetModal();
}

}  // namespace mclite
