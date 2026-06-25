#pragma once

// CompanionService — bridges an external client (phone/desktop/CLI) to MCLite's
// radio over the official MeshCore companion protocol, running in PARALLEL with
// the on-device UI. Scope: messaging + read-only (see CompanionProtocol.h).
//
// Holds a SINGLE active transport (BaseSerialInterface*). WiFi/BLE/USB are
// mutually exclusive — the protocol is single-session and WiFi+BLE share the
// ESP32-S3 2.4 GHz radio. Transport lifetime is owned by the caller (main.cpp);
// this service only enable()/disable()s and pumps it.
//
// Phase 5d.1: scaffold + WiFi link + handshake (APP_START, DEVICE_QUERY,
// HAS_CONNECTION, LOGOUT). All other commands are rejected for now.

#include <Arduino.h>
#include <helpers/BaseSerialInterface.h>

struct ContactInfo;   // MeshCore (helpers/ContactInfo.h)

namespace mclite {

class CompanionService {
public:
    static CompanionService& instance();

    // Bind + enable a transport (e.g. SerialWifiInterface). end() disables it.
    void begin(BaseSerialInterface* iface);
    void end();

    // Pump the link: drain one inbound frame and dispatch it. Cheap + non-blocking;
    // call every main-loop tick.
    void loop();

    bool active() const { return _iface != nullptr; }
    bool clientConnected() const { return _iface && _iface->isConnected(); }

    // Desired companion state (set by the WiFi screen switches; session-only).
    // main.cpp starts/stops the transport based on these + connectivity. The two
    // are mutually exclusive — one transport + one client at a time.
    void setWifiCompanionEnabled(bool on) { _wifiWanted = on; if (on) { _usbWanted = false; _bleWanted = false; } }
    bool wifiCompanionEnabled() const { return _wifiWanted; }
    void setUsbCompanionEnabled(bool on)  { _usbWanted = on;  if (on) { _wifiWanted = false; _bleWanted = false; } }
    bool usbCompanionEnabled() const { return _usbWanted; }
    void setBleCompanionEnabled(bool on)  { _bleWanted = on;  if (on) { _wifiWanted = false; _usbWanted = false; } }
    bool bleCompanionEnabled() const { return _bleWanted; }

    // The BLE pairing PIN — generates + persists a random 6-digit one on first
    // use (config.json `ble.pin`). Shown on the BLE screen for the phone to enter.
    uint32_t ensureBlePin();

    // True once the BLE stack has been initialized this power-on. Bluedroid can't
    // be cleanly torn down at runtime (deinit releases memory permanently), so
    // WiFi must stay off after BLE has been used — a reboot is required to switch
    // back to WiFi. main.cpp sets this when it first inits BLE.
    void setBleInited(bool v) { _bleInited = v; }
    bool bleInited() const { return _bleInited; }

    // ACK bridge (5d.3): MeshManager forwards DM ACK/fail here so the client gets
    // PUSH_CODE_SEND_CONFIRMED and stops re-sending. Safe to call when inactive.
    void onAckConfirmed(uint32_t packetId);
    void onSendFailed(uint32_t packetId);

    // RX tee (5d.4): MeshManager forwards every received DM / channel message here.
    // Queues a sync frame and tickles the client with PUSH_CODE_MSG_WAITING.
    // No-ops when no client is connected (live-forward only). senderPubKey is 32 B;
    // meshChannelIdx matches GET_CHANNEL's index.
    void onContactMessage(const uint8_t* senderPubKey, uint32_t timestamp, const char* text);
    void onChannelMessage(uint8_t meshChannelIdx, uint32_t timestamp, const char* text);

    // Telemetry bridge: MeshManager forwards a contact's telemetry reply here so
    // the client gets PUSH_CODE_TELEMETRY_RESPONSE with the raw LPP payload (the
    // app parses it). Direct push; no-op when no client is connected. pubKey is 32 B.
    void onTelemetryResponse(const uint8_t* pubKey, const uint8_t* lpp, uint8_t lppLen);

