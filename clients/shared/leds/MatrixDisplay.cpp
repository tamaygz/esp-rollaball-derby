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
        case Mode::BALLROLL:
            _stepBallRoll();
            break;
        case Mode::CAMELCHEW:
            _stepCamelChew();
            break;
        case Mode::CAMELSPEEDUP:
            _stepCamelSpeedup();
            break;
        case Mode::SCOREHIGH:
            _stepScoreHigh();
            break;
        case Mode::TOLEAD:
            _stepToLead();
            break;
        case Mode::FARBEHIND:
            _stepFarBehind();
            break;
        case Mode::GAMESTART:
            _stepGameStart();
            break;
        case Mode::GAMEEND:
            _stepGameEnd();
            break;
        case Mode::SOMEONEWON:
            _stepSomeoneWon();
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
    } else if (strcmp(effectName, "ballroll") == 0) {
        showBallRoll(r, g, b, speedMs);
    } else {
        // solid and anything unknown: one-shot fill
        fillColor(r, g, b);
    }
}

void MatrixDisplay::showBallRoll(
    uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs)
{
    if (!_available || !_strip) return;

    _brR       = r;
    _brG       = g;
    _brB       = b;
    _brSpeedMs = speedMs > 0 ? speedMs : 2000;

    // Three target holes near the bottom of the matrix, evenly spread
    _brHoles[0] = { (uint8_t)(_rows - 1), 1 };
    _brHoles[1] = { (uint8_t)(_rows - 1), (uint8_t)(_cols / 2) };
    _brHoles[2] = { (uint8_t)(_rows - 1), (uint8_t)(_cols - 2) };

    // Ball starts at top-centre
    _brStartRowFP   = 0;
    _brStartColFP   = (int32_t)(_cols / 2) << 8;
    _brBallRowFP    = _brStartRowFP;
    _brBallColFP    = _brStartColFP;
    _brTarget       = (uint8_t)(random(0, 3));
    _brTargRowFP    = (int32_t)_brHoles[_brTarget].row << 8;
    _brTargColFP    = (int32_t)_brHoles[_brTarget].col << 8;

    _brPhase        = BallPhase::ROLLING;
    _brPhaseStartMs = millis();
    _animLastStepMs = millis();

    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::BALLROLL;
}

