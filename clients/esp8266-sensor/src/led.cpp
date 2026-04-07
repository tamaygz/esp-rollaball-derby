#include "led.h"

// Color definitions for status LED (using HSV for smooth, vibrant colors)
static const HsvColor COLOR_OFF(0, 0, 0);         // Black (off)
static const HsvColor COLOR_NO_WIFI(0, 255, 255);  // Red (no WiFi)
static const HsvColor COLOR_WIFI(30, 255, 255);    // Orange (WiFi only)
static const HsvColor COLOR_WS(120, 255, 255);     // Green (WebSocket connected)

void StatusLed::begin(uint8_t pin) {
    _pin = pin;
    
    // Initialize LedController with 1 LED
    // Note: For WS2812B status LED. ESP8266 DMA requires GPIO3.
    if (!_controller.begin(1, _pin)) {
        Serial.println("[StatusLed] ERROR: Failed to initialize LED controller");
        return;
    }
    
    _controller.setBrightness(128);  // 50% brightness for status LED
    _write(false);
    _controller.show();
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
        _controller.setPixel(0, COLOR_WS);
        _controller.show();
    }
}

void StatusLed::loop() {
    // Call LedController loop for WiFi yield
    _controller.loop();

    if (_inSeq) {
        unsigned long now = millis();
        if (now - _seqStart >= _seq[_seqIdx].ms) {
            _seqIdx++;
            if (_seqIdx >= _seqLen) {
                // Sequence done — restore the state that was set during the sequence.
                _inSeq      = false;
                _state      = _resumeState;
                _lastToggle = now;
                
                if (_state == LedState::WS_CONNECTED) {
                    _controller.setPixel(0, COLOR_WS);
                } else {
                    _controller.setPixel(0, COLOR_OFF);
                }
                _controller.show();
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
    
    if (!on) {
        _controller.setPixel(0, COLOR_OFF);
    } else {
        // Choose color based on current state
        HsvColor color;
        switch (_state) {
            case LedState::NO_WIFI:
                color = COLOR_NO_WIFI;  // Red
                break;
            case LedState::WIFI_ONLY:
                color = COLOR_WIFI;     // Orange
                break;
            case LedState::WS_CONNECTED:
                color = COLOR_WS;       // Green
                break;
            default:
                color = COLOR_OFF;
                break;
        }
        _controller.setPixel(0, color);
    }
    
    _controller.show();
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
