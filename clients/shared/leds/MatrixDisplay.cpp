#include "MatrixDisplay.h"
#include "font5x3.h"
#include <Arduino.h>

// ─── Lifecycle ────────────────────────────────────────────────────────────────

MatrixDisplay::~MatrixDisplay() {
    _freeStrip();
}

void MatrixDisplay::_freeStrip() {
    delete _strip;
    _strip = nullptr;
}

bool MatrixDisplay::begin(const LedConfig& cfg) {
    _cfg        = cfg;
    _rows       = cfg.matrixRows > 0 ? cfg.matrixRows : 8;
    _cols       = cfg.matrixCols > 0 ? cfg.matrixCols : 8;
    _pixelCount = min((uint16_t)(cfg.ledCount), (uint16_t)MAX_PIXELS);

    if (_pixelCount == 0) {
        Serial.println("[MATRIX] ledCount=0 — matrix subsystem inactive");
        _available = false;
        return false;
    }

    _freeStrip();
    _strip = new LedStrip(_pixelCount, cfg.pin);
    _strip->Begin();
    _strip->Show();

    if (cfg.hasDeviceColor) {
        setDeviceColor(cfg.deviceColorR, cfg.deviceColorG, cfg.deviceColorB);
    }

    _available = true;
    Serial.printf("[MATRIX] Init: %u pixels, pin=%u, rows=%u, cols=%u\n",
                  _pixelCount, cfg.pin, _rows, _cols);
    return true;
}

void MatrixDisplay::applyConfig(const LedConfig& cfg) {
    if (!_available) { begin(cfg); return; }
    if (cfg.ledCount != _cfg.ledCount || cfg.pin != _cfg.pin) {
        begin(cfg);
    } else {
        _cfg = cfg;
        if (cfg.hasDeviceColor) {
            setDeviceColor(cfg.deviceColorR, cfg.deviceColorG, cfg.deviceColorB);
        }
        if (_strip) _strip->SetBrightness(cfg.brightness);
    }
}

void MatrixDisplay::setDeviceColor(uint8_t r, uint8_t g, uint8_t b) {
    _devR = r; _devG = g; _devB = b;
}

// ─── Animations ───────────────────────────────────────────────────────────────

void MatrixDisplay::loop() {
    if (!_available) return;

    switch (_mode) {
        case Mode::SCROLLING:
        case Mode::WINNER:
            _stepScroll();
            break;
        case Mode::BLINK:
            _stepBlink();
            break;
        case Mode::PULSE:
            _stepPulse();
            break;
        case Mode::CHASE:
            _stepChase();
            break;
        case Mode::SPARKLE:
            _stepSparkle();
            break;
        case Mode::IDLE: {
            // Simple rainbow: shift hue over time
            static uint8_t hueOffset = 0;
            static unsigned long lastRainbow = 0;
            unsigned long now = millis();
            if (now - lastRainbow >= 50) {
                lastRainbow = now;
                hueOffset++;
                if (_strip) {
                    for (uint16_t i = 0; i < _pixelCount; ++i) {
                        uint8_t hue = hueOffset + (uint8_t)(i * 256 / _pixelCount);
                        uint8_t segment   = hue / 43;
                        uint8_t remainder = (hue % 43) * 6;
                        uint8_t r = 0, gv = 0, b = 0;
                        switch (segment) {
                            case 0: r = 255; gv = remainder; b = 0; break;
                            case 1: r = 255 - remainder; gv = 255; b = 0; break;
                            case 2: r = 0; gv = 255; b = remainder; break;
                            case 3: r = 0; gv = 255 - remainder; b = 255; break;
                            case 4: r = remainder; gv = 0; b = 255; break;
                            default: r = 255; gv = 0; b = 255 - remainder; break;
                        }
                        _strip->SetPixelColor(i, RgbColor(r, gv, b));
                    }
                    _strip->Show();
                }
            }
            break;
        }
        default:
            break;
    }
}

// ─── Display primitives ───────────────────────────────────────────────────────