void MatrixDisplay::_stepBallRoll() {
    if (!_strip) return;

    // Cap frame rate at ~60 fps
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;

    unsigned long elapsed = now - _brPhaseStartMs;

    switch (_brPhase) {

        case BallPhase::ROLLING: {
            // Ease-in (t²) interpolation from start to target hole
            float t  = min(1.0f, (float)elapsed / (float)_brSpeedMs);
            float te = t * t;

            int8_t ballRow = (int8_t)((_brStartRowFP + (int32_t)(te * (float)(_brTargRowFP - _brStartRowFP))) >> 8);
            int8_t ballCol = (int8_t)((_brStartColFP + (int32_t)(te * (float)(_brTargColFP - _brStartColFP))) >> 8);

            _strip->ClearTo(RgbColor(0));
            // Dim trail one step behind
            if (ballRow > 0) {
                _setPixel(ballRow - 1, ballCol, _brR / 4, _brG / 4, _brB / 4);
            }
            _setPixel(ballRow, ballCol, _brR, _brG, _brB);
            _strip->Show();

            if (t >= 1.0f) {
                _brPhase        = BallPhase::DROPPING;
                _brPhaseStartMs = now;
            }
            break;
        }

        case BallPhase::DROPPING: {
            // Three quick flashes at the hole (120 ms each), then celebrate
            static const uint16_t FLASH_MS  = 120;
            static const uint8_t  FLASHES   = 3;
            uint8_t flashIdx = (uint8_t)(elapsed / FLASH_MS);

            if (flashIdx >= FLASHES * 2) {
                _brPhase        = BallPhase::CELEBRATING;
                _brPhaseStartMs = now;
                break;
            }

            _strip->ClearTo(RgbColor(0));
            if (flashIdx % 2 == 0) {
                // Fade brightness within each flash window
                uint8_t bv = (uint8_t)(255 - (elapsed % FLASH_MS) * 255 / FLASH_MS);
                _setPixel(_brHoles[_brTarget].row, _brHoles[_brTarget].col,
                          (uint8_t)((uint16_t)_brR * bv / 255),
                          (uint8_t)((uint16_t)_brG * bv / 255),
                          (uint8_t)((uint16_t)_brB * bv / 255));
            }
            _strip->Show();
            break;
        }

        case BallPhase::CELEBRATING: {
            static const uint16_t CELEBRATE_MS = 600;
            float t       = min(1.0f, (float)elapsed / (float)CELEBRATE_MS);
            float maxRad  = (float)max(_rows, _cols) * 0.5f;
            float radius  = t * maxRad;

            float cx = (float)_brHoles[_brTarget].row;
            float cy = (float)_brHoles[_brTarget].col;

            _strip->ClearTo(RgbColor(0));
            for (uint8_t row = 0; row < _rows; ++row) {
                for (uint8_t col = 0; col < _cols; ++col) {
                    float dr   = (float)row - cx;
                    float dc   = (float)col - cy;
                    float dist = sqrtf(dr * dr + dc * dc);
                    float diff = fabsf(dist - radius);
                    if (diff < 0.9f) {
                        float bright = (1.0f - diff / 0.9f) * (1.0f - t * 0.6f);
                        uint8_t bv = (uint8_t)(bright * 255);
                        _setPixel(row, col,
                                  (uint8_t)((uint16_t)_brR * bv / 255),
                                  (uint8_t)((uint16_t)_brG * bv / 255),
                                  (uint8_t)((uint16_t)_brB * bv / 255));
                    }
                }
            }
            _strip->Show();

            if (t >= 1.0f) {
                _brPhase        = BallPhase::PAUSE;
                _brPhaseStartMs = now;
                _strip->ClearTo(RgbColor(0));
                _strip->Show();
            }
            break;
        }

        case BallPhase::PAUSE: {
            // 400 ms dark pause, then restart with a different hole
            if (elapsed < 400) break;

            uint8_t newTarget = (uint8_t)(random(0, 3));
            // Prefer a different hole for variety
            if (_rows > 0 && newTarget == _brTarget) {
                newTarget = (newTarget + 1) % 3;
            }
            _brTarget     = newTarget;
            _brStartRowFP = 0;
            _brStartColFP = (int32_t)(_cols / 2) << 8;
            _brBallRowFP  = _brStartRowFP;
            _brBallColFP  = _brStartColFP;
            _brTargRowFP  = (int32_t)_brHoles[_brTarget].row << 8;
            _brTargColFP  = (int32_t)_brHoles[_brTarget].col << 8;

            _brPhase        = BallPhase::ROLLING;
            _brPhaseStartMs = now;
            break;
        }
    }
}

// ─── Private helpers ──────────────────────────────────────────────────────────

void MatrixDisplay::showCamelChew(
    uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs)
{
    if (!_available || !_strip) return;

    _ccHayR        = r;
    _ccHayG        = g;
    _ccHayB        = b;
    _ccChewCycleMs = speedMs > 0 ? speedMs : 2400;
    _ccChewStartMs = millis();

    // Initialize blink/ear timers
    _ccBlinkTimer = millis() + 2500 + random(0, 2500);
    _ccEarTimer   = millis() + 3500 + random(0, 3500);

    _animLastStepMs = 0;  // force first frame immediately
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::CAMELCHEW;
}

