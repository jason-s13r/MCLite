#pragma once

// Tiny semantic-version compare. Header-only so it can be unit-tested natively
// without pulling in any Arduino/ESP deps.

namespace mclite {

// Parse up to 3 dot-separated numeric components (major.minor.patch) from a
// version string, skipping an optional leading 'v'/'V'. Missing components and
// any non-numeric tail are treated as 0 / ignored. e.g. "v0.2" -> {0,2,0}.
inline void parseSemver(const char* s, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!s) return;
    if (*s == 'v' || *s == 'V') ++s;
    for (int part = 0; part < 3 && *s; ++part) {
        int v = 0;
        bool sawDigit = false;
        while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; sawDigit = true; }
        out[part] = v;
        if (!sawDigit) break;   // expected a number but found none
        if (*s == '.') ++s;     // consume separator and continue
        else break;             // end of string or non-dot tail
    }
}

// Returns -1 if a < b, 0 if equal, 1 if a > b (compares major.minor.patch).
inline int compareVersions(const char* a, const char* b) {
    int va[3], vb[3];
    parseSemver(a, va);
    parseSemver(b, vb);
    for (int i = 0; i < 3; ++i) {
        if (va[i] < vb[i]) return -1;
        if (va[i] > vb[i]) return 1;
    }
    return 0;
}

}  // namespace mclite