void MatrixDisplay::showNumber(uint8_t n, uint8_t r, uint8_t g, uint8_t b) {
    if (!_available || !_strip) return;
    if (n > 9) n = 9;
    clear();

    int8_t xOff = (_cols - 3) / 2;
    int8_t yOff = (_rows - 5) / 2;
    const uint8_t* glyph = FONT5x3[n];
    for (int8_t row = 0; row < 5; ++row) {
        for (int8_t col = 0; col < 3; ++col) {
            if (glyph[row] & (1 << (2 - col))) {
                _setPixel(yOff + row, xOff + col, r, g, b);
            }
        }
    }
    _show();
    _mode = Mode::NUMBER;
}

void MatrixDisplay::showCountdown(int n) {
    if (n > 0) {
        showNumber((uint8_t)n, 255, 255, 255);
    } else {
        fillColor(0, 220, 0);    // green "GO"
        _mode = Mode::NUMBER;
    }
}

void MatrixDisplay::fillColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!_available || !_strip) return;
    for (uint16_t i = 0; i < _pixelCount; ++i) {
        _strip->SetPixelColor(i, RgbColor(r, g, b));
    }
    _strip->Show();
    _mode = Mode::STATIC;  // stop IDLE rainbow from overwriting
}

void MatrixDisplay::clear() {
    if (!_available || !_strip) return;
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::STATIC;  // stop IDLE rainbow from overwriting
}

void MatrixDisplay::showText(
    const char* text, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs)
{
    if (!_available || !text) return;
    strlcpy(_scrollText, text, sizeof(_scrollText));
    uint8_t len       = strlen(_scrollText);
    _scrollTotalCols  = (int16_t)(_cols + len * 4 + _cols);
    _scrollOffset     = 0;
    _scrollR          = r;
    _scrollG          = g;
    _scrollB          = b;
    _scrollSpeed      = speedMs;
    _scrollLastStepMs = millis();
    _scrolling        = true;
    _mode             = Mode::SCROLLING;
}

void MatrixDisplay::showWinner(const char* name) {
    showText(name, 255, 200, 0, 60);   // gold colour
    _mode = Mode::WINNER;
}

void MatrixDisplay::showIdle() {
    _mode = Mode::IDLE;
}

void MatrixDisplay::showEffect(
    const char* effectName, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs)
{
    if (!_available) return;
    _animR        = r;
    _animG        = g;
    _animB        = b;
    _animSpeedMs  = speedMs > 0 ? speedMs : 500;
    _animLastStepMs = millis();

    if (strcmp(effectName, "rainbow") == 0) {
        _mode = Mode::IDLE;
    } else if (strcmp(effectName, "blink") == 0) {
        _animOn = true;
        fillColor(r, g, b);   // start with ON
        _mode = Mode::BLINK;
    } else if (strcmp(effectName, "pulse") == 0) {
        _animPhase = 0;
        _mode = Mode::PULSE;
    } else if (strcmp(effectName, "chase") == 0) {
        _animChasePos = 0;
        _strip->ClearTo(RgbColor(0));
        _strip->Show();
        _mode = Mode::CHASE;
    } else if (strcmp(effectName, "sparkle") == 0) {
        _strip->ClearTo(RgbColor(0));
        _strip->Show();
        _mode = Mode::SPARKLE;
    } else {
        // solid and anything unknown: one-shot fill
        fillColor(r, g, b);
    }
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void MatrixDisplay::_setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    if (row >= _rows || col >= _cols || !_strip) return;
    uint16_t idx;
    bool zigzag = (_cfg.topology == LedTopology::MATRIX_ZIGZAG);
    if (zigzag && (row % 2 == 1)) {
        idx = (uint16_t)row * _cols + (_cols - 1 - col);
    } else {
        idx = (uint16_t)row * _cols + col;
    }
    if (idx < _pixelCount) {
        _strip->SetPixelColor(idx, RgbColor(r, g, b));
    }
}

void MatrixDisplay::_show() {
    if (_strip) _strip->Show();
}