void MatrixDisplay::_stepCamelChew() {
    if (!_strip) return;

    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;  // ~60 fps cap
    _animLastStepMs = now;

    unsigned long cycleElapsed = (now - _ccChewStartMs) % _ccChewCycleMs;
    float rawT = (float)cycleElapsed / (float)_ccChewCycleMs;

    // Mouth open curve: 4-phase chew
    float mouthOpen;
    if (rawT < 0.38f) {
        mouthOpen = 0.0f;
    } else if (rawT < 0.56f) {
        float t0 = (rawT - 0.38f) / 0.18f;
        mouthOpen = t0 * t0;
    } else if (rawT < 0.84f) {
        float t1 = (rawT - 0.56f) / 0.28f;
        mouthOpen = 0.85f + sinf(t1 * M_PI * 3.0f) * 0.15f;
    } else {
        float t2 = (rawT - 0.84f) / 0.16f;
        mouthOpen = (1.0f - t2) * (1.0f - t2) * 0.85f;
    }

    // Blink state
    bool blinking = false;
    if (now >= _ccBlinkTimer) {
        unsigned long blinkAge = now - _ccBlinkTimer;
        if (blinkAge < 120) {
            blinking = true;
        } else {
            _ccBlinkTimer = now + 2500 + random(0, 3000);
        }
    }

    // Ear flick state
    uint8_t earFlickSide = 3;
    if (now >= _ccEarTimer) {
        unsigned long earAge = now - _ccEarTimer;
        if (earAge < 220) {
            earFlickSide = _ccEarSide;
        } else {
            _ccEarSide = (uint8_t)(random(0, 3));
            _ccEarTimer = now + 3500 + random(0, 4000);
        }
    }

    // Draw frame
    _strip->ClearTo(RgbColor(200, 140, 55));
    uint8_t mid = _cols / 2;

    // Ears (rows 0-1)
    RgbColor earColL = (earFlickSide != 1) ? RgbColor(235, 175, 70) : RgbColor(255, 210, 110);
    RgbColor earColR = (earFlickSide != 0) ? RgbColor(235, 175, 70) : RgbColor(255, 210, 110);
    for (uint8_t er = 0; er <= 1 && er < _rows; ++er) {
        _setPixel(er, 0, earColL.R, earColL.G, earColL.B);
        if (_cols > 1) _setPixel(er, 1, earColL.R, earColL.G, earColL.B);
        if (_cols > 2) _setPixel(er, _cols - 2, earColR.R, earColR.G, earColR.B);
        if (_cols > 3) _setPixel(er, _cols - 1, earColR.R, earColR.G, earColR.B);
    }

    // Eyes
    uint8_t eyeRow = (_rows * 28) / 100;
    uint8_t eyeColL = (_cols * 25) / 100;
    uint8_t eyeColR = (_cols * 625) / 1000;
    RgbColor eyeCol = blinking ? RgbColor(200, 140, 55) : RgbColor(28, 16, 6);
    if (eyeRow < _rows && eyeColL < _cols) _setPixel(eyeRow, eyeColL, eyeCol.R, eyeCol.G, eyeCol.B);
    if (eyeRow < _rows && eyeColR < _cols) _setPixel(eyeRow, eyeColR, eyeCol.R, eyeCol.G, eyeCol.B);

    // Nostrils
    uint8_t nosRow = (_rows * 50) / 100;
    if (nosRow < _rows && eyeColL < _cols) _setPixel(nosRow, eyeColL, 55, 32, 12);
    if (nosRow < _rows && eyeColR < _cols) _setPixel(nosRow, eyeColR, 55, 32, 12);

    // Lips
    uint8_t lipRow = (_rows * 625) / 1000;
    if (lipRow < _rows) {
        for (uint8_t lc = 1; lc < _cols - 1; ++lc) {
            _setPixel(lipRow, lc, 155, 95, 30);
        }
    }

    // Jaw
    uint8_t jawRow = (_rows * 75) / 100;
    if (jawRow < _rows) {
        uint8_t jawR = (mouthOpen < 0.05f)
            ? 155
            : (uint8_t)(155.0f + (18.0f - 155.0f) * mouthOpen);
        uint8_t jawG = (mouthOpen < 0.05f)
            ? 95
            : (uint8_t)(95.0f + (10.0f - 95.0f) * mouthOpen);
        uint8_t jawB = (mouthOpen < 0.05f)
            ? 30
            : (uint8_t)(30.0f + (4.0f - 30.0f) * mouthOpen);
        for (uint8_t jc = 1; jc < _cols - 1; ++jc) {
            _setPixel(jawRow, jc, jawR, jawG, jawB);
        }
    }

    // Hay in mouth
    if (mouthOpen > 0.3f && jawRow < _rows) {
        float hayAlpha = (mouthOpen - 0.3f) / 0.4f;
        if (hayAlpha > 1.0f) hayAlpha = 1.0f;
        uint8_t hayR = (uint8_t)(_ccHayR * hayAlpha);
        uint8_t hayG = (uint8_t)(_ccHayG * hayAlpha);
        uint8_t hayB = (uint8_t)(_ccHayB * hayAlpha);

        if (mid > 0) _setPixel(jawRow, mid - 1, hayR, hayG, hayB);
        _setPixel(jawRow, mid, hayR, hayG, hayB);
        if (mid + 1 < _cols) _setPixel(jawRow, mid + 1, hayR, hayG, hayB);

        if (mouthOpen > 0.6f && lipRow < _rows) {
            uint8_t haySoft_R = (uint8_t)(hayR * 0.5f);
            uint8_t haySoft_G = (uint8_t)(hayG * 0.5f);
            uint8_t haySoft_B = (uint8_t)(hayB * 0.5f);
            if (mid > 0) _setPixel(lipRow, mid - 1, haySoft_R, haySoft_G, haySoft_B);
            _setPixel(lipRow, mid, haySoft_R, haySoft_G, haySoft_B);
        }
    }

    // Chin
    if (_rows > 0) {
        uint8_t chinRow = _rows - 1;
        for (uint8_t cc = 0; cc < _cols; ++cc) {
            _setPixel(chinRow, cc, 175, 120, 45);
        }
    }

    _strip->Show();
}

