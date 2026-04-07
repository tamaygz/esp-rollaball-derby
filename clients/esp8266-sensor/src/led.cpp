#include "led.h"

// Wemos D1 Mini built-in LED is connected active-LOW (HIGH = off, LOW = on).
static constexpr bool LED_ACTIVE_LOW = true;

void StatusLed::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    _write(false);
}

void StatusLed::setState(LedState state) {
    if (_inSeq) {
        // Don't interrupt a running sequence; remember the desired state for when
        // the sequence ends so the caller's intent is not lost.
        _resumeState = state;
        return;
    }
    if (state == _state) return;
    _state      = state;
    _lastToggle = millis();

    if (state == LedState::WS_CONNECTED) {
        _write(true);
    }
}

void StatusLed::loop() {
    if (_inSeq) {
        unsigned long now = millis();
        if (now - _seqStart >= _seq[_seqIdx].ms) {
            _seqIdx++;
            if (_seqIdx >= _seqLen) {
                // Sequence done — restore the state that was set during the sequence.
                _inSeq      = false;
                _state      = _resumeState;
                _lastToggle = now;
                _write(_state == LedState::WS_CONNECTED);
                return;
            }
            _seqStart = now;
            _write(_seq[_seqIdx].on);
        }
        return;
    }

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

void StatusLed::_startSeq(const LedStep* steps, uint8_t len) {
    uint8_t n = (len < SEQ_MAX) ? len : SEQ_MAX;
    for (uint8_t i = 0; i < n; i++) _seq[i] = steps[i];
    _seqLen      = n;
    _seqIdx      = 0;
    _resumeState = _state;
    _inSeq       = true;
    _seqStart    = millis();
    _write(_seq[0].on);
}

void StatusLed::triggerCountdownTick() {
    // One long blink: 600 ms on, 400 ms off — fits within the 1-second tick interval.
    static const LedStep seq[] = { {true, 600}, {false, 400} };
    _startSeq(seq, 2);
}

void StatusLed::triggerWinner() {
    // Six quick flashes to celebrate a win.
    static const LedStep seq[] = {
        {true, 80}, {false, 80}, {true, 80}, {false, 80},
        {true, 80}, {false, 80}, {true, 80}, {false, 80},
        {true, 80}, {false, 80}, {true, 80}, {false, 80},
    };
    _startSeq(seq, 12);
}

void StatusLed::triggerLoser() {
    // One slow, sad blink.
    static const LedStep seq[] = { {true, 1000}, {false, 500} };
    _startSeq(seq, 2);
}
