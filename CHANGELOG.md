# Changelog

All notable changes to MCLite are documented here. The format is loosely based on
[Keep a Changelog](https://keepachangelog.com/), and the project follows semantic-ish versioning.

Targets: **T-Deck Plus** (`mclite-vX.Y.Z.bin`) and **T-Watch Ultra** (`mclite-watch-vX.Y.Z.bin`).

## [Unreleased]

### Added
- **Emoji in chat** — received emoji now render inline (a monochrome OpenMoji font with a Montserrat fallback,
  so plain text is unchanged and unknown glyphs degrade gracefully). An on-device **emoji picker**
  (`display.emoji`, **default on**, can be disabled) adds a smiley button to the chat input for composing from a curated set;
  it won't let you push a message past the 160-byte limit. Incoming/outgoing text is sanitized (strips emoji
  variation selectors that render as boxes, normalizes “smart” quotes to ASCII). Adopted from the
  [@jason-s13r](https://github.com/jason-s13r) fork; OpenMoji is CC-BY-SA 4.0 (see LICENSES.md).
- **Three-step volume** — the status-bar bell now cycles **max → mute → mid → max** instead of a binary
  mute, and the built-in chime *and* custom WAV notifications scale to the level. Default is max (loudness
  unchanged); SOS stays fixed and loud; always-sound contacts still override mute. Note: custom WAVs are now
  volume-scaled, so they play at the chime's level rather than their raw file loudness
  ([@jason-s13r](https://github.com/jason-s13r), #11).

### Changed
- **`sound.enabled` is now a true master switch.** Previously `false` just booted the device muted (still
  toggleable via the status-bar bell, and SOS/always-sound contacts could still make noise). Now `false` means
  *fully silent* — no notifications, no chime, **and no SOS sound** — and the status-bar bell is hidden so
  there's no per-session volume toggle. Set `sound.enabled: true` (the default) for the previous behavior with
  the 3-step volume bell.

### Fixed
- A last-known position restored after reboot no longer reports "~0s ago" before the clock has synced. The
  saved fix carries an absolute timestamp but `millis()` resets on reboot, so until GPS re-locks (or NTP/WiFi
  syncs the clock) its age can't be computed — it now shows "Last known position" instead of a misleading 0s
  (which also avoided sending a stale position over the mesh as if it were current). Once the clock syncs, the
  real "~Xm ago" age is shown again.
- **A failed send now shows a toast** instead of silently drawing a `FAILED` bubble. When a message can't be
  queued — e.g. the static packet pool is drained by a burst — you get a "Send failed - try again" toast (the
  failed bubble is still there to tap-retry). Previously the only signal was the bubble's small status icon.
- **Map tile loader hardening.** The slippy-tile PNG decoder now rejects any tile larger than the standard
  256×256 (a corrupt or non-standard PNG could previously overflow the fixed scanline buffer) and validates
  tile coordinates (zoom 0–19, indices within range) before touching the SD card — out-of-range tiles grey-fill
  cleanly instead of building nonsense paths. Missing/undecodable tiles already grey-filled; this closes the
  oversized-tile gap.

## [0.3.5] — 2026-06-11

### Fixed
- **Translations past ~128 keys reverted to English.** The i18n loader capped SD-loaded strings at 128, but the
  language files now hold ~197 keys — so on German/French/Italian every key past the cap silently fell back to
  English (e.g. `canned_5`–`8`, plus the offgrid / firmware-update / WiFi / USB / BLE / map / heard-adverts /
  toast screens). Raised the cap to 256 and added a boot-time warning if a language file ever exceeds it.
- **Config tool wiped stored WiFi on edit.** The tool's file-import never loaded the `wifi` section, so
  importing a device's `config.json`, editing it, and re-exporting produced an empty `wifi` block — clearing
  the device's stored SSID/password on the next copy to SD. WiFi (and the persisted BLE pairing PIN) now
  round-trip correctly, including through the share-link and start-fresh paths.

## [0.3.4] — 2026-06-11

### Added
- **Auto-refresh contact GPS** — keeps the map markers / convo-list badges of contacts who *don't* broadcast
  their own location fresh, by quietly re-requesting telemetry GPS before the cached fix goes stale. Throttled
  (one request per scan, respects the EU duty cycle, yields to manual requests) and self-limiting (stops asking
  a contact that doesn't answer). New setting `messaging.auto_telemetry`, **default on**, can be disabled.
- **Per-conversation quick replies** — any contact, channel, or room can carry its own `canned` list (max 8)
  that overrides the global quick-reply list *for that chat only*; leave it empty to fall back to the global
  list. Editable per card in the config tool. Turns a conversation into a command menu — e.g. a Home Assistant
  / automation bridge ("Open gate", "Lights on", "Status?").
- **Last-known location persists across reboots** — the most recent GPS fix is saved to SD
  (`/mclite/last_location.json`, throttled) and restored on boot, so the map opens to your last position
  without waiting for a fresh fix ([@jason-s13r](https://github.com/jason-s13r), #10).
- **Advertise from the companion app** — the MeshCore phone/desktop app's **Advertise** button now works while
  connected (BLE/WiFi/USB); previously it was rejected as an unsupported command. Honours the app's flood vs
  local (zero-hop) option. The on-device advert button and the automatic periodic advert are unchanged.

### Changed
- The device-info / admin screen is now fully localized — every row label routes through the translation
  table (de/fr/it) ([@jason-s13r](https://github.com/jason-s13r), #9).
- Conversation history now loads only the most recent `max_history_per_chat` messages per chat at boot
  (previously the whole file was loaded into RAM). Bounds memory if a history file is larger than the cap —
  e.g. after lowering the setting. No visible change; the runtime cap was already in place.

### Fixed
- Telemetry retry is no longer cancelled by the contact-info pop-up's own timeout when the mesh's outbound
  queue is busy (the two timers now stay in lockstep) — the retry fires under congestion as intended.
- Closing the contact-info pop-up now cancels its in-flight telemetry retry, so no stray flood request goes
  out for a pop-up you already closed.
- Muting a chat is now absolute: it silences notifications even for a contact flagged `always_sound`
  (`always_sound` still overrides *global* mute, unchanged).
- Messages that exceed the 160-**byte** limit (e.g. emoji or accented/non-Latin text — which can be ≤160
  *characters* but more bytes) are now refused with a "Message too long" toast that keeps your text, instead
  of silently failing to send while still drawing a (failed) bubble.
- A timed-out telemetry request now releases the radio's single telemetry slot when the exchange ends
  (previously the slot stayed held after a no-response request). Without this, **auto-refresh contact GPS**
  would stall for the rest of the session after the first contact that didn't answer, and slow/multi-hop
  contacts could be backed off too eagerly; both are resolved.
- Auto-refresh no longer lets a single un-sendable contact block the rest of the round-robin, and
  `max_history_per_chat: 0` now consistently means "unlimited" on both load and prune (previously prune at 0
  would wipe the conversation).

## [0.3.3] — 2026-06-10

Reliability + contact-location improvements, plus a batch of community contributions
([@jason-s13r](https://github.com/jason-s13r)'s fork PRs).

### Added
- **Unified contact location** — one source of truth for where a contact is: fresh telemetry (accurate) →
  their advert GPS → a heard advert. The convo-list **GPS badge** now appears for *any* known position (not
  just telemetry), and the **telemetry pop-up** shows that position even without a telemetry reply — marking
  advert-sourced coordinates approximate (`~`) and offering the **Map** button — and on a request timeout it
  shows the known position instead of a bare "No response". Telemetry stays primary; the 30-min window is
  unchanged.
- **Flood-routing retries** for better delivery when a direct path degrades — on **DM** retries (#5) and on
  **telemetry-request** retries (#6, with a "Retrying…" state and queue-aware timeout extension).
- **Per-chat mute** (opt-in, `messaging.allow_mute`, default off) — long-press a conversation to mute; muted
  chats don't beep or wake the screen (SOS always does), with an indicator in the list and chat header (#4).
- **Vendor row** on the device-info screen showing the firmware's source repo (`owner/repo`) — handy with
  fork flashing (#7).

### Fixed
- Chat **scroll-to-bottom** is more robust on an empty chat area / on open (#3).

### Thanks
- @jason-s13r for PRs #3–#7.

## [0.3.2] — 2026-06-09

Map unification + fixes from a careful review of 0.3.0/0.3.1.

### Changed
- The contact "Map" button and the status-bar GPS icon now open the **same** map (one screen, one set of
  controls). Opening from a contact just centers on that contact and pre-selects it — drawn slightly larger
  with a highlight ring and its name in the bottom bar — while still showing every other contact / telemetry /
  heard-node location and your own position (a distinct green/amber dot).
- **Reload** rebuilds *all* markers and re-checks your own position; **Center** always jumps to your own
  location once a fix is available (even when the map was opened from a contact), falling back to the
  location the map opened on when there's no fix.

### Fixed
- Center button now uses your own location on a contact-opened map (previously it only ever recentered on the
  contact).
- The status-bar GPS icon stays visible (dimmed) and tappable when GPS is disabled in config, so the general
  map remains reachable.
- `@mention` no longer truncates a near-full message draft (skips the insert when it wouldn't fit).
- Companion: the contacts `since` field is read via `memcpy` (removed an unaligned read).

## [0.3.1] — 2026-06-09

Fork-adoption batch (features adopted from the jason-s13r/MCLite fork) plus map polish.

### Added
- **GPS location in adverts** (opt-in, `gps.location_advert`, default off): broadcast your position so
  contacts see you on their map. Uses MeshCore's native advert location (full precision); sends a LIVE fix or
  a still-valid last-known one. Unencrypted broadcast — hence opt-in. Read-only status shown on the admin GPS
  screen; toggled via the config tool / SD only.
- **General map** — tap the status-bar GPS icon to open a map of your own location plus every heard node /
  contact that carries GPS, drawn with the same chat / repeater / room / sensor symbols as the heard-adverts
  list. Tap a marker for its name; **Reload** button re-scans heard nodes without panning.
- **NTP time sync** — when WiFi is connected and GPS hasn't locked, set the clock from an NTP server using the
  configured POSIX timezone. GPS still overrides once it locks.
- **@mention** — tap a sender's name in a channel/room to insert `@name ` into the message box.
- **Fork-aware OTA** — build-time overridable update repo (`MCLITE_REPO_OWNER` / `MCLITE_REPO_NAME`,
  default `laserir/MCLite`) so forks can self-update from their own releases.
- **Web flasher repo/fork picker** — choose which repo's published releases to flash.

### Changed
- Map markers render as filled colored dots (type color + black rim) with a contrasting symbol, so they read
  against any map tile; selection ring sits just outside the dot.

### Fixed
- Map markers are now reliably tappable (tap-slop dead-zone so a jittery tap selects instead of micro-panning;
  wider hit tolerance).
- Map markers no longer blink out near the viewport edge across zoom levels (consistent rounding + wider clip).

## [0.3.0] — 2026-06-09

### Added
- **Companion mode** — bridge the radio to a phone, desktop, or CLI over the standard MeshCore companion
  protocol, in parallel with normal on-device use (messages appear in both):
  - **Bluetooth** — pairs with the official MeshCore iOS/Android apps (6-digit passkey + bonding).
  - **WiFi** — reachable from `meshcore-cli` / `meshcore.js` / `meshcore_py` on the LAN.
  - **USB** — wired serial companion (debug logs muted while active so they don't corrupt the protocol).
  - Messaging works; config is read-only. One transport at a time.

### Changed
- Refreshed README (Quick Start + balanced companion docs).

### Notes
- WiFi and Bluetooth can't run together (shared radio/RAM); the device handles the switch and offers a reboot
  when needed.
- Known limitation: messages typed on the device don't mirror to the companion app (the protocol has no
  firmware-composed-message event).

## [0.2.2] — 2026-06-07

### Fixed
- WiFi over-the-air firmware updates no longer crash the device (TLS handshake + on-stack buffer overflowed
  the loop-task stack). SD-card install and USB flashing were unaffected.

### Notes
- **0.2.1 was withdrawn** — its WiFi installer had the crash above. Update from 0.2.1 via SD card or USB.

## [0.2.0] — 2026-06-01

### Added
- **T-Watch Ultra support** — second target board (touch-native UI, AMOLED, RTC, haptics).
- **On-device firmware update** — install a `.bin` from the SD card, or check & download updates over WiFi
  (the WiFi path was hardened in 0.2.2).

## [0.1.8] — 2026-05-05

### Added
- **Heard Adverts** — 64-entry rolling list of every advert the radio decodes, with type icons, last-heard
  age, per-hop path, and a one-tap **Save** to add a chat advert to contacts (applies next boot). Manual-advert
  button announces yourself on demand.

### Changed
- Config saves are **atomic** (stage → `.bak` → rename) with boot fallback to `config.json.bak`, protecting
  the identity keys against a torn write.

### Fixed
- `Discovered contact` log printed the raw packed `path_len` byte instead of the hop count.

## [0.1.7] — 2026-04-29

### Added
- **Room server client** — join MeshCore community message boards (up to 8), with auto-login + backoff,
  disconnect recovery, per-room flags (`read_only` / `allow_sos` / `send_sos` / `scope`), sender resolution,
  and persisted `sync_since`. Admin "Rooms" section + config-tool Rooms card.

### Changed
- Contact/conversation caps raised for the extra rooms (40 contacts / 56 conversations).

## [0.1.0] – [0.1.5] — 2026-03 / 2026-04

Foundation releases: the core standalone MeshCore companion firmware for the T-Deck Plus — encrypted DMs and
channels, SOS alert system, GPS location sharing (lat/lon + MGRS), telemetry, message history on SD,
internationalization (de/fr/it), the offline config tool, and the browser web flasher — iterated across
0.1.1–0.1.5.