void MatrixDisplay::_setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
    if (row >= _rows || col >= _cols || !_strip) return;
    if (_cfg.mirrorH) col = _cols - 1 - col;
    if (_cfg.mirrorV) row = _rows - 1 - row;
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

// ─── New effects (7 game event + environment animations) ──────────────────────

void MatrixDisplay::showCamelSpeedup(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs) {
    if (!_available || !_strip) return;
    _csR = r; _csG = g; _csB = b;
    _csSpeedMs = speedMs > 0 ? speedMs : 1500;
    _csPhaseStartMs = millis();
    _csPhase = 0;
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::CAMELSPEEDUP;
}

void MatrixDisplay::_stepCamelSpeedup() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _csPhaseStartMs;
    float t = min(1.0f, (float)elapsed / (float)_csSpeedMs);
    
    _strip->ClearTo(RgbColor(0));
    
    if (t < 0.35f) {
        // Fart phase: tail raised, fart cloud
        float fartT = t / 0.35f;
        uint8_t camelRow = _rows / 2;
        uint8_t camelCol = _cols / 4;
        _setPixel(camelRow, camelCol, _csR, _csG, _csB);
        if (camelRow > 0 && camelCol + 1 < _cols) {
            _setPixel(camelRow - 1, camelCol + 1, _csR, _csG, _csB);  // tail
        }
        uint8_t puffCol = camelCol + 2 + (uint8_t)(fartT * 2);
        _setPixel(camelRow, min((uint8_t)(_cols - 1), puffCol), 
                  (uint8_t)(_csR * (1 - fartT)), 
                  (uint8_t)(_csG * (1 - fartT) * 0.7f),
                  (uint8_t)(_csB * (1 - fartT) * 0.5f));
    } else {
        // Burst phase: camel zooms right with motion blur
        float burstT = (t - 0.35f) / 0.65f;
        uint8_t camelRow = _rows / 2;
        int16_t camelCol = (_cols / 4) + (int16_t)(burstT * (_cols * 0.6f));
        if (camelCol >= 0 && camelCol < _cols) {
            _setPixel(camelRow, (uint8_t)camelCol, _csR, _csG, _csB);
            // Motion blur trails
            for (uint8_t i = 1; i <= 3; ++i) {
                int16_t trailCol = (int16_t)camelCol - (int16_t)i;
                if (trailCol >= 0 && trailCol < _cols) {
                    uint8_t alpha = (uint8_t)(255 - i * 80);
                    _setPixel(camelRow, (uint8_t)trailCol,
                              (uint8_t)(_csR * alpha / 255),
                              (uint8_t)(_csG * alpha / 255 * 0.5f),
                              (uint8_t)(_csB * alpha / 255 * 0.3f));
                }
            }
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _csPhaseStartMs = now; }
}

void MatrixDisplay::showScoreHigh(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs) {
    if (!_available || !_strip) return;
    _shR = r; _shG = g; _shB = b;
    _shSpeedMs = speedMs > 0 ? speedMs : 1200;
    _shPhaseStartMs = millis();
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::SCOREHIGH;
}

