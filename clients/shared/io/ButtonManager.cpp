#include "ButtonManager.h"

bool ButtonManager::begin(const uint8_t* pins, uint8_t count, ButtonCallback onPress,
                          unsigned long debounceMs) {
    _count      = min(count, (uint8_t)MAX_BUTTONS);
    _callback   = onPress;
    _debounceMs = debounceMs;

    if (_count == 0) {
        Serial.println("[BTN] count=0 — button subsystem inactive");
        _available = false;
        return false;
    }

    for (uint8_t i = 0; i < _count; ++i) {
        _pins[i]         = pins[i];
        _lastState[i]    = HIGH;
        _lastChangeMs[i] = millis();
        pinMode(_pins[i], INPUT_PULLUP);
    }

    _available = true;
    Serial.printf("[BTN] Initialised: %u buttons\n", _count);
    return true;
}

void ButtonManager::loop() {
    if (!_available) return;

    unsigned long now = millis();
    for (uint8_t i = 0; i < _count; ++i) {
        bool state = digitalRead(_pins[i]);
        if (state != _lastState[i] && (now - _lastChangeMs[i]) >= _debounceMs) {
            _lastChangeMs[i] = now;
            _lastState[i]    = state;

            if (state == LOW) {
                const char* action = _actionForButton(i);
                if (action && _callback) {
                    Serial.printf("[BTN] Button %u pressed: %s\n", i + 1, action);
                    _callback(i + 1, action);
                }
            }
        }
    }
}

void ButtonManager::setGameStatus(const char* status) {
    if (status) strlcpy(_gameStatus, status, sizeof(_gameStatus));
}

// Button 1: start if idle/finished, reset otherwise
// Button 2: pause if running, resume if paused
const char* ButtonManager::_actionForButton(uint8_t idx) const {
    if (idx == 0) {
        if (strcmp(_gameStatus, "idle")     == 0) return "start";
        if (strcmp(_gameStatus, "finished") == 0) return "reset";
        return "reset";
    }
    if (idx == 1) {
        if (strcmp(_gameStatus, "running") == 0) return "pause";
        if (strcmp(_gameStatus, "paused")  == 0) return "resume";
    }
    return nullptr;
}
