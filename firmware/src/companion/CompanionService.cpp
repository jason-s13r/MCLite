#include "CompanionService.h"
#include "util/log.h"
#include "CompanionProtocol.h"

#include "../config/ConfigManager.h"
#include "../config/defaults.h"
#include "../mesh/MeshManager.h"
#include "../mesh/MCLiteMesh.h"   // mesh() reads: contacts, channels, RTC
#include "../mesh/ContactStore.h"
#include "../ui/UIManager.h"      // handleSend() — send + on-device parity
#include "../storage/MessageStore.h"  // ConvoId
#include "../hal/Battery.h"
#include "../storage/SDCard.h"

#include <MeshCore.h>   // PUB_KEY_SIZE, MAX_PATH_SIZE
#include <helpers/ContactInfo.h>
#include <SD.h>
#include <esp_random.h>
#include <cstring>

namespace mclite {

#ifdef PLATFORM_TWATCH
static const char* MANUFACTURER = "LilyGo T-Watch Ultra";
#else
static const char* MANUFACTURER = "LilyGo T-Deck Plus";
#endif

CompanionService& CompanionService::instance() {
    static CompanionService inst;
    return inst;
}

void CompanionService::begin(BaseSerialInterface* iface) {
    if (_iface == iface) return;
    if (_iface) end();
    _iface = iface;
    _appVer = 0;
    if (_iface) _iface->enable();
    LOGLN("[Companion] interface enabled");
}

void CompanionService::end() {
    if (!_iface) return;
    _iface->disable();
    _iface = nullptr;
    _appVer = 0;
    LOGLN("[Companion] interface disabled");
}

uint32_t CompanionService::ensureBlePin() {
    auto& cfg = ConfigManager::instance().config();
    if (cfg.ble.pin < 100000 || cfg.ble.pin > 999999) {
        cfg.ble.pin = 100000 + (esp_random() % 900000);   // random 6-digit passkey
        ConfigManager::instance().save();
        LOGF("[Companion] generated BLE pairing PIN %06lu\n", (unsigned long)cfg.ble.pin);
    }
    return cfg.ble.pin;
}

void CompanionService::loop() {
    if (!_iface) return;

    // Reset per-session state when a client drops, so stale queued frames and a
    // stale negotiated version never leak into the next connection.
    bool conn = _iface->isConnected();
    if (!conn && _wasConnected) { _offlineLen = 0; _contactsIterating = false; _appVer = 0; }
    _wasConnected = conn;

    size_t len = _iface->checkRecvFrame(_cmd);
    if (len > 0) { handleFrame(len); return; }
    // No inbound frame — drive any in-progress GET_CONTACTS stream, one frame per
    // tick, while the transport's send queue has room.
    if (_contactsIterating && !_iface->isWriteBusy()) pumpContacts();
}

void CompanionService::handleFrame(size_t len) {
    const uint8_t cmd = _cmd[0];
    switch (cmd) {
        case CMD_APP_START:        cmdAppStart(len);        break;
        case CMD_DEVICE_QUERY:     cmdDeviceQuery(len);     break;
        case CMD_SEND_TXT_MSG:     cmdSendTxtMsg(len);      break;
        case CMD_SEND_CHANNEL_TXT_MSG: cmdSendChannelTxtMsg(len); break;
        case CMD_GET_DEVICE_TIME:  cmdGetDeviceTime();      break;
        case CMD_SET_DEVICE_TIME:  cmdSetDeviceTime(len);   break;
        case CMD_SEND_SELF_ADVERT: cmdSendSelfAdvert(len);  break;
        case CMD_GET_BATT_AND_STORAGE: cmdGetBattAndStorage(); break;
        case CMD_GET_CONTACTS:     cmdGetContacts(len);     break;
        case CMD_GET_CONTACT_BY_KEY: cmdGetContactByKey(len); break;
        case CMD_GET_CHANNEL:      cmdGetChannel(len);      break;
        case CMD_SYNC_NEXT_MESSAGE: cmdSyncNextMessage();   break;
        case CMD_LOGOUT:           writeOK();               break;   // no room sessions yet
        case CMD_HAS_CONNECTION:   writeErr(ERR_CODE_NOT_FOUND); break;
        // Everything else — including all config/radio/contact/channel/key WRITE
        // commands — is refused (messaging + read-only scope).
        default:                   writeErr(ERR_CODE_UNSUPPORTED_CMD); break;
    }
}

// CMD_APP_START -> RESP_CODE_SELF_INFO
// Layout mirrors MyMesh.cpp:1019-1057 (companion-v1.15.0).
void CompanionService::cmdAppStart(size_t len) {
    if (len < 8) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }

