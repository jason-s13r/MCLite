#pragma once

// Lightweight debug-log gate.
//
// All MCLite serial logging goes through LOGF/LOGLN/LOGP so it can be muted
// atomically while a binary protocol owns the USB-CDC port (USB companion mode —
// the protocol and log text can't share the one port). When g_logMuted is true
// the macros no-op; otherwise they forward to Serial exactly like the old
// Serial.printf/println/print calls.

#include <Arduino.h>

// C++17 inline variable: one definition across all TUs (firmware + native tests)
// with no separate .cpp. Set true while USB companion owns the USB-CDC port.
namespace mclite { inline bool g_logMuted = false; }

#define LOGF(...)  do { if (!::mclite::g_logMuted) Serial.printf(__VA_ARGS__); } while (0)
#define LOGLN(x)   do { if (!::mclite::g_logMuted) Serial.println(x); } while (0)
#define LOGP(x)    do { if (!::mclite::g_logMuted) Serial.print(x); } while (0)