void MatrixDisplay::_stepScroll() {
    if (!_scrolling) return;
    unsigned long now = millis();
    if (now - _scrollLastStepMs < _scrollSpeed) return;
    _scrollLastStepMs = now;

    if (!_available || !_strip) return;
    _strip->ClearTo(RgbColor(0));

    for (int16_t screenCol = 0; screenCol < _cols; ++screenCol) {
        int16_t srcCol   = _scrollOffset + screenCol - (int16_t)_cols;
        if (srcCol < 0) continue;
        uint8_t  len     = strlen(_scrollText);
        int16_t charIdx  = srcCol / 4;
        int16_t colInChar = srcCol % 4;
        if (charIdx >= (int16_t)len) continue;

        char c  = _scrollText[charIdx];
        int  fi = font5x3Index(c);
        if (fi < 0 || colInChar >= 3) continue;

        const uint8_t* glyph = FONT5x3[fi];
        int8_t yOff = (_rows - 5) / 2;
        for (int8_t row = 0; row < 5; ++row) {
            if (glyph[row] & (1 << (2 - colInChar))) {
                _setPixel(yOff + row, screenCol, _scrollR, _scrollG, _scrollB);
            }
        }
    }
    _strip->Show();

    _scrollOffset++;
    if (_scrollOffset >= _scrollTotalCols) {
        if (_mode == Mode::WINNER) {
            _scrollOffset = 0;    // loop winner animation
        } else {
            _scrolling = false;
            _mode      = Mode::IDLE;
        }
    }
}

void MatrixDisplay::_stepBlink() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < _animSpeedMs) return;
    _animLastStepMs = now;

    _animOn = !_animOn;
    if (_animOn) {
        for (uint16_t i = 0; i < _pixelCount; ++i)
            _strip->SetPixelColor(i, RgbColor(_animR, _animG, _animB));
    } else {
        _strip->ClearTo(RgbColor(0));
    }
    _strip->Show();
}

void MatrixDisplay::_stepPulse() {
    if (!_strip) return;
    // Update at ~40 Hz. Speed controls full cycle duration (ms).
    unsigned long now = millis();
    if (now - _animLastStepMs < 25) return;
    _animLastStepMs = now;

    // Phase 0..511: triangle 0→255→0 (brightness). Steps per frame:
    uint16_t stepsPerFrame = (uint16_t)max(1, (int)(_animSpeedMs / (512 / 25 + 1)));
    _animPhase = (_animPhase + stepsPerFrame) % 512;
    uint16_t brightness = _animPhase < 256 ? _animPhase : 511 - _animPhase;

    for (uint16_t i = 0; i < _pixelCount; ++i) {
        _strip->SetPixelColor(i, RgbColor(
            (uint8_t)((uint16_t)_animR * brightness / 255),
            (uint8_t)((uint16_t)_animG * brightness / 255),
            (uint8_t)((uint16_t)_animB * brightness / 255)));
    }
    _strip->Show();
}

void MatrixDisplay::_stepChase() {
    if (!_strip || _pixelCount == 0) return;
    unsigned long now = millis();
    uint16_t stepMs = max((uint16_t)20, (uint16_t)(_animSpeedMs / _pixelCount));
    if (now - _animLastStepMs < stepMs) return;
    _animLastStepMs = now;

    _strip->ClearTo(RgbColor(0));
    _strip->SetPixelColor(_animChasePos % _pixelCount, RgbColor(_animR, _animG, _animB));
    _strip->Show();
    _animChasePos = (_animChasePos + 1) % _pixelCount;
}

void MatrixDisplay::_stepSparkle() {
    if (!_strip || _pixelCount == 0) return;
    unsigned long now = millis();
    uint16_t stepMs = max((uint16_t)20, (uint16_t)(_animSpeedMs / 8));
    if (now - _animLastStepMs < stepMs) return;
    _animLastStepMs = now;

    // Fade all pixels toward black, ignite one random new one
    for (uint16_t i = 0; i < _pixelCount; ++i)
        _strip->SetPixelColor(i, RgbColor(0));
    uint16_t idx = (uint16_t)(random(0, _pixelCount));
    _strip->SetPixelColor(idx, RgbColor(_animR, _animG, _animB));
    _strip->Show();
}
