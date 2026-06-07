#pragma once

#include <Arduino.h>

// Queries the GitHub Releases API for the latest MCLite firmware and finds the
// asset matching this board. Requires an active WiFi connection (caller brings
// WiFi up via WiFiManager). TLS is validated against the arduino-esp32 built-in
// Mozilla root-CA bundle (covers api.github.com and the release-asset CDN).

namespace mclite {

struct RemoteRelease {
    String version;   // parsed from the tag, e.g. "0.2.1" (leading 'v' stripped)
    String url;       // browser_download_url of the board-matching .bin asset
};

class UpdateChecker {
public:
    // Returns true and fills `out` when the latest release has a board-matching
    // asset (any version — the caller compares against MCLITE_VERSION). Returns
    // false on network/TLS/parse error or when no matching asset is present.
    static bool checkLatest(RemoteRelease& out);
};

}  // namespace mclite
