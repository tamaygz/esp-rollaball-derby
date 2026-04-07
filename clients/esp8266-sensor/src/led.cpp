#include "led.h"

// Wemos D1 Mini built-in LED is connected active-LOW (HIGH = off, LOW = on).
static constexpr bool LED_ACTIVE_LOW = true;

void StatusLed::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    _write(false);
}

void StatusLed::setState(LedState state) {
    if (state == _state) return;
    _state      = state;
    _lastToggle = millis();

    if (state == LedState::WS_CONNECTED) {
        _write(true);
    }
}

void StatusLed::loop() {
    if (_state == LedState::WS_CONNECTED) {
        return;  // setState() already turned LED on when entering this state
    }

    // NO_WIFI → 100 ms half-period (5 Hz),  WIFI_ONLY → 500 ms half-period (1 Hz)
    unsigned long periodMs = (_state == LedState::NO_WIFI) ? 100UL : 500UL;
    unsigned long now      = millis();

    if (now - _lastToggle >= periodMs) {
        _lastToggle = now;
        _ledOn      = !_ledOn;
        _write(_ledOn);
    }
}

void StatusLed::_write(bool on) {
    _ledOn = on;
    digitalWrite(_pin, LED_ACTIVE_LOW ? !on : on);
}