    const auto& radio = ConfigManager::instance().config().radio;
    const String& name = ConfigManager::instance().config().deviceName;
    const uint8_t* pubkey = MeshManager::instance().selfPubKey();
    if (!pubkey) { writeErr(ERR_CODE_BAD_STATE); return; }

    int i = 0;
    _out[i++] = RESP_CODE_SELF_INFO;
    _out[i++] = COMPANION_ADV_TYPE_CHAT;
    _out[i++] = (uint8_t)radio.txPower;
    _out[i++] = (uint8_t)LORA_TX_POWER;          // max TX power
    memcpy(&_out[i], pubkey, PUB_KEY_SIZE); i += PUB_KEY_SIZE;

    int32_t lat = 0, lon = 0;                    // MCLite does not advertise location
    memcpy(&_out[i], &lat, 4); i += 4;
    memcpy(&_out[i], &lon, 4); i += 4;

    _out[i++] = 0;   // multi_acks (v7+)
    _out[i++] = 0;   // advert_loc_policy
    _out[i++] = 0;   // telemetry mode bits (v5+)
    _out[i++] = 1;   // manual_add_contacts (MCLite manages contacts via config)

    uint32_t freq = (uint32_t)(radio.frequency * 1000.0f);   // kHz
    memcpy(&_out[i], &freq, 4); i += 4;
    uint32_t bw = (uint32_t)(radio.bandwidth * 1000.0f);     // Hz
    memcpy(&_out[i], &bw, 4); i += 4;
    _out[i++] = radio.spreadingFactor;
    _out[i++] = radio.codingRate;

    int tlen = name.length();
    if (i + tlen > MAX_FRAME_SIZE) tlen = MAX_FRAME_SIZE - i;
    memcpy(&_out[i], name.c_str(), tlen); i += tlen;

    _iface->writeFrame(_out, i);
    LOGF("[Companion] APP_START -> SELF_INFO (%d B)\n", i);

    // App is now fully connected (DEVICE_QUERY set _appVer first) — replay history.
    backfillHistory();
}

// CMD_DEVICE_QUERY -> RESP_CODE_DEVICE_INFO
// Layout mirrors MyMesh.cpp:996-1014 (companion-v1.15.0).
void CompanionService::cmdDeviceQuery(size_t len) {
    if (len >= 2) _appVer = _cmd[1];   // protocol version the app understands

    const auto& radio = ConfigManager::instance().config().radio;

    int i = 0;
    _out[i++] = RESP_CODE_DEVICE_INFO;
    _out[i++] = COMPANION_FW_VER_CODE;
    _out[i++] = MAX_CONTACTS / 2;        // v3+
    _out[i++] = MAX_GROUP_CHANNELS;      // v3+

    uint32_t blePin = 0;                 // no BLE auth on the WiFi transport
    memcpy(&_out[i], &blePin, 4); i += 4;

    memset(&_out[i], 0, 12);
    strncpy((char*)&_out[i], __DATE__, 12);   // build date (12 B field)
    i += 12;

    memset(&_out[i], 0, 40);
    strncpy((char*)&_out[i], MANUFACTURER, 40);
    i += 40;

    memset(&_out[i], 0, 20);
    {
        char ver[20];
        snprintf(ver, sizeof(ver), "v%s", MCLITE_VERSION);
        strncpy((char*)&_out[i], ver, 20);
    }
    i += 20;

    _out[i++] = 0;                       // client_repeat (v9+)
    _out[i++] = radio.pathHashMode;      // path_hash_mode (v10+)

    _iface->writeFrame(_out, i);
    LOGF("[Companion] DEVICE_QUERY (appVer=%u) -> DEVICE_INFO (%d B)\n", _appVer, i);
}

