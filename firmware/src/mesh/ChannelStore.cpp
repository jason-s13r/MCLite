#include "ChannelStore.h"
#include "util/log.h"
#include "../config/ConfigManager.h"
#include "../storage/SDCard.h"
#include "../util/hex.h"
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <cstring>

namespace mclite {

ChannelStore& ChannelStore::instance() {
    static ChannelStore inst;
    return inst;
}

// Encode raw PSK bytes to base64 string
static String pskToBase64(const uint8_t* psk, size_t len) {
    char b64[64];
    size_t b64Len = 0;
    mbedtls_base64_encode((uint8_t*)b64, sizeof(b64), &b64Len, psk, len);
    return String(b64, b64Len);
}

void ChannelStore::loadFromConfig() {
    _channels.clear();
    _builtinCount = 0;
    const auto& cfg = ConfigManager::instance().config();

    for (const auto& cc : cfg.channels) {
        Channel ch;
        ch.name  = cc.name;
        ch.type  = (cc.type == "private") ? ChannelType::PRIVATE : ChannelType::HASHTAG;
        ch.index   = cc.index;
        ch.allowSos = cc.allowSos;
        ch.sendSos = cc.sendSos;
        ch.readOnly = cc.readOnly;
        ch.scope    = cc.scope;
        ch.custom   = false;   // <-- built-in channels are not custom
        memset(ch.psk, 0, sizeof(ch.psk));

        if (cc.psk.length() > 0) {
            // Decode PSK — supports hex (32 or 64 chars) and base64 (legacy)
            bool pskOk = false;
            size_t outLen = 0;

            if ((cc.psk.length() == 32 || cc.psk.length() == 64) && isHexString(cc.psk)) {
                // Hex format
                outLen = cc.psk.length() / 2;
                for (size_t i = 0; i < outLen; i++) {
                    char byte[3] = { cc.psk[i*2], cc.psk[i*2+1], 0 };
                    ch.psk[i] = (uint8_t)strtoul(byte, nullptr, 16);
                }
                pskOk = true;
            } else {
                // Base64 format (legacy)
                int ret = mbedtls_base64_decode(ch.psk, sizeof(ch.psk), &outLen,
                                      (const uint8_t*)cc.psk.c_str(),
                                      cc.psk.length());
                pskOk = (ret == 0 && (outLen == 16 || outLen == 32));
            }

            if (!pskOk) {
                LOGF("[ChannelStore] Skipping channel '%s': invalid PSK\n",
                              cc.name.c_str());
                continue;
            }
            ch.pskLen = (uint8_t)outLen;
            ch.pskB64 = pskToBase64(ch.psk, outLen);
        } else if (cc.name.length() > 0 && cc.name[0] == '#') {
            // Hashtag channel: derive PSK as SHA256(name)[:16]
            // Name includes the '#' prefix, matching MeshCore convention
            // Sanitize: lowercase + strip invalid chars (only a-z, 0-9, - allowed after #).
            // MeshCore official apps only allow lowercase hashtag names.
            // If a future MeshCore version allows uppercase, remove this normalization.
            String normalized;
            for (size_t j = 0; j < cc.name.length(); j++) {
                char c = tolower(cc.name[j]);
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '#')
                    normalized += c;
            }
            if (normalized.length() <= 1) {  // just '#' or empty
                LOGF("[ChannelStore] Skipping hashtag channel '%s': invalid name\n", cc.name.c_str());
                continue;
            }
            ch.name = normalized;
            uint8_t hash[32];
            mbedtls_sha256((const uint8_t*)normalized.c_str(), normalized.length(), hash, 0);
            memcpy(ch.psk, hash, 16);
            memset(ch.psk + 16, 0, 16);
            ch.pskLen = 16;
            ch.pskB64 = pskToBase64(ch.psk, 16);
            LOGF("[ChannelStore] Derived PSK for hashtag channel '%s'\n", cc.name.c_str());
        } else {
            LOGF("[ChannelStore] Skipping channel '%s': no PSK provided\n", cc.name.c_str());
            continue;
        }

        _channels.push_back(ch);
    }

    _builtinCount = _channels.size();
    LOGF("[ChannelStore] Loaded %u built-in channels\n", (unsigned)_builtinCount);
}

// Helper: normalize a hashtag name (lowercase, strip invalid chars)
static String normalizeHashtagName(const String& name) {
    String normalized;
    for (size_t j = 0; j < name.length(); j++) {
        char c = tolower(name[j]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '#')
            normalized += c;
    }
    return normalized;
}

