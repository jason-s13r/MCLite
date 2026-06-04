#include "AdminScreen.h"
#include <Arduino.h>
#include <time.h>
#include "UIManager.h"
#include "theme.h"
#include "../config/ConfigManager.h"
#include "../hal/Display.h"
#include "../mesh/ContactStore.h"
#include "../mesh/ChannelStore.h"
#include "../mesh/MeshManager.h"
#include "../storage/HeardAdvertCache.h"
#include "../storage/MessageStore.h"
#include "../hal/Battery.h"
#include "../hal/GPS.h"
#include "../hal/Speaker.h"
#include "../config/defaults.h"
#include "../i18n/I18n.h"
#include "../util/TimeHelper.h"
#include "../util/offgrid.h"

namespace mclite {

void AdminScreen::create(lv_obj_t* parent) {
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
        if (dir == LV_DIR_RIGHT) UIManager::instance().goHome();
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
    lv_obj_t* title = lv_win_add_title(_screen, t("admin_title"));
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

void AdminScreen::show() {
    if (!_screen) return;

    // Clear old content from the scrollable area
    uint32_t childCount = lv_obj_get_child_cnt(_content);
    while (childCount > 0) {
        lv_obj_del(lv_obj_get_child(_content, childCount - 1));
        childCount--;
    }

    const auto& cfg = ConfigManager::instance().config();

    // Helper to add a row
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

    // Helper for section headers
    auto addSection = [this](const char* title) {
        lv_obj_t* lbl = lv_label_create(_content);
        lv_obj_set_style_text_font(lbl, FONT_HEADING, 0);
        lv_obj_set_style_text_color(lbl, theme::ACCENT, 0);
        lv_obj_set_style_pad_top(lbl, theme::PAD_MEDIUM, 0);
        lv_label_set_text(lbl, title);
    };

    // Offgrid toggle — promoted above all sections as the only interactive control.
    // Clickable row with tinted OFFGRID_ACCENT bg; tint depth signals state
    // (subtle when OFF, stronger when ON), distinct from BG_SECONDARY info rows.
    {
        lv_obj_t* row = lv_obj_create(_content);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(row, theme::OFFGRID_ACCENT, 0);
        lv_obj_set_style_bg_opa(row, cfg.offgrid.enabled ? LV_OPA_50 : LV_OPA_20, 0);
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
        lv_label_set_text(lbl, "Offgrid");

        lv_obj_t* val = lv_label_create(row);
        lv_obj_set_style_text_font(val, FONT_BODY, 0);
        lv_obj_set_style_text_color(val, theme::TEXT_PRIMARY, 0);
        if (cfg.offgrid.enabled) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%s (%d MHz)",
                     t("offgrid_on"),
                     (int)mclite::offgridFreqFor(cfg.radio.frequency));
            lv_label_set_text(val, buf);
        } else {
            lv_label_set_text(val, t("offgrid_off"));
        }

        lv_obj_add_event_cb(row, offgridToggleCb, LV_EVENT_CLICKED, nullptr);
    }

    // Heard adverts shortcut — mirrors info-row styling but clickable, with chevron.
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

        _heardCountLabel = lv_label_create(row);
        lv_obj_set_style_text_font(_heardCountLabel, FONT_BODY, 0);
        lv_obj_set_style_text_color(_heardCountLabel, theme::TEXT_PRIMARY, 0);
        char rowBuf[40];
        snprintf(rowBuf, sizeof(rowBuf), "%s (%d)",
                 t("heard_adverts_title"),
                 HeardAdvertCache::instance().count());
        lv_label_set_text(_heardCountLabel, rowBuf);
        _heardCacheVersion = HeardAdvertCache::instance().version();