// CMD_SEND_TXT_MSG -> RESP_CODE_SENT (DM). Layout: [1]=txt_type [2]=attempt
// [3..6]=timestamp [7..12]=6-byte pubkey prefix [13..]=text. (MyMesh.cpp:1057+)
// Routes through MeshManager::sendMessage so MCLite's ACK/retry pipeline tracks
// it; the returned packetId doubles as the protocol's expected_ack token.
void CompanionService::cmdSendTxtMsg(size_t len) {
    if (len < 14) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }

    uint8_t txt_type = _cmd[1];
    if (txt_type != COMPANION_TXT_TYPE_PLAIN) { writeErr(ERR_CODE_UNSUPPORTED_CMD); return; }
    const uint8_t* prefix = &_cmd[7];   // 6-byte pubkey prefix

    // Resolve the recipient: mesh lookup gives the full pubkey, then map to the
    // MCLite ContactStore for its conversation shortId.
    ContactInfo* c = mesh->lookupContactByPubKey(prefix, 6);
    if (!c) { writeErr(ERR_CODE_NOT_FOUND); return; }
    Contact* sc = ContactStore::instance().findByPublicKey(c->id.pub_key);
    if (!sc) { writeErr(ERR_CODE_NOT_FOUND); return; }

    _cmd[len] = 0;   // null-terminate text (buffer is MAX_FRAME_SIZE+1)
    const char* text = (const char*)&_cmd[13];

    // Route through UIManager so the DM is sent AND mirrored on-device (TX parity).
    ConvoId id{ConvoId::DM, sc->shortId()};
    uint32_t packetId = UIManager::instance().handleSend(id, String(text));
    if (packetId == 0) { writeErr(ERR_CODE_TABLE_FULL); return; }

    noteSent(packetId);                 // track for the ACK -> SEND_CONFIRMED bridge

    uint32_t expected_ack = packetId;   // opaque token; matched by SEND_CONFIRMED push
    uint32_t est_timeout  = 8000;       // rough estimate (ms) for the client's pending UI
    _out[0] = RESP_CODE_SENT;
    _out[1] = 0;                        // direct/flood unknown at this layer
    memcpy(&_out[2], &expected_ack, 4);
    memcpy(&_out[6], &est_timeout, 4);
    _iface->writeFrame(_out, 10);
}

// CMD_SEND_CHANNEL_TXT_MSG -> OK (fire-and-forget). Layout: [1]=txt_type
// [2]=channel_idx [3..6]=timestamp [7..]=text. channel_idx is the mesh channel
// index (as returned by GET_CHANNEL), so send via the base GroupChannel API to
// avoid MCLite's ChannelStore-index remapping.
void CompanionService::cmdSendChannelTxtMsg(size_t len) {
    if (len < 7) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }

    uint8_t txt_type = _cmd[1];
    uint8_t channel_idx = _cmd[2];
    // _cmd[3..6] = app timestamp (ignored — handleSend stamps with bestEpoch)
    if (txt_type != COMPANION_TXT_TYPE_PLAIN) { writeErr(ERR_CODE_UNSUPPORTED_CMD); return; }

    ChannelDetails ch;
    if (!mesh->getChannel(channel_idx, ch)) { writeErr(ERR_CODE_NOT_FOUND); return; }

    _cmd[len] = 0;
    const char* text = (const char*)&_cmd[7];

    // Route through UIManager (by channel name) so it's sent AND mirrored on-device.
    ConvoId id{ConvoId::CHANNEL, String(ch.name)};
    if (UIManager::instance().handleSend(id, String(text))) writeOK();
    else writeErr(ERR_CODE_NOT_FOUND);
}

void CompanionService::noteSent(uint32_t packetId) {
    for (auto& p : _pending) {
        if (!p.active) { p.packetId = packetId; p.sentMs = millis(); p.active = true; return; }
    }
    // Table full — overwrite the oldest so we never leak slots.
    PendingAck* oldest = &_pending[0];
    for (auto& p : _pending) if (p.sentMs < oldest->sentMs) oldest = &p;
    oldest->packetId = packetId; oldest->sentMs = millis(); oldest->active = true;
}

