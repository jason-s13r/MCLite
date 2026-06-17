#include "theme.h"
#include "../config/ConfigManager.h"
#include "../util/color.h"
#include <string.h>
#include <stddef.h>

namespace mclite {
namespace theme {

// The live palette. Defaults to DARK until applyThemeFromConfig() runs at boot.
Palette ACTIVE = PALETTE_DARK;

const Palette* builtinPaletteByName(const char* name) {
    if (!name) return nullptr;
    if (!strcmp(name, "dark"))          return &PALETTE_DARK;
    if (!strcmp(name, "light"))         return &PALETTE_LIGHT;
    if (!strcmp(name, "amber"))         return &PALETTE_AMBER;
    if (!strcmp(name, "high_contrast")) return &PALETTE_HIGHCON;
    return nullptr;
}

namespace {

// Canonical color-key name → Palette member offset. Keep in sync with the
// Palette struct order in theme.h. Custom themes reference these keys.
struct ColorKey { const char* key; size_t offset; };
const ColorKey COLOR_KEYS[] = {
    {"bg_primary",       offsetof(Palette, bg_primary)},
    {"bg_secondary",     offsetof(Palette, bg_secondary)},
    {"bg_status_bar",    offsetof(Palette, bg_status_bar)},
    {"bg_input",         offsetof(Palette, bg_input)},
    {"text_primary",     offsetof(Palette, text_primary)},
    {"text_secondary",   offsetof(Palette, text_secondary)},
    {"text_timestamp",   offsetof(Palette, text_timestamp)},
    {"bubble_self",      offsetof(Palette, bubble_self)},
    {"bubble_self_meta", offsetof(Palette, bubble_self_meta)},
    {"bubble_them",      offsetof(Palette, bubble_them)},
    {"bubble_self_text", offsetof(Palette, bubble_self_text)},
    {"accent",           offsetof(Palette, accent)},
    {"unread_dot",       offsetof(Palette, unread_dot)},
    {"online_dot",       offsetof(Palette, online_dot)},
    {"battery_low",      offsetof(Palette, battery_low)},
    {"battery_ok",       offsetof(Palette, battery_ok)},
    {"gps_last_known",   offsetof(Palette, gps_last_known)},
    {"offgrid_accent",   offsetof(Palette, offgrid_accent)},
    {"room_accent",      offsetof(Palette, room_accent)},
    {"scrim",            offsetof(Palette, scrim)},
    {"text_on_accent",   offsetof(Palette, text_on_accent)},
};

// Apply one "key":"#RRGGBB" override to a palette. Unknown keys / bad hex ignored.
void applyOverride(Palette& p, const String& key, const String& hex) {
    uint32_t rgb;
    if (!parseHexRGB(hex.c_str(), rgb)) return;
    lv_color_t c = lv_color_make((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    for (const auto& ck : COLOR_KEYS) {
        if (key == ck.key) {
            *reinterpret_cast<lv_color_t*>(reinterpret_cast<char*>(&p) + ck.offset) = c;
            return;
        }
    }
}

}  // namespace

void applyThemeFromConfig() {
    const auto& cfg = ConfigManager::instance().config();
    const String& name = cfg.display.theme;

    // Built-in?
    if (const Palette* b = builtinPaletteByName(name.c_str())) {
        ACTIVE = *b;
        return;
    }
    // Custom theme: start from its base built-in, apply overrides.
    for (const auto& ct : cfg.display.customThemes) {
        if (name == ct.name) {
            const Palette* base = builtinPaletteByName(ct.base.c_str());
            ACTIVE = base ? *base : PALETTE_DARK;
            for (const auto& c : ct.colors) applyOverride(ACTIVE, c.first, c.second);
            return;
        }
    }
    // Unknown → safe default.
    ACTIVE = PALETTE_DARK;
}

}  // namespace theme
}  // namespace mclite
