#pragma once

// MeshCore Companion Protocol — command / response / push / error codes.
//
// These mirror the official protocol implemented in MeshCore's companion_radio
// example (examples/companion_radio/MyMesh.cpp:6-134, companion-v1.15.0). We
// re-declare only the codes MCLite uses; the example keeps them in a .cpp, not a
// shared header, so there is nothing to include. Frame byte layouts are copied
// from the example's reply builders as each handler is implemented.
//
// Scope: messaging + read-only. All config/radio/contact/channel/key WRITE
// commands are rejected with RESP_CODE_ERR + ERR_CODE_UNSUPPORTED_CMD. A few
// commands are honoured because they only transmit / hold an in-RAM session and
// change no stored config:
//   - CMD_SEND_SELF_ADVERT — re-broadcasts our own advert (same as the on-device
//     button / periodic timer).
//   - CMD_SEND_TELEMETRY_REQ — asks a contact for telemetry over the mesh (same
//     as the on-device chat-header telemetry button); gated by the existing
//     messaging.request_telemetry flag.
//   - CMD_SEND_LOGIN — logs into an already-configured room/repeater over the mesh
//     (same as the on-device room login); session is RAM-only, no config write.
//   - CMD_SHARE_CONTACT — re-broadcasts a known contact's advert at zero hop (same as the
//     on-device Share button); transmits only, gated by messaging.share_contact.
//   - CMD_REBOOT — reboots the device (a power-cycle; no stored-state change). No response.
// Honored conversation-management *writes* (mutate stored config), gated by
// permissions.conversation_management, mirroring on-device add/remove:
//   - CMD_SET_CHANNEL — add a channel (or remove it, via an empty name). Adding applies LIVE
//     (MeshCore addChannel registers at runtime; the app's immediate GET_CHANNEL returns the
//     real key). Removing reboots to apply — MeshCore has no runtime channel removal (no
//     removeChannel; channels[] is private), so the live table is rebuilt from config at boot.
//   - CMD_ADD_UPDATE_CONTACT — add a new contact, or edit an existing one's display name.
//     Applies LIVE with no reboot (MeshCore addContact + ContactStore renders the alias), so
//     the session stays connected. Editing maps to the display name only: per-contact
//     permission flags are device-owner policy (not in the app's contact-settings model) and
//     the app's flags byte is app-local.
//   - CMD_REMOVE_CONTACT — remove a contact (plus its chat history and any held advert).
//     Reboots to apply, matching channel removal — the uniform model is: adding/editing is
//     live, removing reboots (the app reconnects).
// Honored device-settings *writes*, gated by permissions.settings == "full" (the same gate the
// on-device Admin uses; "restricted"/"none" reject):
//   - CMD_SET_ADVERT_NAME — set the device/advert name (cfg.deviceName). Live: the name is read
//     fresh from config on the next advert, so no reboot.
//   - CMD_SET_RADIO_PARAMS — freq/SF/BW/CR (validated). The repeater flag is rejected (MCLite
//     isn't a repeater). Reboots to apply (radio re-inits from config at boot).
//   - CMD_SET_RADIO_TX_POWER — TX power dBm (validated). Reboots to apply.
//   - CMD_SET_DEVICE_PIN — BLE pairing PIN (0 or 6 digits; 0 regenerates). Reboots (BLE re-init).
//   - CMD_SET_PATH_HASH_MODE — 1/2/3 bytes per hop (cfg.radio.pathHashMode). Reboots to apply.
//   - CMD_SET_DEFAULT_FLOOD_SCOPE — persistent region/flood-scope (name+key). Stored as the
//     region string (cfg.radio.scope) when the key matches the name's derived key (public
//     '#'-region); reboots to apply. CMD_GET_DEFAULT_FLOOD_SCOPE reads it back.
//   - CMD_SET_FLOOD_SCOPE_KEY — session-only flood-scope override (raw key); live, not persisted.
// Deliberately rejected (fall through to UNSUPPORTED_CMD), conflicting with MCLite's design:
//   SET_ADVERT_LATLON(14) [GPS-based location], SET_TUNING_PARAMS(21), SET_OTHER_PARAMS(38) +
//   SET_AUTOADD_CONFIG(58/59) [fixed manual-add + per-contact telemetry], EXPORT/IMPORT_PRIVATE_KEY
//   (23/24), FACTORY_RESET(51), repeater/custom-vars, signing, raw datagrams. See companion-commands.md.