    // Anon-request bridge: MeshManager forwards a CMD_SEND_ANON_REQ reply here. The
    // client gets PUSH_CODE_BINARY_RESPONSE keyed by the request tag (it matched the
    // tag from RESP_CODE_SENT). Direct push; no-op when no client is connected.
    void onAnonResponse(uint32_t tag, const uint8_t* data, uint8_t len);

    // Room-login bridge: MeshManager forwards a room/repeater login response here.
    // Drives PUSH_CODE_LOGIN_SUCCESS/FAIL, but only for logins the app initiated
    // (CMD_SEND_LOGIN) — a background/on-device auto-login pushes nothing. Also runs
    // the instant wrong-password fallback (retry once with the configured password).
    void onRoomLoginResult(size_t roomIdx, uint8_t status, uint8_t permissions,
                           uint8_t aclPerms, uint8_t fwLevel);

private:
    CompanionService() = default;

    void handleFrame(size_t len);

    // Handshake handlers (5d.1)
    void cmdAppStart(size_t len);
    void cmdDeviceQuery(size_t len);

    // Read-only query handlers (5d.2)
    void cmdGetDeviceTime();
    void cmdSetDeviceTime(size_t len);
    void cmdSendSelfAdvert(size_t len);
    void cmdGetBattAndStorage();
    void cmdGetContacts(size_t len);
    void cmdGetContactByKey(size_t len);
    void cmdGetChannel(size_t len);
    void cmdSyncNextMessage();   // 5d.2: replies NO_MORE_MESSAGES (real queue in 5d.4)

    // Outbound messaging handlers (5d.3)
    void cmdSendTxtMsg(size_t len);
    void cmdSendChannelTxtMsg(size_t len);
    void cmdSendTelemetryReq(size_t len);   // request telemetry from a contact (over the mesh)
    void cmdSendAnonReq(size_t len);        // anonymous request to a node by pubkey (over the mesh)
    void cmdSendLogin(size_t len);          // log into an already-configured room/repeater
    void cmdSetChannel(size_t len);         // add a channel (or remove via empty name); reboots to apply
    void cmdAddUpdateContact(size_t len);   // add a contact, or edit an existing one's name (live, no reboot)
    void cmdRemoveContact(size_t len);      // remove a contact + its history/advert (live, no reboot)
    void cmdShareContact(size_t len);       // re-broadcast a contact's advert (zero hop)
    void cmdReboot(size_t len);             // reboot the device (requires "reboot" magic word; no response)

    // Device-settings writes (gate: permissions.settings == "full"). Name applies live;
    // radio/TX/path-hash/PIN save to config and reboot to apply (re-applied at boot).
    bool settingsAllowed() const;           // permissions.settings == "full"
    void cmdSetAdvertName(size_t len);
    void cmdSetRadioParams(size_t len);
    void cmdSetTxPower(size_t len);
    void cmdSetDevicePin(size_t len);
    void cmdSetPathHashMode(size_t len);
    void cmdSetDefaultFloodScope(size_t len);   // persistent region (name+key) -> cfg.radio.scope
    void cmdGetDefaultFloodScope();             // read current region (name + derived key)
    void cmdSetFloodScopeKey(size_t len);       // session-only live scope override (raw key)
    void cmdResetPath(size_t len);              // reset a contact's learned path (flood rediscover)
    void cmdExportContact(size_t len);          // export self/contact advert blob
    void cmdImportContact(size_t len);          // inject an advert blob -> Heard Adverts
    void cmdGetAutoaddConfig();                 // always reports auto-add off (manual-add by design)
    void noteSent(uint32_t packetId);   // track a DM awaiting ACK confirmation

    // Stream one contact per loop tick while a GET_CONTACTS sync is in progress.
    void pumpContacts();
    void writeContactFrame(uint8_t code, const ::ContactInfo& c);

    // Clear all per-session state (pending ACKs, pending logins, offline queue,
    // contact-iteration cursor, negotiated app version) so nothing leaks from one
    // client/transport into the next. Called on connect (begin), teardown (end),
    // and the client-disconnect edge in loop().
    void resetSessionState();

    // Reply helpers
    void writeOK();
    void writeErr(uint8_t code);

