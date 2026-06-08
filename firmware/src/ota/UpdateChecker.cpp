#include "UpdateChecker.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config/defaults.h"

namespace mclite {

// arduino-esp32 embeds a Mozilla root-CA bundle; this symbol points at it.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

static const char* API_URL =
    "https://api.github.com/repos/" MCLITE_REPO_OWNER "/" MCLITE_REPO_NAME "/releases/latest";

#ifdef PLATFORM_TWATCH
static const char* ASSET_PREFIX = "mclite-watch-v";
#else
static const char* ASSET_PREFIX = "mclite-v";
#endif

bool UpdateChecker::checkLatest(RemoteRelease& out) {
    WiFiClientSecure client;
    client.setCACertBundle(rootca_crt_bundle_start);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setUserAgent("MCLite");                       // GitHub 403s without a UA
    if (!http.begin(client, API_URL)) return false;
    http.addHeader("Accept", "application/vnd.github+json");

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Update] GitHub API HTTP %d\n", code);
        http.end();
        return false;
    }

    // Read the whole (de-chunked) body first — parsing directly off the TLS
    // stream gives IncompleteInput when data arrives in bursts. The filter keeps
    // the parsed document small even though the raw String holds the response.
    String payload = http.getString();
    http.end();
    if (payload.length() == 0) {
        Serial.println("[Update] empty response body");
        return false;
    }

    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload,
                                               DeserializationOption::Filter(filter));
    if (err) {
        Serial.printf("[Update] JSON parse: %s\n", err.c_str());
        return false;
    }

    String version = doc["tag_name"] | "";            // e.g. "v0.2.1"
    if (version.startsWith("v") || version.startsWith("V")) version = version.substring(1);

    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
        String name = asset["name"] | "";
        if (name.startsWith(ASSET_PREFIX) && name.endsWith(".bin")) {
            out.version = version.length()
                              ? version
                              : name.substring(strlen(ASSET_PREFIX), name.length() - 4);
            out.url = asset["browser_download_url"] | "";
            return out.url.length() > 0;
        }
    }
    Serial.println("[Update] no board-matching asset in latest release");
    return false;
}

}  // namespace mclite