void MatrixDisplay::_stepScoreHigh() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _shPhaseStartMs;
    float t = min(1.0f, (float)elapsed / (float)_shSpeedMs);
    
    _strip->ClearTo(RgbColor(0));
    
    uint8_t centerRow = _rows / 2;
    uint8_t centerCol = _cols / 2;
    float radius = t * (_rows / 2 + 2);
    float ringWidth = 1.2f;
    
    // Expanding ring
    for (uint8_t row = 0; row < _rows; ++row) {
        for (uint8_t col = 0; col < _cols; ++col) {
            float dr = (float)row - centerRow;
            float dc = (float)col - centerCol;
            float dist = sqrtf(dr * dr + dc * dc);
            if (fabsf(dist - radius) < ringWidth) {
                float fade = 1.0f - t * t;
                _setPixel(row, col,
                          (uint8_t)(_shR * fade),
                          (uint8_t)(_shG * fade),
                          (uint8_t)(_shB * fade));
            }
        }
    }
    
    // Center sparkles
    if (t > 0.2f) {
        _setPixel(centerRow, centerCol, _shR, _shG, _shB);
        if (centerRow > 0 && (random(0, 100) < 40)) {
            _setPixel(centerRow - 1, centerCol, 
                      (uint8_t)(_shR * 0.8f),
                      (uint8_t)(_shG * 0.8f),
                      (uint8_t)(_shB * 0.8f));
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _shPhaseStartMs = now; }
}

void MatrixDisplay::showToLead(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs) {
    if (!_available || !_strip) return;
    _tlR = r; _tlG = g; _tlB = b;
    _tlSpeedMs = speedMs > 0 ? speedMs : 1000;
    _tlPhaseStartMs = millis();
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::TOLEAD;
}

void MatrixDisplay::_stepToLead() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _tlPhaseStartMs;
    float t = min(1.0f, (float)elapsed / (float)_tlSpeedMs);
    
    _strip->ClearTo(RgbColor(0));
    
    uint8_t crownRow = (_rows * 25) / 100;
    uint8_t markerCol = (_cols * 75) / 100;
    float pulse = sinf(t * M_PI * 4) * 0.5f + 0.5f;
    float crownAlpha = min(1.0f, t * 2) * pulse;
    
    // Crown peaks
    uint8_t c_r = (uint8_t)(_tlR * crownAlpha);
    uint8_t c_g = (uint8_t)(_tlG * crownAlpha);
    uint8_t c_b = (uint8_t)(_tlB * crownAlpha);
    
    if (markerCol > 0) _setPixel(crownRow, markerCol - 1, c_r, c_g, c_b);
    _setPixel(crownRow, markerCol, c_r, c_g, c_b);
    if (markerCol + 1 < _cols) _setPixel(crownRow, markerCol + 1, c_r, c_g, c_b);
    if (crownRow > 0) _setPixel(crownRow - 1, markerCol, c_r, c_g, c_b);
    
    // "1st" marker flashing
    if (sinf(t * M_PI * 3) > 0) {
        uint8_t marker_row = crownRow + 2;
        if (marker_row < _rows) {
            if (markerCol > 0) _setPixel(marker_row, markerCol - 1, _tlR, _tlG, _tlB);
            _setPixel(marker_row, markerCol, _tlR, _tlG, _tlB);
            if (markerCol + 1 < _cols) _setPixel(marker_row, markerCol + 1, _tlR, _tlG, _tlB);
        }
    }
    
    // Halo glow expanding
    float haloAlpha = max(0.0f, 1.0f - t);
    for (uint8_t offset = 1; offset <= 3; ++offset) {
        float intensity = haloAlpha / (offset + 1);
        if (intensity > 0 && crownRow >= offset) {
            _setPixel(crownRow - offset, markerCol,
                      (uint8_t)(_tlR * intensity),
                      (uint8_t)(_tlG * intensity),
                      (uint8_t)(_tlB * intensity));
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _tlPhaseStartMs = now; }
}

void MatrixDisplay::showFarBehind(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs) {
    if (!_available || !_strip) return;
    _fbR = r; _fbG = g; _fbB = b;
    _fbSpeedMs = speedMs > 0 ? speedMs : 1800;
    _fbPhaseStartMs = millis();
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::FARBEHIND;
}

