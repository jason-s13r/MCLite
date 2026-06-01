#pragma once

#include <cstdint>

namespace mclite {

// DRV2605L vibration driver on the T-Watch Ultra. Sits on the shared Wire
// bus at 0x5A and is gated by the XL9555 expander's port 6
// (TWATCH_EXP_DRV_EN), which init() raises before talking to the chip.
//
// The DRV2605 has 117 ROM waveforms (libraries 1..5 are ERM, 6 is LRA).
// We use library 1 ("Strong/Sharp/Soft Click, Double/Triple Click, Buzz")
// since the watch's actuator is an ERM. Effect IDs picked for MCLite:
//   message arrival → Double Click 100% (effect 10)
//   button feedback → Soft Bump 100%    (effect 7)
//   SOS event       → Alert 1000ms 100% (effect 16)
//
// Mute is tied to the speaker mute for v1: callers should already check
// `Speaker::isMuted()` before triggering the matching haptic; this avoids
// surprising the user with vibration when they've silenced the device.
class Haptic {
public:
    static Haptic& instance();

    bool init();
    bool isReady() const { return _ready; }

    // Effect playback. All non-blocking — the chip plays the ROM waveform
    // autonomously and the call returns immediately.
    void playMessage();
    void playButton();
    void playSos();
    void stop();

private:
    Haptic() = default;
    bool _ready = false;
};

}  // namespace mclite
