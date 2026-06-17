#pragma once

#include <stdint.h>

namespace mclite {

// Parse an "#RRGGBB" or "RRGGBB" hex color string into a packed 0xRRGGBB value.
// Returns false (leaving `out` untouched) on wrong length or non-hex chars.
// Pure / no Arduino deps so it's unit-testable on the host.
inline bool parseHexRGB(const char* s, uint32_t& out) {
    if (!s) return false;
    if (s[0] == '#') s++;
    uint32_t v = 0;
    int n = 0;
    for (; s[n] != '\0'; n++) {
        if (n >= 6) return false;            // too long
        char c = s[n];
        uint8_t d;
        if      (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
        else return false;                   // non-hex
        v = (v << 4) | d;
    }
    if (n != 6) return false;                // too short
    out = v;
    return true;
}

}  // namespace mclite