// MeshManager forwards the DM ACK here -> PUSH_CODE_SEND_CONFIRMED (MyMesh.cpp:412-422).
// Carries the same token returned in RESP_CODE_SENT so the client matches it and
// stops re-sending. trip_time lets the client show round-trip latency.
void CompanionService::onAckConfirmed(uint32_t packetId) {
    for (auto& p : _pending) {
        if (p.active && p.packetId == packetId) {
            p.active = false;
            if (_iface && _iface->isConnected()) {
                uint32_t trip = millis() - p.sentMs;
                _out[0] = PUSH_CODE_SEND_CONFIRMED;
                memcpy(&_out[1], &packetId, 4);
                memcpy(&_out[5], &trip, 4);
                _iface->writeFrame(_out, 9);
            }
            return;
        }
    }
}

void CompanionService::onSendFailed(uint32_t packetId) {
    for (auto& p : _pending) {
        if (p.active && p.packetId == packetId) { p.active = false; return; }
    }
}

// CMD_GET_DEVICE_TIME -> RESP_CODE_CURR_TIME (MyMesh.cpp:1209-1214)
void CompanionService::cmdGetDeviceTime() {
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }
    uint32_t now = mesh->getRTCClock()->getCurrentTime();
    _out[0] = RESP_CODE_CURR_TIME;
    memcpy(&_out[1], &now, 4);
    _iface->writeFrame(_out, 5);
}

// CMD_SET_DEVICE_TIME -> OK (only if monotonic-forward). Apps set time on
// connect; this feeds the same RTC clock the mesh stamps messages with.
void CompanionService::cmdSetDeviceTime(size_t len) {
    if (len < 5) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }
    uint32_t secs; memcpy(&secs, &_cmd[1], 4);
    uint32_t curr = mesh->getRTCClock()->getCurrentTime();
    if (secs >= curr) {
        mesh->getRTCClock()->setCurrentTime(secs);
        writeOK();
    } else {
        writeErr(ERR_CODE_ILLEGAL_ARG);
    }
}

// CMD_SEND_SELF_ADVERT -> OK / ERR. Optional param byte (_cmd[1]): 1 = flood,
// 0 or absent = zero-hop (local). Mirrors companion_radio MyMesh.cpp:1222. This
// is the one "write" we honour from the client: it only re-broadcasts our own
// advert — the same thing the on-device button and the periodic timer do — and
// changes no stored config/identity, so it stays within the messaging scope.
void CompanionService::cmdSendSelfAdvert(size_t len) {
    bool flood = (len >= 2 && _cmd[1] == 1);
    if (MeshManager::instance().sendAdvertNow(flood)) {
        writeOK();
    } else {
        writeErr(ERR_CODE_BAD_STATE);  // radio not ready / packet pool exhausted
    }
}

