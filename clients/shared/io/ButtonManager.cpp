#include "ButtonManager.h"
#include <derby_logger.h>

bool ButtonManager::begin(const uint8_t* pins, uint8_t count, ButtonCallback onPress,
                          unsigned long debounceMs) {
    _count      = min(count, (uint8_t)MAX_BUTTONS);
    _callback   = onPress;
    _debounceMs = debounceMs;

    if (_count == 0) {
        DERBY_LOG_LN("[BTN] count=0 — button subsystem inactive");
        _available = false;
        return false;
    }

    for (uint8_t i = 0; i < _count; ++i) {
        _pins[i]         = pins[i];
        _lastState[i]    = HIGH;
        _lastChangeMs[i] = millis();

        // GPIO34-39 on original ESP32 are input-only (GPI) pins — the hardware
        // silently ignores INPUT_PULLUP, leaving the pin floating and causing
        // spurious button fires every debounce window.  Warn and use INPUT instead;
        // the caller must supply external pull-up resistors on those pins.
#if defined(ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
        if (_pins[i] >= 34 && _pins[i] <= 39) {
            pinMode(_pins[i], INPUT);
            DERBY_LOG_F("[BTN] WARNING: GPIO%u is input-only (no internal pull-up). Add external 10k pull-up to 3.3V or move to another pin!\n", _pins[i]);
        } else {
            pinMode(_pins[i], INPUT_PULLUP);
        }
#else
        pinMode(_pins[i], INPUT_PULLUP);
#endif
    }

    _available = true;
    DERBY_LOG_F("[BTN] Initialised: %u buttons\n", _count);
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
                    DERBY_LOG_F("[BTN] Button %u pressed: %s\n", i + 1, action);
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