void ChannelStore::loadCustomChannels() {
    String json = SDCard::instance().readFile("/mclite/custom/channels.json");
    if (json.length() == 0) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        LOGF("[ChannelStore] Custom channels parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    if (!arr) return;

    for (JsonObject ch : arr) {
        ChannelConfig cc;
        cc.name     = ch["name"] | "";
        cc.type     = ch["type"] | "hashtag";
        cc.psk      = ch["psk"] | "";
        cc.index    = ch["index"] | 0;
        cc.allowSos = ch["allow_sos"] | false;
        cc.sendSos  = ch["send_sos"] | false;
        cc.readOnly = ch["read_only"] | false;
        cc.scope    = ch["scope"] | "";

        if (cc.name.length() == 0) continue;

        // Skip duplicates (already loaded from config.json)
        if (findByName(cc.name)) continue;

        Channel channel;
        channel.name  = cc.name;
        channel.type  = (cc.type == "private") ? ChannelType::PRIVATE : ChannelType::HASHTAG;
        channel.index = cc.index;
        channel.allowSos = cc.allowSos;
        channel.sendSos  = cc.sendSos;
        channel.readOnly = cc.readOnly;
        channel.scope    = cc.scope;
        channel.custom   = true;   // <-- mark as custom
        memset(channel.psk, 0, sizeof(channel.psk));

        if (cc.psk.length() > 0) {
            bool pskOk = false;
            size_t outLen = 0;
            if ((cc.psk.length() == 32 || cc.psk.length() == 64) && isHexString(cc.psk)) {
                outLen = cc.psk.length() / 2;
                for (size_t i = 0; i < outLen; i++) {
                    char byte[3] = { cc.psk[i*2], cc.psk[i*2+1], 0 };
                    channel.psk[i] = (uint8_t)strtoul(byte, nullptr, 16);
                }
                pskOk = true;
            } else {
                int ret = mbedtls_base64_decode(channel.psk, sizeof(channel.psk), &outLen,
                                      (const uint8_t*)cc.psk.c_str(), cc.psk.length());
                pskOk = (ret == 0 && (outLen == 16 || outLen == 32));
            }
            if (!pskOk) {
                LOGF("[ChannelStore] Skipping custom channel '%s': invalid PSK\n",
                              cc.name.c_str());
                continue;
            }
            channel.pskLen = (uint8_t)outLen;
            channel.pskB64 = pskToBase64(channel.psk, outLen);
        } else if (cc.name.length() > 0 && cc.name[0] == '#') {
            String normalized;
            for (size_t j = 0; j < cc.name.length(); j++) {
                char c = tolower(cc.name[j]);
                if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '#')
                    normalized += c;
            }
            if (normalized.length() <= 1) continue;
            channel.name = normalized;
            uint8_t hash[32];
            mbedtls_sha256((const uint8_t*)normalized.c_str(), normalized.length(), hash, 0);
            memcpy(channel.psk, hash, 16);
            memset(channel.psk + 16, 0, 16);
            channel.pskLen = 16;
            channel.pskB64 = pskToBase64(channel.psk, 16);
        } else {
            continue;
        }

        _channels.push_back(channel);
    }

    LOGF("[ChannelStore] Loaded %u custom channels\n",
                  (unsigned)(_channels.size() - _builtinCount));
}

Channel* ChannelStore::deriveHashtagChannel(const String& name) {
    return deriveHashtagChannel(name, false, false, false);
}

Channel* ChannelStore::deriveHashtagChannel(const String& name, bool allowSos, bool sendSos, bool readOnly) {
    if (name.length() == 0 || name[0] != '#') return nullptr;

    String normalized = normalizeHashtagName(name);
    if (normalized.length() <= 1) return nullptr;

    // Prevent duplicates
    if (findByName(normalized)) return nullptr;

    // Find next available index
    uint8_t nextIndex = 0;
    for (const auto& ch : _channels) {
        if (ch.index >= nextIndex) nextIndex = ch.index + 1;
    }

    Channel ch;
    ch.name = normalized;
    ch.type = ChannelType::HASHTAG;
    ch.index = nextIndex;
    ch.allowSos = allowSos;
    ch.sendSos = sendSos;
    ch.readOnly = readOnly;
    ch.scope = "";
    ch.custom = true;   // <-- mark as custom
    memset(ch.psk, 0, sizeof(ch.psk));

    uint8_t hash[32];
    mbedtls_sha256((const uint8_t*)normalized.c_str(), normalized.length(), hash, 0);
    memcpy(ch.psk, hash, 16);
    ch.pskLen = 16;
    ch.pskB64 = pskToBase64(ch.psk, 16);

    _channels.push_back(ch);
    LOGF("[ChannelStore] Derived PSK for hashtag channel '%s' (index=%d, allowSos=%d, sendSos=%d, readOnly=%d)\n",
                  normalized.c_str(), nextIndex, (int)allowSos, (int)sendSos, (int)readOnly);
    return &_channels.back();
}

bool ChannelStore::removeChannelByName(const String& name) {
    for (auto it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->name == name) {
            if (!it->custom) {
                LOGF("[ChannelStore] Cannot remove built-in channel '%s'\n", name.c_str());
                return false;
            }
            _channels.erase(it);
            return true;
        }
    }
    return false;
}

bool ChannelStore::saveCustomChannels() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (const auto& ch : _channels) {
        if (!ch.custom) continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = ch.name;
        obj["type"] = (ch.type == ChannelType::PRIVATE) ? "private" : "hashtag";
        // Encode PSK as hex for readability
        char hex[65];
        for (int i = 0; i < ch.pskLen; i++) {
            sprintf(hex + i * 2, "%02x", ch.psk[i]);
        }
        hex[ch.pskLen * 2] = '\0';
        obj["psk"] = String(hex);
        obj["index"] = ch.index;
        if (ch.allowSos) obj["allow_sos"] = true;
        if (ch.sendSos) obj["send_sos"] = true;
        if (ch.readOnly) obj["read_only"] = true;
        if (ch.scope.length() > 0) obj["scope"] = ch.scope;
    }

    String output;
    serializeJson(doc, output);

    // Ensure directory exists
    SDCard::instance().mkdir("/mclite/custom");
    if (!SDCard::instance().writeAtomic("/mclite/custom/channels.json", output)) {
        LOGLN("[ChannelStore] saveCustomChannels: write failed");
        return false;
    }
    LOGF("[ChannelStore] Saved %u custom channels\n", (unsigned)arr.size());
    return true;
}

Channel* ChannelStore::findByName(const String& name) {
    for (auto& ch : _channels) {
        if (ch.name == name) return &ch;
    }
    return nullptr;
}

Channel* ChannelStore::findByIndex(uint8_t index) {
    for (auto& ch : _channels) {
        if (ch.index == index) return &ch;
    }
    return nullptr;
}

}  // namespace mclite
