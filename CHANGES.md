# Changes since 0693a840

## 🔧 Configuration & Build
- **Dynamic versioning & repo owner** — `merge_firmware.py` now reads `MCLITE_VERSION` and `MCLITE_REPO_OWNER` from `PLATFORMIO_BUILD_FLAGS` (set via `.env`) and falls back to `defaults.h`. `UpdateChecker.cpp` uses these macros to construct the GitHub API URL, enabling forks to check their own releases. The `.env` file was later deleted and added to `.gitignore`.
- **Radio presets** — Added a `radio_presets.json` file with 19 regional presets (Australia, EU/UK, New Zealand, USA/Canada, etc.) and moved it from `firmware/sdcard/` to top-level `sdcard/`.

## 🗺️ Map & GPS Features
- **Map panning & zoom controls** — Added directional pan buttons (up/down/left/right) to `MapScreen`. Zoom buttons moved into the `lv_win` header. Initial zoom is now chosen based on telemetry age (stale data → lower zoom) and whether the user's own position fits on screen alongside the contact.
- **Home Screen** — New `HomeScreen` with Chat and Map launcher buttons. `UIManager` was refactored from a flat `goHome()` model to a `pushScreen()`/`popScreen()` navigation stack, enabling proper back-navigation through nested screens.
- **GPS location advertising** — `MCLiteMesh::advertise()` can now include the device's GPS coordinates in periodic self-adverts. Coordinates are obfuscated via bit-shifting based on a configurable `locationPrecision` (0=off, 10–19=grid steps ~23 km to ~45 m, 32=full precision). Settings are exposed in `RadioGpsScreen`.
- **Map node markers** — `MapScreen::drawNodeMarkers()` renders dots for contacts with cached telemetry locations and for heard adverts that contain GPS data, using distinct colors per advert type (chat, repeater, room, sensor).
- **TileLoader improvements** — `computeCenterFromTiles()` now prefers a mid-range zoom (6–10) over the highest zoom to avoid freezing when deep tile pyramids exist. Tile scanning is capped at 2000 files and uses bounding-box center instead of mean for robustness. GPS module now persists/restores the last known location to `/mclite/last_location.json` on SD, so the map can open meaningfully even without a live fix.

## � Mesh & Routing
- **Flood routing on message retries** — `MCLiteMesh::retryOrFail()` now forces flood routing (`out_path_len = OUT_PATH_UNKNOWN`) when retrying a failed direct message, giving the mesh a better chance to deliver if the direct path has degraded.
- **Telemetry retry with flood fallback** — Added a `TelemRetry` tracking struct and `checkTelemTimeout()` loop. If a telemetry request times out and the outbound queue is empty, it retries once via flood routing. If the queue is still busy, the timeout is extended by 2 seconds. A new `onTelemetryRetry` callback chain propagates the retry to the UI, which updates the telemetry modal with "Retrying via flood..." and extends the displayed timeout.

## �💬 Messaging & Chat
- **Reply-to via tap** — Tapping a sender name in a chat bubble prepends `@[name] ` to the text input.
- **Mute/unmute conversations** — Long-pressing a conversation row in `ConvoListScreen` toggles mute state. Muted chats show a mute icon in the conversation list and chat header. Incoming messages from muted chats do not trigger sounds or screen wake (SOS alerts still bypass mute).
- **Chat scroll fix** — `scrollToBottom()` now scrolls to the last child object instead of using `LV_COORD_MAX`, fixing jumps when the chat area is empty. Also called automatically when `ChatScreen::show()` opens.

## 😀 Emoji Support
- **Emoji rendering** — Added OpenMoji black-glyph font assets and generated LVGL C font files at 12/14/16/20 px (`emoji_font_*.c`). `lv_conf.h` enables the extra font symbols. A new `TextSanitizer.h` utility strips invisible U+FE0F variation selectors and replaces "smart" typographic quotes with ASCII equivalents; it is applied to message text, sender names, conversation names, device name, and SOS alerts.
- **Emoji keyboard** — Added a 6×4 grid emoji picker (24 emojis) in `ChatScreen`, toggled via a 🙂 button next to the canned-messages button. Selection appends the emoji to the textarea.

## ⚙️ Admin & Settings
- **Admin screen refactor** — The monolithic `AdminScreen` was split into four focused screens:
  - `AdminScreen` — device info, firmware version, public key, and navigation rows to sub-screens.
  - `RadioGpsScreen` — off-grid repeater toggle, radio preset loader, editable radio parameters (frequency, SF, bandwidth, coding rate, TX power, scope, path hash mode) with validation and reboot-to-apply prompts, GPS enable toggle, location advert toggle, and location precision slider.
  - `DisplaySoundBatteryScreen` — interactive brightness slider, keyboard backlight slider, editable boot text, plus read-only sound and battery info.
  - `MessagingContactsChannelsRoomsScreen` — messaging settings, contact list, channel list with add/remove functionality, and room list.
- **Channel management** — `ChannelStore` gained support for custom channels persisted to `/mclite/custom/channels.json`, with `deriveHashtagChannel()`, `removeChannelByName()`, and `saveCustomChannels()`. `MeshManager::addChannel()` registers new channels with MeshCore at runtime. The add-channel modal includes switches for send-SOS, receive-SOS, and read-only.
- **Admin UI fix** — Removed a fixed `32×32` size on the channel trash button so it sizes naturally.

## 🖥️ UI/UX Refactors
- **Window header management** — `AdminScreen`, `HeardAdvertsScreen`, `MapScreen`, `ChatScreen`, and `ConvoListScreen` were refactored to use `lv_win` with a consistent back button in the header. `MapScreen` changed from a full-screen overlay (`lv_scr_load`) to a normal child object managed by `UIManager`. `HeardAdvertsScreen` rows gained info and map buttons per entry instead of making the whole row clickable.

## 🌐 Tools
- **Web flasher custom firmware** — Added a file upload input to the web flasher HTML so users can flash a custom `.bin` file.

## 🔀 Maintenance
- **Upstream merge** — Merged upstream `v0.2.2` (plus 4 post-release commits, tip `50d1e14`). This brought in:
  - **WiFi setup UI** — New `WiFiSetupScreen` with on-screen keyboard, saved-password support, and QR-code scan-to-connect flow. `WiFiManager` handles connection state and reconnection.
  - **OTA firmware updates** — `FirmwareUpdater` and `UpdateChecker` classes that download and flash new firmware from GitHub releases over HTTPS, with version comparison via `util/version.h`.
  - **Status bar WiFi icon** — Shows connection state in the top status bar.
  - **Localization files** — Added `de.json`, `fr.json`, and `it.json` language packs under `sdcard/mclite/lang/`.
  - **Web flasher refresh** — Updated bundled firmware binaries to `v0.2.2` for both T-Deck (`mclite-v0.2.2.bin`) and T-Watch (`mclite-watch-v0.2.2.bin`).
