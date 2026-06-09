# Changelog

All notable changes to MCLite are documented here. The format is loosely based on
[Keep a Changelog](https://keepachangelog.com/), and the project follows semantic-ish versioning.

Targets: **T-Deck Plus** (`mclite-vX.Y.Z.bin`) and **T-Watch Ultra** (`mclite-watch-vX.Y.Z.bin`).

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
