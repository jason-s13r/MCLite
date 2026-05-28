#pragma once

#include <lvgl.h>

#include "../hal/boards/board.h"

// Icon aliases — simple text characters that always render correctly
#define ICON_DM       "@"                  // DM contact
#define ICON_CHANNEL  "#"                  // Public channel
#define ICON_PRIVATE  "*"                  // Private channel
#define ICON_ROOM     "R"                  // Room server
#define ICON_LOCK     LV_SYMBOL_CLOSE      // PIN lock screen

namespace mclite {
namespace theme {

// Colors — dark theme, high contrast for daylight readability
constexpr lv_color_t BG_PRIMARY      = LV_COLOR_MAKE(0x1A, 0x1A, 0x2E);  // Deep navy
constexpr lv_color_t BG_SECONDARY    = LV_COLOR_MAKE(0x16, 0x21, 0x3E);  // Slightly lighter
constexpr lv_color_t BG_STATUS_BAR   = LV_COLOR_MAKE(0x0F, 0x0F, 0x1A);  // Darkest
constexpr lv_color_t BG_INPUT        = LV_COLOR_MAKE(0x22, 0x22, 0x3A);  // Input field

constexpr lv_color_t TEXT_PRIMARY    = LV_COLOR_MAKE(0xE8, 0xE8, 0xF0);  // Bright white
constexpr lv_color_t TEXT_SECONDARY  = LV_COLOR_MAKE(0x88, 0x88, 0xAA);  // Muted
constexpr lv_color_t TEXT_TIMESTAMP  = LV_COLOR_MAKE(0x66, 0x66, 0x88);  // Dim

constexpr lv_color_t BUBBLE_SELF      = LV_COLOR_MAKE(0x00, 0x5A, 0xD4);  // Blue (outgoing)
constexpr lv_color_t BUBBLE_SELF_META = LV_COLOR_MAKE(0x99, 0xBB, 0xEE);  // Light blue (timestamp on blue)
constexpr lv_color_t BUBBLE_THEM      = LV_COLOR_MAKE(0x2A, 0x2A, 0x45);  // Dark gray (incoming)

constexpr lv_color_t ACCENT          = LV_COLOR_MAKE(0x00, 0x7A, 0xFF);  // Bright blue
constexpr lv_color_t UNREAD_DOT      = LV_COLOR_MAKE(0x00, 0xCC, 0x66);  // Green
constexpr lv_color_t ONLINE_DOT      = LV_COLOR_MAKE(0x00, 0xCC, 0x66);  // Green
constexpr lv_color_t BATTERY_LOW     = LV_COLOR_MAKE(0xFF, 0x44, 0x44);  // Red
constexpr lv_color_t BATTERY_OK      = LV_COLOR_MAKE(0x00, 0xCC, 0x66);  // Green
constexpr lv_color_t GPS_LAST_KNOWN  = LV_COLOR_MAKE(0xFF, 0xAA, 0x00);  // Amber/yellow
constexpr lv_color_t OFFGRID_ACCENT  = LV_COLOR_MAKE(0xFF, 0x8C, 0x00);  // Warm orange (offgrid mode indicator)
constexpr lv_color_t ROOM_ACCENT     = LV_COLOR_MAKE(0xA2, 0x59, 0xFF);  // Purple (room server icon/header)

// Spacing
constexpr int PAD_SMALL   = 4;
constexpr int PAD_MEDIUM  = 8;
constexpr int PAD_LARGE   = 12;

// Safe-area insets for displays with rounded bezel corners. T-Deck's rect LCD
// uses 0; T-Watch's AMOLED clips into the panel at each rounded corner —
// vertical clipping is more aggressive than horizontal.
// T-Watch's rounded AMOLED used to push *every* content widget down by
// SAFE_AREA_TOP/BOTTOM=24 to clear the bezel curves. R8 stops doing that:
// the status bar at the top and the new footer at the bottom now ABSORB
// the vertical safe area (their inner pad_top / pad_bottom keep content
// out of the rounded zones), so widgets in between can use the full
// vertical band between the two bars without further offset.
#ifdef PLATFORM_TWATCH
constexpr int SAFE_AREA_TOP    = 0;    // was 24 — now absorbed by STATUS_BAR
constexpr int SAFE_AREA_BOTTOM = 0;    // was 24 — now absorbed by FOOTER
constexpr int SAFE_AREA_LEFT   = 20;
constexpr int SAFE_AREA_RIGHT  = 20;
#else
constexpr int SAFE_AREA_TOP    = 0;
constexpr int SAFE_AREA_BOTTOM = 0;
constexpr int SAFE_AREA_LEFT   = 0;
constexpr int SAFE_AREA_RIGHT  = 0;
#endif

// Width of the content area after horizontal safe-area insets. Equals the full
// display width on T-Deck; 32px narrower on T-Watch. Use this (centered) for
// any content-bearing widget (status bar, chat header/input bar, list rows).
// Full-screen backgrounds can stay at Display::width() — the rounded corners
// just clip the background fill.
constexpr int CONTENT_WIDTH = BOARD_DISP_W - SAFE_AREA_LEFT - SAFE_AREA_RIGHT;

// Status bar — taller on T-Watch to fit a two-row layout (centered device
// name on top, finger-sized centered icons below). R8: bar height grows
// from 72 to 96 to absorb the 24 px rounded-top safe area; inner pad_top
// keeps content out of the corner. The new FOOTER does the same on the
// bottom and hosts the clock. T-Deck stays single-row, no footer.
#ifdef PLATFORM_TWATCH
constexpr int STATUS_BAR_HEIGHT   = 96;
constexpr int FOOTER_HEIGHT       = 64;
constexpr int STATUS_BAR_PAD_V    = 20;  // inner top pad — clear rounded top
constexpr int FOOTER_PAD_V        = 20;  // inner bottom pad — clear rounded bottom
constexpr int STATUS_BAR_ICON_GAP = PAD_LARGE;
#else
constexpr int STATUS_BAR_HEIGHT = 24;
constexpr int FOOTER_HEIGHT     = 0;     // T-Deck has no footer
#endif

// Inner horizontal padding for full-width bars (status bar, chat header, chat
// input bar). On T-Deck these stay at PAD_SMALL so visuals are unchanged. On
// T-Watch the inner content needs to clear the rounded-corner safe area AND
// have generous touch breathing room — status bar gets the most, input bar
// slightly more than the header.
#ifdef PLATFORM_TWATCH
constexpr int STATUS_BAR_PAD_HOR  = SAFE_AREA_LEFT + PAD_LARGE;   // 32
constexpr int INPUT_BAR_PAD_HOR   = SAFE_AREA_LEFT + PAD_MEDIUM;  // 28
constexpr int CHAT_HEADER_PAD_HOR = SAFE_AREA_LEFT + PAD_SMALL;   // 24
#else
constexpr int STATUS_BAR_PAD_HOR  = PAD_SMALL;
constexpr int INPUT_BAR_PAD_HOR   = PAD_SMALL;
constexpr int CHAT_HEADER_PAD_HOR = PAD_SMALL;
#endif

// Chat header / input bar heights. Touch finger targets on T-Watch need to be
// larger than trackball-click targets on T-Deck.
#ifdef PLATFORM_TWATCH
constexpr int CHAT_HEADER_HEIGHT = 56;
constexpr int CHAT_INPUT_HEIGHT  = 72;
#else
constexpr int CHAT_HEADER_HEIGHT = 28;
constexpr int CHAT_INPUT_HEIGHT  = 36;
#endif

// Button + textarea sizes. T-Deck values match the prior hardcoded numbers
// so the binary stays bit-identical; T-Watch bumps everything for finger
// touch within the now-taller header/input-bar slots.
#ifdef PLATFORM_TWATCH
constexpr int BTN_ACTION_W      = 48;  // chat input bar action buttons (canned, gps)
constexpr int BTN_ACTION_H      = 48;
constexpr int BTN_SEND_W        = 60;
constexpr int BTN_SEND_H        = 48;
constexpr int BTN_HEADER_BACK_W = 48;
constexpr int BTN_HEADER_BACK_H = 40;
constexpr int BTN_HEADER_ICON_W = 48;  // header icon buttons (HeardAdverts, Admin close)
constexpr int BTN_HEADER_ICON_H = 40;
constexpr int CHAT_NAME_BTN_H   = 40;
constexpr int CHAT_TEXTAREA_H   = 48;
#else
constexpr int BTN_ACTION_W      = 28;
constexpr int BTN_ACTION_H      = 28;
constexpr int BTN_SEND_W        = 50;
constexpr int BTN_SEND_H        = 28;
constexpr int BTN_HEADER_BACK_W = 30;
constexpr int BTN_HEADER_BACK_H = 20;
constexpr int BTN_HEADER_ICON_W = 32;
constexpr int BTN_HEADER_ICON_H = 24;
constexpr int CHAT_NAME_BTN_H   = 20;
constexpr int CHAT_TEXTAREA_H   = 28;
#endif

// Standard modal/text widths — derived from CONTENT_WIDTH so they fit
// each board. T-Deck: 320 - 40 = 280 (matches prior 280 literal).
constexpr int MODAL_TEXT_WIDTH = CONTENT_WIDTH - 40;

// Chat bubbles
constexpr int BUBBLE_RADIUS     = 8;
constexpr int BUBBLE_MAX_WIDTH  = BOARD_DISP_W * 3 / 4;  // ~75% of screen width
constexpr int BUBBLE_PAD        = 6;

// Conversation list — taller rows on T-Watch for finger touch + bigger fonts.
#ifdef PLATFORM_TWATCH
constexpr int CONVO_ROW_HEIGHT  = 64;
#else
constexpr int CONVO_ROW_HEIGHT  = 48;
#endif

// Fonts — T-Watch bumps every level up by 2-4pt so labels are readable at
// arm's length. T-Deck unchanged.
#ifdef PLATFORM_TWATCH
#define FONT_SMALL          &lv_font_montserrat_14
#define FONT_NORMAL         &lv_font_montserrat_16
#define FONT_LARGE          &lv_font_montserrat_20
#define FONT_TITLE          &lv_font_montserrat_24
#define FONT_STATUSBAR_ICON &lv_font_montserrat_28  // 2x current icon size for finger taps
#else
#define FONT_SMALL    &lv_font_montserrat_12
#define FONT_NORMAL   &lv_font_montserrat_14
#define FONT_LARGE    &lv_font_montserrat_16
#define FONT_TITLE    &lv_font_montserrat_20
#endif

}  // namespace theme
}  // namespace mclite
