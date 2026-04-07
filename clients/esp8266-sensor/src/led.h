#pragma once
#include <Arduino.h>

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

private:
    uint8_t      _pin         = LED_BUILTIN;
    LedState     _state       = LedState::NO_WIFI;
    bool         _ledOn       = false;
    unsigned long _lastToggle = 0;

    // Write LED, accounting for active-LOW wiring of Wemos D1 Mini LED.
    void _write(bool on);
};
