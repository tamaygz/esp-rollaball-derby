#pragma once
#include <Arduino.h>

/**
 * StatusLed — Non-blocking status LED controller.
 *
 * Drives a single GPIO LED to reflect device connectivity state:
 *   NO_WIFI     → fast blink (150 ms)
 *   WIFI_ONLY   → slow blink (1000 ms)
 *   WS_CONNECTED → solid on
 *
 * Usage:
 *   StatusLed led;
 *   led.begin(PIN_STATUS_LED);   // call once in setup()
 *   led.setState(LedState::NO_WIFI);
 *   // in loop():
 *   led.loop();
 */

enum class LedState : uint8_t {
    NO_WIFI,       // No WiFi connection — fast blink
    WIFI_ONLY,     // WiFi connected, no WebSocket — slow blink
    WS_CONNECTED,  // WebSocket connected to server — solid on
};

class StatusLed {
public:
    // Initialise GPIO output.  Must be called before loop().
    // pin: GPIO pin number — no default, caller supplies (e.g. PIN_STATUS_LED from config.h).
    void begin(uint8_t pin);

    // Drive the LED state machine.  Call every loop() iteration.
    void loop();

    // Update the connectivity state reflected by the LED.
    void setState(LedState state);

    LedState getState() const { return _state; }

private:
    uint8_t       _pin        = 0;
    LedState      _state      = LedState::NO_WIFI;
    bool          _ledOn      = false;
    unsigned long _lastToggle = 0;

    unsigned long _period() const;
};
