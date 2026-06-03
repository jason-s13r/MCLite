#pragma once
#include <Arduino.h>

namespace mclite {

/**
 * Sanitize a string for on-screen display.
 *
 * 1. Strips U+FE0F (Variation Selector-16) which trails many emoji and
 *    renders as an invisible tofu glyph when the font doesn't contain it.
 *    UTF-8 encoding: \xEF\xB8\x8F
 *
 * 2. Replaces "smart" / "fancy" typographic quotes with plain ASCII quotes
 *    so they render correctly on devices whose fonts lack the glyphs.
 *
 *    U+201C " → "
 *    U+201D " → "
 *    U+2018 ' → '
 *    U+2019 ' → '
 *    U+201A ‚ → ,
 *    U+201E „ → "
 *    U+2032 ′ → '
 *    U+2033 ″ → "
 */
inline String sanitizeForDisplay(const String& src) {
    String out;
    out.reserve(src.length());

    const uint8_t* p = reinterpret_cast<const uint8_t*>(src.c_str());
    size_t len = src.length();
    size_t i = 0;

    while (i < len) {
        // U+FE0F in UTF-8 = EF B8 8F
        if (i + 2 < len && p[i] == 0xEF && p[i + 1] == 0xB8 && p[i + 2] == 0x8F) {
            i += 3;  // skip variation selector
            continue;
        }

        // Multi-byte sequences that start with 0xE0..0xEF are 3-byte UTF-8
        if (i + 2 < len && p[i] >= 0xE0 && p[i] <= 0xEF) {
            uint8_t b1 = p[i];
            uint8_t b2 = p[i + 1];
            uint8_t b3 = p[i + 2];

            // U+201C "  = E2 80 9C
            // U+201D "  = E2 80 9D
            // U+2018 '  = E2 80 98
            // U+2019 '  = E2 80 99
            // U+201A ‚  = E2 80 9A
            // U+201E „  = E2 80 9E
            // U+2032 ′  = E2 80 B2
            // U+2033 ″  = E2 80 B3
            if (b1 == 0xE2 && b2 == 0x80) {
                if (b3 == 0x9C) { out += '"'; i += 3; continue; }
                if (b3 == 0x9D) { out += '"'; i += 3; continue; }
                if (b3 == 0x98) { out += '\''; i += 3; continue; }
                if (b3 == 0x99) { out += '\''; i += 3; continue; }
                if (b3 == 0x9A) { out += ',';  i += 3; continue; }
                if (b3 == 0x9E) { out += '"'; i += 3; continue; }
                if (b3 == 0xB2) { out += '\''; i += 3; continue; }
                if (b3 == 0xB3) { out += '"'; i += 3; continue; }
            }
        }

        out += static_cast<char>(p[i]);
        ++i;
    }

    return out;
}

}  // namespace mclite