#include <helpers/BaseSerialInterface.h>   // MAX_FRAME_SIZE (172)

namespace mclite {

// ---- Commands (app -> firmware), cmd_frame[0] ----
enum : uint8_t {
    CMD_APP_START              = 1,
    CMD_SEND_TXT_MSG           = 2,
    CMD_SEND_CHANNEL_TXT_MSG   = 3,
    CMD_GET_CONTACTS           = 4,   // optional 'since' for incremental sync
    CMD_GET_DEVICE_TIME        = 5,
    CMD_SET_DEVICE_TIME        = 6,
    CMD_SEND_SELF_ADVERT       = 7,   // optional param byte: 1 = flood, 0/absent = zero-hop
    CMD_SYNC_NEXT_MESSAGE      = 10,
    CMD_GET_BATT_AND_STORAGE   = 20,
    CMD_DEVICE_QUERY           = 22,  // (example spells it CMD_DEVICE_QEURY)
    CMD_HAS_CONNECTION         = 28,
    CMD_LOGOUT                 = 29,
    CMD_GET_CONTACT_BY_KEY     = 30,
    CMD_GET_CHANNEL            = 31,
    CMD_SET_CHANNEL            = 0x20,  // [1]=idx(0-7) [2..33]=name(32) [34..49]=16-byte secret; empty name = remove
    CMD_ADD_UPDATE_CONTACT     = 9,   // [1..32]=pubkey [33]=type [34]=flags [35]=out_path_len [36..99]=out_path(64) [100..131]=adv_name(32) [132..135]=last_advert [136..143]=lat/lon(opt)
    CMD_REMOVE_CONTACT         = 15,  // [1..32]=32-byte contact pubkey
    CMD_SHARE_CONTACT          = 16,  // [1..32]=32-byte contact pubkey; re-broadcast its advert (zero hop)
    CMD_REBOOT                 = 19,  // [1..]="reboot"; reboot the device (no response)
    CMD_SEND_LOGIN             = 26,  // [1..32]=32-byte room/repeater pubkey [33..]=password (<=15)
    CMD_SEND_TELEMETRY_REQ     = 39,  // [1..3]=reserved [4..35]=32-byte contact pubkey
    // Device-settings writes (gate: permissions.settings == "full")
    CMD_SET_ADVERT_NAME        = 8,   // [1..]=name (<=20)
    CMD_SET_RADIO_PARAMS       = 11,  // [1..4]=freq kHz u32 [5..8]=bw Hz u32 [9]=sf [10]=cr [11]=repeat(opt, rejected if 1)
    CMD_SET_RADIO_TX_POWER     = 12,  // [1]=int8 dBm
    CMD_SET_DEVICE_PIN         = 37,  // [1..4]=u32 PIN (0 or 100000-999999; 0 = regenerate)
    CMD_SET_PATH_HASH_MODE     = 61,  // [1]=0 (reserved) [2]=mode (0/1/2 -> 1/2/3 bytes per hop)
    CMD_SET_FLOOD_SCOPE_KEY    = 54,  // [1]=0 (reserved) [2..17]=16-byte key (absent = null); session-only
    CMD_SET_DEFAULT_FLOOD_SCOPE = 63, // [1..31]=name [32..47]=16-byte key (len==1 = clear); persistent region
    CMD_GET_DEFAULT_FLOOD_SCOPE = 64, // -> RESP_CODE_DEFAULT_FLOOD_SCOPE
    CMD_RESET_PATH             = 13,  // [1..32]=contact pubkey; reset its learned path (flood rediscover)
    CMD_EXPORT_CONTACT         = 17,  // [1..32]=pubkey (absent = self) -> RESP_CODE_EXPORT_CONTACT (advert blob)
    CMD_IMPORT_CONTACT         = 18,  // [1..]=advert blob; inject -> lands in Heard Adverts (curated)
    CMD_GET_AUTOADD_CONFIG     = 59,  // -> RESP_CODE_AUTOADD_CONFIG (MCLite always reports auto-add off)
};

// ---- Responses (firmware -> app), out_frame[0] ----
enum : uint8_t {
    RESP_CODE_OK               = 0,
    RESP_CODE_ERR              = 1,
    RESP_CODE_CONTACTS_START   = 2,
    RESP_CODE_CONTACT          = 3,
    RESP_CODE_END_OF_CONTACTS  = 4,
    RESP_CODE_SELF_INFO        = 5,   // reply to CMD_APP_START
    RESP_CODE_SENT             = 6,
    RESP_CODE_CONTACT_MSG_RECV = 7,   // sync reply (app ver < 3)
    RESP_CODE_CHANNEL_MSG_RECV = 8,   // sync reply (app ver < 3)
    RESP_CODE_CURR_TIME        = 9,   // reply to CMD_GET_DEVICE_TIME
    RESP_CODE_NO_MORE_MESSAGES = 10,
    RESP_CODE_BATT_AND_STORAGE = 12,
    RESP_CODE_DEVICE_INFO      = 13,  // reply to CMD_DEVICE_QUERY
    RESP_CODE_CONTACT_MSG_RECV_V3 = 16,  // sync reply (app ver >= 3)
    RESP_CODE_CHANNEL_MSG_RECV_V3 = 17,  // sync reply (app ver >= 3)
    RESP_CODE_CHANNEL_INFO     = 18,  // reply to CMD_GET_CHANNEL
    RESP_CODE_DEFAULT_FLOOD_SCOPE = 28,  // reply to CMD_GET_DEFAULT_FLOOD_SCOPE: [1..31]=name [32..47]=key
    RESP_CODE_EXPORT_CONTACT   = 11,  // reply to CMD_EXPORT_CONTACT: [1..]=advert blob
    RESP_CODE_AUTOADD_CONFIG   = 25,  // reply to CMD_GET_AUTOADD_CONFIG: [1]=config [2]=max_hops
};

// ---- Push codes (unsolicited firmware -> app), out_frame[0] ----
enum : uint8_t {
    PUSH_CODE_SEND_CONFIRMED   = 0x82,
    PUSH_CODE_MSG_WAITING      = 0x83,
    PUSH_CODE_LOGIN_SUCCESS    = 0x85,  // [1]=perms [2..7]=prefix [8..11]=tag [12]=v7 ACL perms [13]=fw level
    PUSH_CODE_LOGIN_FAIL       = 0x86,  // [1]=reserved [2..7]=pubkey prefix
    PUSH_CODE_TELEMETRY_RESPONSE = 0x8B,  // [1]=reserved [2..7]=pubkey prefix [8..]=raw LPP
};

// ---- Error codes (second byte after RESP_CODE_ERR) ----
enum : uint8_t {
    ERR_CODE_UNSUPPORTED_CMD   = 1,
    ERR_CODE_NOT_FOUND         = 2,
    ERR_CODE_TABLE_FULL        = 3,
    ERR_CODE_BAD_STATE         = 4,
    ERR_CODE_FILE_IO_ERROR     = 5,
    ERR_CODE_ILLEGAL_ARG       = 6,
};

// Firmware/protocol version code we advertise in RESP_CODE_DEVICE_INFO. Matches
// the companion-v1.15.0 protocol level so clients negotiate the v3 message
// formats we support; the actual negotiated app version is tracked per session.
static constexpr uint8_t COMPANION_FW_VER_CODE = 11;

// Advert type this node identifies as (ADV_TYPE_CHAT in AdvertDataHelpers.h).
static constexpr uint8_t COMPANION_ADV_TYPE_CHAT = 1;

// Text message type (TxtDataHelpers.h). We only accept plain text.
static constexpr uint8_t COMPANION_TXT_TYPE_PLAIN = 0;

}  // namespace mclite