void MatrixDisplay::_stepFarBehind() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _fbPhaseStartMs;
    float t = min(1.0f, (float)elapsed / (float)_fbSpeedMs);
    
    _strip->ClearTo(RgbColor(0));
    
    uint8_t camelRow = _rows / 2;
    uint8_t camelCol = (_cols * 15) / 100;
    
    // Tired drooping camel
    _setPixel(camelRow, camelCol, _fbR, _fbG, _fbB);
    if (camelRow > 0 && camelCol > 0) {
        _setPixel(camelRow - 1, camelCol - 1, _fbR, _fbG, _fbB);  // drooping head
    }
    
    // Sweat beads: fade in/out
    float sweatAlpha = sinf(t * M_PI * 3) * 0.5f + 0.35f;
    if (sweatAlpha < 0.0f) sweatAlpha = 0.0f;
    uint8_t sw_r = (uint8_t)(0.8f * _fbR * sweatAlpha);
    uint8_t sw_g = (uint8_t)(0.9f * _fbG * sweatAlpha);
    uint8_t sw_b = (uint8_t)(_fbB * sweatAlpha);
    
    if (camelRow >= 2 && camelCol + 1 < _cols) {
        _setPixel(camelRow - 2, camelCol + 1, sw_r, sw_g, sw_b);
    }
    if (camelRow > 0 && camelCol + 2 < _cols) {
        _setPixel(camelRow - 1, camelCol + 2, sw_r, sw_g, sw_b);
    }
    
    // Faint "ahead" track marker on right
    uint8_t ahead_r = (_fbR * 30) / 100;
    uint8_t ahead_g = (_fbG * 30) / 100;
    uint8_t ahead_b = (_fbB * 30) / 100;
    for (uint8_t c = (_cols * 70) / 100; c < _cols; ++c) {
        if ((c - _cols + 1) % 2 == 0) {
            _setPixel(camelRow, c, ahead_r, ahead_g, ahead_b);
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _fbPhaseStartMs = now; }
}

void MatrixDisplay::showGameStart() {
    if (!_available || !_strip) return;
    _gsPhaseStartMs = millis();
    _gsPhase = 0;
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::GAMESTART;
}

void MatrixDisplay::_stepGameStart() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _gsPhaseStartMs;
    uint16_t phaseDur = 400;
    
    if (elapsed >= phaseDur) {
        _gsPhaseStartMs = now;
        _gsPhase = (_gsPhase + 1) % 4;
        elapsed = 0;
    }
    
    float phaseFrac = (float)elapsed / (float)phaseDur;
    
    _strip->ClearTo(RgbColor(0));
    
    if (_gsPhase == 0) {
        // Checkered flag
        for (uint8_t r = 0; r < _rows; ++r) {
            for (uint8_t c = 0; c < _cols; ++c) {
                if ((r + c) % 2 == 0) {
                    _setPixel(r, c, 255, 255, 255);
                }
            }
        }
    } else if (_gsPhase == 1) {
        // Ready - top half orange
        for (uint8_t r = 0; r < (_rows / 2); ++r) {
            for (uint8_t c = 0; c < _cols; ++c) {
                _setPixel(r, c, 255, 165, 0);
            }
        }
    } else if (_gsPhase == 2) {
        // Set - middle yellow
        uint8_t midRow = _rows / 2;
        for (uint8_t c = 0; c < _cols; ++c) {
            if (midRow > 0) _setPixel(midRow - 1, c, 255, 255, 0);
            if (midRow < _rows) _setPixel(midRow, c, 255, 255, 0);
        }
    } else {
        // Go! - all green pulsing
        float pulse = sinf(phaseFrac * M_PI * 4) * 0.5f + 0.5f;
        for (uint8_t r = 0; r < _rows; ++r) {
            for (uint8_t c = 0; c < _cols; ++c) {
                _setPixel(r, c, 
                          (uint8_t)(0 * pulse),
                          (uint8_t)(255 * pulse),
                          (uint8_t)(0 * pulse));
            }
        }
    }
    _strip->Show();
}

void MatrixDisplay::showGameEnd() {
    if (!_available || !_strip) return;
    _gePhaseStartMs = millis();
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::GAMEEND;
}