// CMD_GET_BATT_AND_STORAGE -> RESP_CODE_BATT_AND_STORAGE (MyMesh.cpp)
void CompanionService::cmdGetBattAndStorage() {
    int i = 0;
    _out[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t mv = Battery::instance().milliVolts();
    uint32_t usedKb = 0, totalKb = 0;
    if (SDCard::instance().isMounted()) {
        totalKb = (uint32_t)(SD.totalBytes() / 1024ULL);
        usedKb  = (uint32_t)(SD.usedBytes()  / 1024ULL);
    }
    memcpy(&_out[i], &mv, 2);      i += 2;
    memcpy(&_out[i], &usedKb, 4);  i += 4;
    memcpy(&_out[i], &totalKb, 4); i += 4;
    _iface->writeFrame(_out, i);
}

// CMD_GET_CONTACTS -> RESP_CODE_CONTACTS_START, then a CONTACT per tick, then
// END_OF_CONTACTS. Optional 4-byte 'since' (lastmod filter) for incremental sync.
void CompanionService::cmdGetContacts(size_t len) {
    if (_contactsIterating) { writeErr(ERR_CODE_BAD_STATE); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }

    // Read the 4-byte 'since' via memcpy — &_cmd[1] is unaligned, and a raw
    // uint32_t* cast there is UB (and inconsistent with the rest of this file).
    uint32_t since = 0;
    if (len >= 5) memcpy(&since, &_cmd[1], 4);
    _contactsSince = since;

    uint32_t count = (uint32_t)mesh->getNumContacts();
    _out[0] = RESP_CODE_CONTACTS_START;
    memcpy(&_out[1], &count, 4);
    _iface->writeFrame(_out, 5);

    _contactCount      = (int)count;
    _contactCursor     = 0;
    _mostRecentLastmod = 0;
    _contactsIterating = true;
}

void CompanionService::pumpContacts() {
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { _contactsIterating = false; return; }

    while (_contactCursor < _contactCount) {
        ContactInfo* c = mesh->getContactByIdx(_contactCursor++);
        if (!c) continue;
        // Incremental sync ('since' > 0) skips unchanged contacts. A full sync
        // ('since' == 0) returns everything — MCLite's config-registered contacts
        // carry lastmod == 0, so a "<= since" test would wrongly drop them all.
        if (_contactsSince != 0 && c->lastmod <= _contactsSince) continue;
        writeContactFrame(RESP_CODE_CONTACT, *c);
        if (c->lastmod > _mostRecentLastmod) _mostRecentLastmod = c->lastmod;
        return;                                         // one frame per tick
    }
    // Done — terminate the stream.
    _out[0] = RESP_CODE_END_OF_CONTACTS;
    memcpy(&_out[1], &_mostRecentLastmod, 4);
    _iface->writeFrame(_out, 5);
    _contactsIterating = false;
}

// CMD_GET_CONTACT_BY_KEY -> RESP_CODE_CONTACT / ERR_NOT_FOUND
void CompanionService::cmdGetContactByKey(size_t len) {
    if (len < 1 + PUB_KEY_SIZE) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }
    ContactInfo* c = mesh->lookupContactByPubKey(&_cmd[1], PUB_KEY_SIZE);
    if (c) writeContactFrame(RESP_CODE_CONTACT, *c);
    else   writeErr(ERR_CODE_NOT_FOUND);
}

// CMD_GET_CHANNEL -> RESP_CODE_CHANNEL_INFO (MyMesh.cpp:1660-1672)
void CompanionService::cmdGetChannel(size_t len) {
    if (len < 2) { writeErr(ERR_CODE_ILLEGAL_ARG); return; }
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) { writeErr(ERR_CODE_BAD_STATE); return; }
    uint8_t idx = _cmd[1];
    ChannelDetails ch;
    if (!mesh->getChannel(idx, ch)) { writeErr(ERR_CODE_NOT_FOUND); return; }
    int i = 0;
    _out[i++] = RESP_CODE_CHANNEL_INFO;
    _out[i++] = idx;
    memset(&_out[i], 0, 32);
    strncpy((char*)&_out[i], ch.name, 32); i += 32;
    memcpy(&_out[i], ch.channel.secret, 16); i += 16;   // 128-bit PSK only
    _iface->writeFrame(_out, i);
}

// CMD_SYNC_NEXT_MESSAGE -> next queued message, or NO_MORE_MESSAGES.
void CompanionService::cmdSyncNextMessage() {
    if (_offlineLen <= 0) {
        _out[0] = RESP_CODE_NO_MORE_MESSAGES;
        _iface->writeFrame(_out, 1);
        return;
    }
    // Pop the front (FIFO) and send it.
    const OfflineMsg& m = _offline[0];
    _iface->writeFrame(m.buf, m.len);
    _offlineLen--;
    for (int i = 0; i < _offlineLen; i++) _offline[i] = _offline[i + 1];
}

void CompanionService::enqueueOffline(const uint8_t* frame, int len) {
    if (len <= 0 || len > MAX_FRAME_SIZE) return;
    if (_offlineLen >= OFFLINE_QUEUE_SIZE) {
        // Drop the oldest to make room (bounded, never blocks).
        for (int i = 0; i < OFFLINE_QUEUE_SIZE - 1; i++) _offline[i] = _offline[i + 1];
        _offlineLen = OFFLINE_QUEUE_SIZE - 1;
        LOGLN("[Companion] offline queue full — dropped oldest");
    }
    _offline[_offlineLen].len = (uint8_t)len;
    memcpy(_offline[_offlineLen].buf, frame, len);
    _offlineLen++;
}