    BaseSerialInterface* _iface = nullptr;
    uint8_t _appVer = 0;   // protocol version the connected app negotiated
    bool _wifiWanted = false;   // WiFi companion desired (UI switch)
    bool _usbWanted  = false;   // USB companion desired (UI switch)
    bool _bleWanted  = false;   // BLE companion desired (UI switch)
    bool _bleInited  = false;   // BLE stack initialized this session (no clean teardown)

    // GET_CONTACTS streaming state (single client → single iteration at a time)
    bool     _contactsIterating  = false;
    int      _contactCursor      = 0;
    int      _contactCount       = 0;
    uint32_t _contactsSince      = 0;
    uint32_t _mostRecentLastmod  = 0;

    // DMs sent via the companion, awaiting an ACK to confirm to the client.
    struct PendingAck { uint32_t packetId; uint32_t sentMs; bool active; };
    static constexpr int PENDING_ACKS = 8;
    PendingAck _pending[PENDING_ACKS] = {};

    // App-initiated room logins (CMD_SEND_LOGIN), keyed by config room index. Drives
    // the LOGIN_SUCCESS/FAIL push + the instant wrong-password fallback (retry once
    // with the configured password). prefix = first 6 B of the room pubkey.
    static constexpr size_t MAX_ROOMS = 8;
    struct PendingLogin { bool active = false; bool retried = false;
                          bool fallbackEligible = false; uint8_t prefix[6] = {0}; };
    PendingLogin _pendingLogin[MAX_ROOMS];

    // Deferred reboot after a config-mutating command (CMD_SET_CHANNEL). Re-armed on
    // each change so a burst persists before one reboot; 0 = none. Checked in loop().
    static constexpr uint32_t REBOOT_DELAY_MS = 1500;
    uint32_t _rebootAtMs = 0;

    // Offline queue of pre-built sync frames (drained by SYNC_NEXT_MESSAGE).
    struct OfflineMsg { uint8_t len; uint8_t buf[MAX_FRAME_SIZE]; };
    static constexpr int OFFLINE_QUEUE_SIZE = 24;
    OfflineMsg _offline[OFFLINE_QUEUE_SIZE];   // LIVE messages received while connected
    int _offlineLen = 0;
    void enqueueOffline(const uint8_t* frame, int len);
    void tickleMsgWaiting();

    // Cursor-driven backfill: on connect, SYNC walks MessageStore directly (no cap) and
    // builds each inbound history frame on demand, so every stored message is delivered
    // (the old fixed 24-slot pre-stage dropped the newest messages and starved chats).
    bool _bfActive = false;     // history backfill in progress this session
    int  _bfConvoIdx = 0;       // cursor: conversation index into MessageStore
    int  _bfMsgIdx = 0;         // cursor: message index within that conversation
    int  buildNextBackfillFrame();   // positions the cursor at the next inbound msg, builds it into _out; 0 = done
    // Send the next SYNC response (a queued message, or NO_MORE_MESSAGES). Only pops
    // the message once the transport confirms it accepted the frame, so a silently
    // dropped frame (WiFi send_queue full) never loses a message. Returns false if the
    // transport can't take it right now — the caller defers via _syncResponsePending.
    bool trySendSyncResponse();
    bool _syncResponsePending = false;   // a SYNC reply is owed but the transport was full/busy

    // Build a CONTACT/CHANNEL_MSG_RECV[_V3] frame into _out; returns its length.
    int  buildContactRecvFrame(const uint8_t* senderPubKey, uint32_t timestamp, const char* text);
    int  buildChannelRecvFrame(uint8_t meshChannelIdx, uint32_t timestamp, const char* text);

    // On client connect: replay stored received messages so the client shows
    // history, not just messages that arrive while connected.
    void backfillHistory();
    int  channelIdxByName(const String& name);   // name -> mesh channel index
    bool _wasConnected = false;   // connect-edge detection in loop()

    uint8_t _cmd[MAX_FRAME_SIZE + 1];
    uint8_t _out[MAX_FRAME_SIZE + 1];
};

}  // namespace mclite
