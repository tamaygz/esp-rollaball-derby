#pragma once
#include <Arduino.h>

// One step of a non-blocking LED sequence: LED on/off for a given duration.
struct LedStep { bool on; unsigned long ms; };

// LED states drive the visual status indicator on the built-in LED.
enum class LedState {
    NO_WIFI,       // Fast blink 5 Hz (100 ms half-period) — no WiFi
    WIFI_ONLY,     // Slow blink 1 Hz (500 ms half-period) — WiFi OK, WS disconnected
    WS_CONNECTED   // Solid on — WebSocket connected
};

class StatusLed {
public:
    // Call once in setup(). pin should be PIN_LED from config.h.
    void begin(uint8_t pin);

    // Change the displayed state. Redundant calls (same state) are ignored.
    void setState(LedState state);

    // Must be called every loop() iteration for non-blocking blink.
    void loop();

    // One-shot LED sequences for game events. Each call interrupts any in-progress
    // sequence and starts a new one; normal state resumes when the sequence ends.
    void triggerCountdownTick(); // 1 long blink (600 ms on)
    void triggerWinner();        // 6 fast blinks
    void triggerLoser();         // 1 long blink (1000 ms on)

private:
    static constexpr uint8_t SEQ_MAX = 14; // enough for 6 on+off pairs

    uint8_t       _pin          = LED_BUILTIN;
    LedState      _state        = LedState::NO_WIFI;
    LedState      _resumeState  = LedState::NO_WIFI; // restored after sequence
    bool          _ledOn        = false;
    unsigned long _lastToggle   = 0;

    // One-shot sequence player
    LedStep       _seq[SEQ_MAX]  = {};
    uint8_t       _seqLen        = 0;
    uint8_t       _seqIdx        = 0;
    bool          _inSeq         = false;
    unsigned long _seqStart      = 0;

    // Write LED, accounting for active-LOW wiring of Wemos D1 Mini LED.
    void _write(bool on);
    void _startSeq(const LedStep* steps, uint8_t len);
};
