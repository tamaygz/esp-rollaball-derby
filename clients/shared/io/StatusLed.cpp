#include "StatusLed.h"

void StatusLed::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    _lastToggle = millis();
}

void StatusLed::setState(LedState state) {
    _state = state;
}

void StatusLed::loop() {
    if (_state == LedState::WS_CONNECTED) {
        if (!_ledOn) {
            digitalWrite(_pin, HIGH);
            _ledOn = true;
        }
        return;
    }

    unsigned long period = _period();
    unsigned long now    = millis();
    if (now - _lastToggle >= period) {
        _lastToggle = now;
        _ledOn      = !_ledOn;
        digitalWrite(_pin, _ledOn ? HIGH : LOW);
    }
}

unsigned long StatusLed::_period() const {
    switch (_state) {
        case LedState::WIFI_ONLY: return 1000UL;
        case LedState::NO_WIFI:   return 150UL;
        default:                  return 0UL;
    }
}