void CompanionService::tickleMsgWaiting() {
    if (_iface && _iface->isConnected()) {
        uint8_t f = PUSH_CODE_MSG_WAITING;
        _iface->writeFrame(&f, 1);
    }
}

// Build RESP_CODE_CONTACT_MSG_RECV[_V3] into _out (MyMesh::queueMessage).
int CompanionService::buildContactRecvFrame(const uint8_t* senderPubKey, uint32_t timestamp,
                                            const char* text) {
    int i = 0;
    if (_appVer >= 3) {
        _out[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
        _out[i++] = 0;   // SNR*4 (not available here)
        _out[i++] = 0;   // reserved
        _out[i++] = 0;   // reserved
    } else {
        _out[i++] = RESP_CODE_CONTACT_MSG_RECV;
    }
    memcpy(&_out[i], senderPubKey, 6); i += 6;   // 6-byte prefix
    _out[i++] = 0xFF;                             // path_len unknown
    _out[i++] = COMPANION_TXT_TYPE_PLAIN;
    memcpy(&_out[i], &timestamp, 4); i += 4;
    int tlen = strlen(text);
    if (i + tlen > MAX_FRAME_SIZE) tlen = MAX_FRAME_SIZE - i;
    memcpy(&_out[i], text, tlen); i += tlen;
    return i;
}

// Build RESP_CODE_CHANNEL_MSG_RECV[_V3] into _out (MyMesh::onChannelMessageRecv).
int CompanionService::buildChannelRecvFrame(uint8_t meshChannelIdx, uint32_t timestamp,
                                            const char* text) {
    int i = 0;
    if (_appVer >= 3) {
        _out[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
        _out[i++] = 0;   // SNR*4
        _out[i++] = 0;   // reserved
        _out[i++] = 0;   // reserved
    } else {
        _out[i++] = RESP_CODE_CHANNEL_MSG_RECV;
    }
    _out[i++] = meshChannelIdx;
    _out[i++] = 0xFF;                             // path_len unknown
    _out[i++] = COMPANION_TXT_TYPE_PLAIN;
    memcpy(&_out[i], &timestamp, 4); i += 4;
    int tlen = strlen(text);
    if (i + tlen > MAX_FRAME_SIZE) tlen = MAX_FRAME_SIZE - i;
    memcpy(&_out[i], text, tlen); i += tlen;
    return i;
}

// Received DM (live tee) -> queue + tickle. meshChannelIdx matches GET_CHANNEL.
void CompanionService::onContactMessage(const uint8_t* senderPubKey, uint32_t timestamp,
                                        const char* text) {
    if (!clientConnected()) return;   // live-forward only
    enqueueOffline(_out, buildContactRecvFrame(senderPubKey, timestamp, text));
    tickleMsgWaiting();
}

void CompanionService::onChannelMessage(uint8_t meshChannelIdx, uint32_t timestamp,
                                        const char* text) {
    if (!clientConnected()) return;
    enqueueOffline(_out, buildChannelRecvFrame(meshChannelIdx, timestamp, text));
    tickleMsgWaiting();
}

// Replay stored RECEIVED messages so a freshly-connected client shows history,
// not just messages that arrive while connected. Iterates conversations
// (recent first) and enqueues their inbound messages chronologically, bounded by
// the offline queue. Relies on the client deduping by (sender, timestamp).
void CompanionService::backfillHistory() {
    if (!clientConnected()) return;
    _offlineLen = 0;   // start clean for this session

    auto convos = MessageStore::instance().getConversationsSorted();
    int enqueued = 0;
    for (auto* convo : convos) {
        if (!convo || enqueued >= OFFLINE_QUEUE_SIZE) break;

        if (convo->convoId.type == ConvoId::DM) {
            // All inbound messages in a DM are from that one contact.
            Contact* sc = nullptr;
            auto& cs = ContactStore::instance();
            for (size_t k = 0; k < cs.count(); k++) {
                Contact* c = cs.findByIndex(k);
                if (c && c->shortId() == convo->convoId.id) { sc = c; break; }
            }
            if (!sc) continue;
            for (const auto& m : convo->messages) {
                if (m.fromSelf) continue;
                if (enqueued >= OFFLINE_QUEUE_SIZE) break;
                enqueueOffline(_out, buildContactRecvFrame(sc->publicKey, m.timestamp, m.text.c_str()));
                enqueued++;
            }
        } else if (convo->convoId.type == ConvoId::CHANNEL) {
            int meshIdx = channelIdxByName(convo->convoId.id);
            if (meshIdx < 0) continue;
            for (const auto& m : convo->messages) {
                if (m.fromSelf) continue;
                if (enqueued >= OFFLINE_QUEUE_SIZE) break;
                // Reconstruct the on-wire "sender: text" form when we know the sender.
                String wire = m.senderName.length() ? (m.senderName + ": " + m.text) : m.text;
                enqueueOffline(_out, buildChannelRecvFrame((uint8_t)meshIdx, m.timestamp, wire.c_str()));
                enqueued++;
            }
        }
        // ROOM conversations are not exposed over the companion link yet.
    }
    if (enqueued > 0) {
        LOGF("[Companion] backfilled %d message(s)\n", enqueued);
        tickleMsgWaiting();
    }
}

// Map a channel name to its mesh channel index (as used by GET_CHANNEL).
int CompanionService::channelIdxByName(const String& name) {
    auto* mesh = MeshManager::instance().mesh();
    if (!mesh) return -1;
    ChannelDetails ch;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        if (mesh->getChannel(i, ch) && name == ch.name) return i;
    }
    return -1;
}

// Shared CONTACT frame builder (MyMesh::writeContactRespFrame).
void CompanionService::writeContactFrame(uint8_t code, const ContactInfo& c) {
    int i = 0;
    _out[i++] = code;
    memcpy(&_out[i], c.id.pub_key, PUB_KEY_SIZE); i += PUB_KEY_SIZE;
    _out[i++] = c.type;
    _out[i++] = c.flags;
    _out[i++] = c.out_path_len;
    memcpy(&_out[i], c.out_path, MAX_PATH_SIZE); i += MAX_PATH_SIZE;

    // Prefer the locally-configured display name over MeshCore's
    // advert-overwritten c.name (BaseChatMesh::onAdvertRecv rewrites it on every
    // advert). Chat contacts come from ContactStore (alias); room servers from
    // config.roomServers (matched by hex pubkey). The companion protocol is
    // read-only and keys on pub_key, so this is a safe cosmetic override.
    const char* disp = c.name;
    if (Contact* sc = ContactStore::instance().findByPublicKey(c.id.pub_key)) {
        if (sc->name.length()) disp = sc->name.c_str();
    } else {
        char hex[2 * PUB_KEY_SIZE + 1];
        for (int b = 0; b < PUB_KEY_SIZE; b++) snprintf(hex + 2 * b, 3, "%02x", c.id.pub_key[b]);
        const auto& rooms = ConfigManager::instance().config().roomServers;
        for (const auto& rs : rooms) {
            if (rs.publicKey.length() == 64 && rs.name.length() &&
                rs.publicKey.equalsIgnoreCase(hex)) { disp = rs.name.c_str(); break; }
        }
    }

    memset(&_out[i], 0, 32);
    strncpy((char*)&_out[i], disp, 32); i += 32;
    memcpy(&_out[i], &c.last_advert_timestamp, 4); i += 4;
    memcpy(&_out[i], &c.gps_lat, 4); i += 4;
    memcpy(&_out[i], &c.gps_lon, 4); i += 4;
    memcpy(&_out[i], &c.lastmod, 4); i += 4;
    _iface->writeFrame(_out, i);
}

void CompanionService::writeOK() {
    _out[0] = RESP_CODE_OK;
    _iface->writeFrame(_out, 1);
}

void CompanionService::writeErr(uint8_t code) {
    _out[0] = RESP_CODE_ERR;
    _out[1] = code;
    _iface->writeFrame(_out, 2);
}

}  // namespace mclite
