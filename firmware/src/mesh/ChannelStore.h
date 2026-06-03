#pragma once

#include <vector>
#include <cstdint>
#include <Arduino.h>

namespace mclite {

enum class ChannelType : uint8_t {
    HASHTAG,   // Open group channel (#)
    PRIVATE    // PSK-protected channel (lock icon)
};

struct Channel {
    String      name;
    ChannelType type;
    uint8_t     psk[32];     // Pre-shared key bytes (16 or 32, zero-padded)
    uint8_t     pskLen = 0;  // Actual key length (16 or 32)
    String      pskB64;      // Base64 encoded (what gets passed to MeshCore)
    uint8_t     index;       // Channel index in MeshCore

    bool    allowSos = true;  // Allow SOS alerts from this channel
    bool    sendSos = true;   // Include in outgoing SOS broadcast
    bool    readOnly = false;  // Hide input bar in chat view
    String  scope;             // Region scope override ("" = inherit global)
    bool    custom = false;    // true = added via admin screen (deletable)

    bool isPrivate() const { return type == ChannelType::PRIVATE; }
};

class ChannelStore {
public:
    void loadFromConfig();
    void loadCustomChannels();   // /mclite/custom/channels.json
    void addChannel(const Channel& ch);

    // Derive a hashtag channel (name must start with '#').
    // Returns nullptr if name is invalid or already exists.
    Channel* deriveHashtagChannel(const String& name);

    // Derive with explicit flags (used by AdminScreen with checkboxes).
    Channel* deriveHashtagChannel(const String& name, bool allowSos, bool sendSos, bool readOnly);

    // Remove a channel by name. Only removes custom channels. Returns true if removed.
    bool removeChannelByName(const String& name);

    // Save custom channels to /mclite/custom/channels.json
    bool saveCustomChannels();

    Channel* findByName(const String& name);
    Channel* findByIndex(uint8_t index);

    size_t count() const { return _channels.size(); }
    const std::vector<Channel>& all() const { return _channels; }

    static ChannelStore& instance();

private:
    ChannelStore() = default;
    std::vector<Channel> _channels;
    size_t _builtinCount = 0;   // number of channels loaded from config.json
};

}  // namespace mclite