void MatrixDisplay::_stepGameEnd() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _gePhaseStartMs;
    float t = min(1.0f, (float)elapsed / 800.0f);
    
    _strip->ClearTo(RgbColor(0));
    
    uint16_t curtainWidth = (uint16_t)(t * (_cols / 2));
    
    // Curtain closes from left and right
    for (uint8_t r = 0; r < _rows; ++r) {
        for (uint8_t c = 0; c < _cols; ++c) {
            uint16_t distLeft = c;
            uint16_t distRight = _cols - 1 - c;
            
            if (distLeft < curtainWidth || distRight < curtainWidth) {
                uint8_t shade = (uint8_t)(255 * (1 - t * 0.8f));
                _setPixel(r, c, shade, (uint8_t)(shade * 0.3f), (uint8_t)(shade * 0.3f));
            }
        }
    }
    
    // Finish line in center
    if (t > 0.3f) {
        float finishAlpha = min(1.0f, (t - 0.3f) / 0.2f);
        uint8_t fin_r = (uint8_t)(255 * finishAlpha);
        uint8_t fin_g = (uint8_t)(255 * finishAlpha);
        uint8_t midCol = _cols / 2;
        for (uint8_t rf = 0; rf < _rows; ++rf) {
            if (midCol > 0) _setPixel(rf, midCol - 1, fin_r, fin_g, 0);
            if (midCol < _cols) _setPixel(rf, midCol, fin_r, fin_g, 0);
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _gePhaseStartMs = now; }
}

void MatrixDisplay::showSomeoneWon(uint8_t r, uint8_t g, uint8_t b) {
    if (!_available || !_strip) return;
    _swR = r; _swG = g; _swB = b;
    _swPhaseStartMs = millis();
    _animLastStepMs = millis();
    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::SOMEONEWON;
}

void MatrixDisplay::_stepSomeoneWon() {
    if (!_strip) return;
    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;
    _animLastStepMs = now;
    
    unsigned long elapsed = now - _swPhaseStartMs;
    float t = min(1.0f, (float)elapsed / 1500.0f);
    
    _strip->ClearTo(RgbColor(0));
    
    uint8_t trophyRow = (_rows * 35) / 100;
    uint8_t trophyCol = _cols / 2;
    
    // Trophy pulses and grows
    float pulseSize = sinf(t * M_PI * 5) * 0.3f + 0.7f;
    uint8_t trophyAlpha = (uint8_t)(pulseSize * 255);
    uint8_t t_r = (_swR * trophyAlpha) / 255;
    uint8_t t_g = (_swG * trophyAlpha) / 255;
    uint8_t t_b = (_swB * trophyAlpha) / 255;
    
    // Draw simple trophy
    if (trophyCol > 0) _setPixel(trophyRow, trophyCol - 1, t_r, t_g, t_b);
    if (trophyCol < _cols) _setPixel(trophyRow, trophyCol, t_r, t_g, t_b);
    if (trophyCol + 1 < _cols) _setPixel(trophyRow, trophyCol + 1, t_r, t_g, t_b);
    if (trophyRow + 1 < _rows) _setPixel(trophyRow + 1, trophyCol, t_r, t_g, t_b);
    if (trophyRow + 2 < _rows) {
        if (trophyCol > 0) _setPixel(trophyRow + 2, trophyCol - 1, t_r, t_g, t_b);
        _setPixel(trophyRow + 2, trophyCol, t_r, t_g, t_b);
        if (trophyCol + 1 < _cols) _setPixel(trophyRow + 2, trophyCol + 1, t_r, t_g, t_b);
    }
    
    // Confetti burst radiating
    for (uint8_t i = 0; i < 8; ++i) {
        float angle = (float)i / 8.0f * M_PI * 2 + t * M_PI * 4;
        float cx = cosf(angle);
        float sy = sinf(angle);
        float dist = t * 3;
        int16_t cCol = trophyCol + (int16_t)(cx * dist);
        int16_t cRow = trophyRow + (int16_t)(sy * dist);
        
        if (cCol >= 0 && cCol < _cols && cRow >= 0 && cRow < _rows) {
            float confAlpha = max(0.0f, 1.0f - t);
            _setPixel((uint8_t)cRow, (uint8_t)cCol,
                      (uint8_t)(_swR * confAlpha * 0.8f),
                      (uint8_t)(_swG * confAlpha * 0.6f),
                      (uint8_t)(_swB * confAlpha * 0.4f));
        }
    }
    _strip->Show();
    if (t >= 1.0f) { _swPhaseStartMs = now; }
}
