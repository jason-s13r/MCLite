#include "ContactLocation.h"

#include <Arduino.h>
#include <cstring>

#include "../storage/TelemetryCache.h"
#include "../storage/HeardAdvertCache.h"
#include "../mesh/MeshManager.h"
#include "../mesh/MCLiteMesh.h"

namespace mclite {

ContactLocation bestKnownLocation(const uint8_t* pubKey32) {
    ContactLocation r;
    if (!pubKey32) return r;

    // 1) Telemetry — permission-gated, full precision. Wins while fresh.
    auto& tc = TelemetryCache::instance();
    const TelemetryData* td = tc.get(pubKey32);
    if (td && td->hasLocation && tc.isFresh(pubKey32)) {
        r.valid = true;
        r.lat = td->lat;
        r.lon = td->lon;
        r.source = ContactLocation::TELEMETRY;
        r.approximate = false;
        r.hasAge = true;
        r.ageMs = millis() - td->receivedAt;
        return r;
    }

    // 2) The contact's advert GPS (precision unknown -> approximate, no timestamp).
    MCLiteMesh* mesh = MeshManager::instance().mesh();
    if (mesh) {
        int n = mesh->getNumContacts();
        for (int i = 0; i < n; i++) {
            ContactInfo* c = mesh->getContactByIdx(i);
            if (!c || memcmp(c->id.pub_key, pubKey32, 32) != 0) continue;
            if (c->gps_lat || c->gps_lon) {
                r.valid = true;
                r.lat = c->gps_lat / 1e6;
                r.lon = c->gps_lon / 1e6;
                r.source = ContactLocation::ADVERT;
                r.approximate = true;
                r.hasAge = false;   // ContactInfo carries no receive time
            }
            break;  // found the contact; stop regardless
        }
        if (r.valid) return r;
    }

    // 3) A heard advert with GPS (covers non-contacts too).
    auto& hc = HeardAdvertCache::instance();
    const HeardAdvert* es = hc.entries();
    for (int i = 0; i < hc.count(); i++) {
        if (memcmp(es[i].pubKey, pubKey32, 32) == 0 && es[i].hasGps) {
            r.valid = true;
            r.lat = es[i].gpsLat / 1e6;
            r.lon = es[i].gpsLon / 1e6;
            r.source = ContactLocation::HEARD;
            r.approximate = true;
            r.hasAge = true;
            r.ageMs = millis() - es[i].lastHeardMs;
            return r;
        }
    }

    return r;
}

}  // namespace mclite
