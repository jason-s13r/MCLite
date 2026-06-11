#pragma once

#include <cstdint>

namespace mclite {

// A contact/node's best-known position, with where it came from and how much to
// trust it. Telemetry is permission-gated and full-precision (accurate); advert
// positions are whatever precision the sender chose to broadcast (we can't tell
// if they obfuscated), so they're flagged approximate.
struct ContactLocation {
    enum Source : uint8_t { NONE, TELEMETRY, ADVERT, HEARD };

    bool     valid       = false;
    double   lat         = 0.0;
    double   lon         = 0.0;
    Source   source      = NONE;
    bool     approximate = true;   // false only for TELEMETRY
    bool     hasAge      = false;  // ADVERT carries no timestamp
    uint32_t ageMs       = 0;      // ms since received/heard (valid if hasAge)
};

// Best-known location for a node by 32-byte public key. Single source of truth
// for every consumer (convo-list badge, telemetry modal, map). Priority:
//   1. fresh telemetry (TelemetryCache, <30 min)  -> accurate
//   2. the contact's advert GPS (ContactInfo.gps) -> approximate
//   3. a heard advert with GPS (HeardAdvertCache) -> approximate
// Returns valid=false if nothing is known. pubKey32 may be null.
ContactLocation bestKnownLocation(const uint8_t* pubKey32);

// True if the node broadcasts its own location — its advert GPS (ContactInfo)
// or a heard advert with GPS. Ignores telemetry on purpose: the auto-refresh
// scheduler uses this to skip contacts whose position we already get for free,
// while judging telemetry freshness separately. pubKey32 may be null.
bool advertisesLocation(const uint8_t* pubKey32);

}  // namespace mclite
