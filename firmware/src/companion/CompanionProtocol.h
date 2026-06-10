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
// commands are rejected with RESP_CODE_ERR + ERR_CODE_UNSUPPORTED_CMD.

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
    CMD_SYNC_NEXT_MESSAGE      = 10,
    CMD_GET_BATT_AND_STORAGE   = 20,
    CMD_DEVICE_QUERY           = 22,  // (example spells it CMD_DEVICE_QEURY)
    CMD_HAS_CONNECTION         = 28,
    CMD_LOGOUT                 = 29,
    CMD_GET_CONTACT_BY_KEY     = 30,
    CMD_GET_CHANNEL            = 31,
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
};

// ---- Push codes (unsolicited firmware -> app), out_frame[0] ----
enum : uint8_t {
    PUSH_CODE_SEND_CONFIRMED   = 0x82,
    PUSH_CODE_MSG_WAITING      = 0x83,
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