        lv_obj_t* chev = lv_label_create(row);
        lv_obj_set_style_text_font(chev, FONT_BODY, 0);
        lv_obj_set_style_text_color(chev, theme::TEXT_SECONDARY, 0);
        lv_label_set_text(chev, LV_SYMBOL_RIGHT);

        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            UIManager::instance().showScreen(Screen::HEARD_ADVERTS);
        }, LV_EVENT_CLICKED, nullptr);
    }

    // --- Device ---
    addSection(t("sec_device"));
    addRow("Firmware", String("MCLite v") + defaults::FIRMWARE_VERSION);
    addRow("Built", String(__DATE__) + " " + __TIME__);
    addRow("Device Name", cfg.deviceName);
    if (cfg.publicKey.length() > 0) {
        addRow("Public Key", cfg.publicKey.substring(0, 16) + "...");
    }

    // --- Radio ---
    addSection(t("sec_radio"));
    {
        // Show the active frequency: in offgrid mode this is the derived band,
        // with "(offgrid)" marker so users see 869.000 (offgrid) vs 869.618 at a glance.
        float activeFreq = cfg.radio.frequency;
        String freqSuffix = " MHz";
        if (cfg.offgrid.enabled) {
            activeFreq = mclite::offgridFreqFor(cfg.radio.frequency);
            freqSuffix += " (offgrid)";
        }
        addRow("Frequency", String(activeFreq, 3) + freqSuffix);
    }
    addRow("SF / BW", String(cfg.radio.spreadingFactor) + " / " + String(cfg.radio.bandwidth, 1));
    addRow("Coding Rate", String(cfg.radio.codingRate));
    addRow("TX Power", String(cfg.radio.txPower) + " dBm");
    addRow("Scope", cfg.radio.scope);
    {
        char phBuf[16];
        snprintf(phBuf, sizeof(phBuf), "%u B/hop", (unsigned)(cfg.radio.pathHashMode + 1));
        addRow("Path Hash", phBuf);
    }
    addRow("Status", MeshManager::instance().isRadioReady() ? t("ready") : t("error"));

    // Channel utilization (TX duty cycle over last hour)
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

        // Label: prefix + name + badges
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

        // Trash icon button on the right (only for custom channels)
        if (ch.custom) {
            lv_obj_t* trashBtn = lv_btn_create(row);
            lv_obj_set_size(trashBtn, 32, 32);
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

            // Store channel name in user_data on the trash button
            String* nameCopy = new String(ch.name);
            lv_obj_set_user_data(trashBtn, nameCopy);
            lv_obj_add_event_cb(trashBtn, [](lv_event_t* e) {
                String* name = (String*)lv_obj_get_user_data(lv_event_get_target(e));
                delete name;
            }, LV_EVENT_DELETE, nullptr);
        }
    }

    // "Add channel" button row — at the bottom of the channel list
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

    // --- Rooms (read-only — config tool manages add/remove) ---
    {
        const auto& rooms = ConfigManager::instance().config().roomServers;
        char secRoomsBuf[32];
        snprintf(secRoomsBuf, sizeof(secRoomsBuf), t("sec_rooms"), (int)rooms.size());
        addSection(secRoomsBuf);
        auto& store = MessageStore::instance();
        for (size_t i = 0; i < rooms.size(); i++) {
            String info = rooms[i].name;
            info += UIManager::instance().isRoomLoggedIn(i) ? " [online]" : " [offline]";
            // Last sync timestamp (Unix seconds) from the room's history
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

    // --- Display ---
    addSection(t("sec_display"));
    addRow("Brightness", String(cfg.display.brightness));
    addRow("Auto-Dim", cfg.display.autoDimSeconds > 0
        ? String(cfg.display.autoDimSeconds) + "s" : String(t("off")));
    addRow("Dim Brightness", cfg.display.dimBrightness > 0
        ? String(cfg.display.dimBrightness) : String(t("off")));
    addRow("Kbd Backlight", cfg.display.kbdBacklight
        ? String(t("on")) + " (" + String(cfg.display.kbdBrightness) + ")" : String(t("off")));
    if (cfg.display.bootText.length() > 0) {
        addRow("Boot Text", cfg.display.bootText);
    }

    // --- Messaging ---
    addSection(t("sec_messaging"));
    addRow("History", cfg.messaging.saveHistory ? t("enabled") : t("disabled"));
    addRow("Max Per Chat", String(cfg.messaging.maxHistoryPerChat));
    addRow("Max Retries", String(cfg.messaging.maxRetries));
    addRow("Req. Telemetry", cfg.messaging.requestTelemetry ? t("enabled") : t("disabled"));
    addRow("Telemetry Badges", cfg.messaging.showTelemetry);

    // --- GPS ---
    addSection(t("sec_gps"));
    addRow("GPS", cfg.gpsEnabled ? t("enabled") : t("disabled"));
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
            // Show abbreviation (chars before first digit/sign) + "(auto-DST)"
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

    // --- Security ---
    addSection(t("sec_security"));
    String lockLabel = t("off");
    if (cfg.security.lockMode == "key") lockLabel = "Key Lock";
    else if (cfg.security.lockMode == "pin") lockLabel = "PIN Lock";
    addRow("Lock", lockLabel);

    // --- Language ---
    addSection(t("sec_language"));
    addRow(t("lang_current"), I18n::instance().currentLanguage());
    addRow(t("lang_available"), I18n::instance().availableLanguages());

    // --- Licenses ---
    addSection(t("sec_licenses"));
    addRow("MCLite", "MIT");

    // Expandable 3rd-party licenses
    lv_obj_t* licToggle = lv_label_create(_content);
    lv_obj_set_style_text_font(licToggle, FONT_BODY, 0);
    lv_obj_set_style_text_color(licToggle, theme::ACCENT, 0);
    lv_obj_add_flag(licToggle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_top(licToggle, theme::PAD_SMALL, 0);
    String licToggleText = String(LV_SYMBOL_RIGHT " ") + t("licenses_toggle");
    lv_label_set_text(licToggle, licToggleText.c_str());

    lv_obj_t* licContainer = lv_obj_create(_content);
    lv_obj_set_size(licContainer, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(licContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(licContainer, 0, 0);
    lv_obj_set_style_pad_all(licContainer, 0, 0);
    lv_obj_set_style_pad_row(licContainer, 2, 0);
    lv_obj_set_flex_flow(licContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(licContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(licContainer, LV_OBJ_FLAG_HIDDEN);

    // Full copyright notices (MIT requires copyright + permission notice)
    static const char* licenseText =
        "LVGL 8.4.0 - MIT\n"
        "(c) 2021 LVGL Kft\n\n"
        "LovyanGFX 1.2.19 - MIT + BSD-2-Clause\n"
        "(c) lovyan03\n\n"
        "ArduinoJson 7.4.3 - MIT\n"
        "(c) 2014-2026 Benoit Blanchon\n\n"
        "RadioLib 7.3.0 - MIT\n"
        "(c) 2018 Jan Gromes\n\n"
        "MeshCore 1.10.0 - MIT\n"
        "(c) 2025 Scott Powell / rippleradios.com\n\n"
        "base64 1.4.0 - MIT\n"
        "(c) 2016 Densaugeo\n\n"
        "PNGdec 1.0.3 - Apache-2.0\n"
        "(c) 2020-2024 Larry Bank\n\n"
        "orlp/ed25519 - Zlib\n"
        "(c) 2015 Orson Peters\n\n"
        "Crypto 0.4.0 - MIT\n"
        "(c) 2015, 2018 Southern Storm Software\n\n"
        "RTClib 2.1.4 - MIT\n"
        "(c) 2019 Adafruit Industries\n\n"
        "Adafruit BusIO 1.17.4 - MIT\n"
        "(c) 2017 Adafruit Industries\n\n"
        "CayenneLPP 1.6.1 - MIT\n"
        "(c) Electronic Cats\n\n"
        "Melopero RV3028 1.2.0 - MIT\n"
        "(c) 2020 Melopero\n\n"
        "TinyGPSPlus 1.1.0 - LGPL-2.1\n"
        "(c) 2008-2024 Mikal Hart\n\n"
        "Arduino ESP32 core - LGPL-2.1\n"
        "(c) Espressif\n\n"
        "OpenMoji 15.0 - CC BY-SA 4.0\n"
        "(c) 2026 Gross, B., Utz, D.,\n"
        "& The OpenMoji Contributors\n"
        "https://openmoji.org\n\n"
        "MIT/Apache-2.0/Zlib libraries are used under\n"
        "the terms of those licenses. LGPL-2.1 libraries\n"
        "are dynamically linked; users may replace them\n"
        "by rebuilding with PlatformIO.\n\n"
        "Full license texts: see LICENSES.md";

    lv_obj_t* licLabel = lv_label_create(licContainer);
    lv_obj_set_style_text_font(licLabel, FONT_BODY, 0);
    lv_obj_set_style_text_color(licLabel, theme::TEXT_SECONDARY, 0);
    lv_obj_set_width(licLabel, LV_PCT(100));
    lv_label_set_long_mode(licLabel, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(licLabel, licenseText);

    // Toggle callback
    lv_obj_add_event_cb(licToggle, [](lv_event_t* e) {
        lv_obj_t* toggle = lv_event_get_target(e);
        lv_obj_t* container = (lv_obj_t*)lv_event_get_user_data(e);
        bool hidden = lv_obj_has_flag(container, LV_OBJ_FLAG_HIDDEN);
        if (hidden) {
            lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
            String txt = String(LV_SYMBOL_DOWN " ") + t("licenses_toggle");
            lv_label_set_text(toggle, txt.c_str());
        } else {
            lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
            String txt = String(LV_SYMBOL_RIGHT " ") + t("licenses_toggle");
            lv_label_set_text(toggle, txt.c_str());
        }
    }, LV_EVENT_CLICKED, licContainer);

    // Footer
    lv_obj_t* footer = lv_label_create(_content);
    lv_obj_set_style_text_font(footer, FONT_BODY, 0);
    lv_obj_set_style_text_color(footer, theme::TEXT_TIMESTAMP, 0);
    lv_obj_set_style_pad_top(footer, theme::PAD_MEDIUM, 0);
    lv_label_set_text(footer, t("admin_footer"));

    // Add content area to input group so trackball can scroll
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _content);
        lv_group_focus_obj(_content);
        lv_group_set_editing(grp, true);
    }

    lv_obj_clear_flag(_screen, LV_OBJ_FLAG_HIDDEN);
}

void AdminScreen::backBtnCb(lv_event_t* e) {
    UIManager::instance().goHome();
}

void AdminScreen::offgridToggleCb(lv_event_t* e) {
    // Build the confirm-dialog text with the current derived freq.
    const auto& cfg = ConfigManager::instance().config();
    bool enabling = !cfg.offgrid.enabled;

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
            // "Reboot now" — flip the flag, persist, restart.
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

void AdminScreen::hide() {
    if (_screen) {
        lv_group_t* grp = lv_group_get_default();
        if (grp) {
            lv_group_set_editing(grp, false);
            lv_group_remove_obj(_content);
        }
        // _heardCountLabel lives inside the row that show() recreates each visit,
        // so the pointer is dead until next show(). Drop it now to avoid a
        // dangling deref from tick() if something else paints over it.
        _heardCountLabel = nullptr;
        lv_obj_add_flag(_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void AdminScreen::tick() {
    if (!_screen || lv_obj_has_flag(_screen, LV_OBJ_FLAG_HIDDEN)) return;
    if (!_heardCountLabel) return;

    uint32_t v = HeardAdvertCache::instance().version();
    if (v == _heardCacheVersion) return;
    _heardCacheVersion = v;

    char rowBuf[40];
    snprintf(rowBuf, sizeof(rowBuf), "%s (%d)",
             t("heard_adverts_title"),
             HeardAdvertCache::instance().count());
    lv_label_set_text(_heardCountLabel, rowBuf);
}

// ---- Add hashtag channel modal ----

void AdminScreen::showAddChannelModal() {
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

    // Title
    lv_obj_t* title = lv_label_create(_addChannelModal);
    lv_obj_set_style_text_font(title, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title, theme::TEXT_PRIMARY, 0);
    lv_label_set_text(title, t("add_channel_title"));

    // Textarea
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
    // On-screen keyboard for T-Watch
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

    // Focus textarea so keyboard/trackball input goes there
    lv_group_t* grp = lv_group_get_default();
    if (grp) {
        lv_group_add_obj(grp, _addChannelTextarea);
        lv_group_focus_obj(_addChannelTextarea);
        lv_group_set_editing(grp, true);
    }
}

void AdminScreen::hideAddChannelModal() {
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

void AdminScreen::addChannelBtnCb(lv_event_t* e) {
    AdminScreen* self = static_cast<AdminScreen*>(lv_event_get_user_data(e));
    if (self) self->showAddChannelModal();
}

void AdminScreen::addChannelConfirmCb(lv_event_t* e) {
    AdminScreen* self = static_cast<AdminScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_addChannelTextarea) return;

    const char* raw = lv_textarea_get_text(self->_addChannelTextarea);
    String name(raw);
    name.trim();

    // Ensure it starts with #
    if (name.length() == 0 || name[0] != '#') {
        UIManager::instance().showToast(t("channel_invalid"));
        return;
    }

    // Read switch states
    bool sendSos  = self->_swSendSos  ? lv_obj_has_state(self->_swSendSos,  LV_STATE_CHECKED) : false;
    bool recvSos  = self->_swRecvSos  ? lv_obj_has_state(self->_swRecvSos,  LV_STATE_CHECKED) : false;
    bool readOnly = self->_swReadOnly ? lv_obj_has_state(self->_swReadOnly, LV_STATE_CHECKED) : false;

    // 1. Add to ChannelStore (derive PSK, check duplicates)
    Channel* ch = ChannelStore::instance().deriveHashtagChannel(name, recvSos, sendSos, readOnly);
    if (!ch) {
        UIManager::instance().showToast(t("channel_exists"));
        return;
    }

    // 2. Persist to custom channels file
    if (!ChannelStore::instance().saveCustomChannels()) {
        // Rollback: remove from ChannelStore
        ChannelStore::instance().removeChannelByName(ch->name);
        UIManager::instance().showToast(t("channel_save_fail"));
        return;
    }

    // 3. Create conversation entry so it appears in the convo list immediately
    ConvoId cid{ConvoId::CHANNEL, ch->name};
    MessageStore::instance().ensureConversation(cid, ch->name, ch->isPrivate(), ch->readOnly);
    MessageStore::instance().loadHistory(cid);

    // 4. Register with MeshCore at runtime
    int meshIdx = MeshManager::instance().addChannel(ch->name.c_str(), ch->pskB64.c_str());
    if (meshIdx < 0) {
        UIManager::instance().showToast(t("channel_save_fail"));
        return;
    }

    UIManager::instance().showToast(t("channel_added"));
    self->hideAddChannelModal();

    // Refresh admin screen so the new channel appears immediately
    self->show();
}

void AdminScreen::addChannelCancelCb(lv_event_t* e) {
    AdminScreen* self = static_cast<AdminScreen*>(lv_event_get_user_data(e));
    if (self) self->hideAddChannelModal();
}

// ---- Channel delete (trash icon) ----

void AdminScreen::channelDeleteCb(lv_event_t* e) {
    AdminScreen* self = static_cast<AdminScreen*>(lv_event_get_user_data(e));
    String* name = (String*)lv_obj_get_user_data(lv_event_get_target(e));
    if (!self || !name) return;

    // Confirmation msgbox
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

    // Pass both name and self via user_data so we can refresh the admin screen
    struct DeleteCtx {
        String name;
        AdminScreen* screen;
    };
    DeleteCtx* ctx = new DeleteCtx{*name, self};
    lv_obj_set_user_data(msgbox, ctx);
    lv_obj_add_event_cb(msgbox, [](lv_event_t* ev) {
        DeleteCtx* c = (DeleteCtx*)lv_obj_get_user_data(lv_event_get_current_target(ev));
        lv_obj_t* mbox = lv_event_get_current_target(ev);
        uint16_t btnIdx = lv_msgbox_get_active_btn(mbox);
        if (btnIdx == 1 && c) {
            // Confirmed delete
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
