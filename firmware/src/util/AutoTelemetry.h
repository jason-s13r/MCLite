#pragma once

#include <cstdint>

namespace mclite {

// Decide whether a contact is due for an automatic GPS-telemetry refresh.
//
// Pure (no globals) so the scheduling policy stays unit-testable. The caller
// gathers the inputs from ContactLocation / TelemetryCache and the per-contact
// session state; the orchestration (round-robin, idle/duty-cycle gating,
// single-slot serialization) lives in MeshManager::tickAutoTelemetry().
//
//   advertisesLoc : the contact broadcasts its own location (advert/heard GPS)
//                   -> we already get it for free, never request.
//   gaveUp        : backed off this session after too many unanswered tries.
//   hasTelemGps   : we hold a telemetry GPS fix for them (any age).
//   telemAgeMs    : age of that fix (ignored when !hasTelemGps).
//   refreshAgeMs  : refresh threshold — request once the fix reaches this age,
//                   chosen just under the cache's stale window.
//
// Due iff: they don't advertise, we haven't given up, and our telemetry GPS is
// either missing or old enough to refresh before it goes stale.
inline bool autoTelemetryDue(bool advertisesLoc, bool gaveUp,
                             bool hasTelemGps, uint32_t telemAgeMs,
                             uint32_t refreshAgeMs) {
    if (advertisesLoc || gaveUp) return false;
    return !hasTelemGps || telemAgeMs >= refreshAgeMs;
}

}  // namespace mclite
