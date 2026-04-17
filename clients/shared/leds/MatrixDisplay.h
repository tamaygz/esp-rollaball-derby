#pragma once
#include "LedConfig.h"
#include "LedPlatform.h"
#include <Arduino.h>

/**
 * MatrixDisplay — WS2812B LED matrix renderer backed by NeoPixelBus.
 *
 * Platform note: uses the LedStrip / LedMethod type aliases from LedPlatform.h
 * so the correct hardware driver (UART1/DMA on ESP8266, RMT on ESP32) is
 * selected at compile time rather than using the generic Neo800KbpsMethod.
 *
 * Features:
 *  - Single digit / number display with 5×3 pixel font
 *  - Full-matrix colour fill and clear
 *  - Non-blocking scrolling text banner
 *  - Countdown (3/2/1 in white → 0 = green "GO")
 *  - Winner animation (gold scrolling name, loops until stopped)
 *  - Ambient idle rainbow wave
 *  - Supports MATRIX_ZIGZAG and MATRIX_PROGRESSIVE wiring topologies
 */
class MatrixDisplay {
public:
    MatrixDisplay() = default;
    ~MatrixDisplay();

    // Initialise the strip from a LedConfig received from the server.
    // Returns false if ledCount == 0 or allocation fails.
    bool begin(const LedConfig& cfg);

    // Non-blocking update: advance animations / scrolling.  Call every loop().
    void loop();

    // Display a single digit or small number centred on the matrix.
    void showNumber(uint8_t n, uint8_t r, uint8_t g, uint8_t b);

    // Fill every pixel with one colour.
    void fillColor(uint8_t r, uint8_t g, uint8_t b);

    // Clear all pixels (turn off).
    void clear();

    // Countdown display: n > 0 = white digit, n == 0 = green "GO".
    void showCountdown(int n);

    // Non-blocking scrolling text.
    // speedMs = ms per pixel-column scroll step (default 80 ms).
    void showText(const char* text, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 80);

    // Winner animation: gold scrolling name, loops continuously.
    void showWinner(const char* name);

    // Start ambient rainbow wave animation.
    void showIdle();

    // Play a named effect with colour and speed — handles all strip + matrix effects.
    // effectName: "solid"|"blink"|"pulse"|"chase"|"sparkle"|"rainbow"
    void showEffect(const char* effectName, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs);

    // Ball-roll animation: rolls a ball from bottom-centre toward a randomly
    // chosen V-shaped hole, drops it in, then shows an expanding-ring celebration.
    // Loops automatically. speedMs controls roll duration (default 2000 ms).
    void showBallRoll(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 2000);

    // Camel-chew animation: close-up camel face with animated jaw, hay in mouth,
    // blinking eyes, ear flicks. Loops automatically. Color is hay colour,
    // speedMs controls full chew cycle duration (default 2400 ms).
    void showCamelChew(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 2400);

    // Camel speeding up: tail raised, fart fume cloud, then burst of speed with motion lines.
    // Dynamic humor effect. speedMs controls total animation duration (default 1500 ms).
    void showCamelSpeedup(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 1500);

    // High score celebration: expanding rings of sparkles, pulsing center.
    // Triggers on player high score. speedMs controls animation duration (default 1200 ms).
    void showScoreHigh(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 1200);

    // Player took the lead: crown icon at top, "1st" marker, pulsing halo.
    // Celebration when player overtakes. speedMs controls animation duration (default 1000 ms).
    void showToLead(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 1000);

    // Player far behind: drooping tired camel, sweat beads, faint track ahead.
    // Visual indication of trailing player. speedMs controls animation duration (default 1800 ms).
    void showFarBehind(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 1800);

    // Game start sequence: checkered flag, "Ready", "Set", "Go!" phases.
    // Plays once at race start.
    void showGameStart();

    // Game end sequence: curtain close effect, finish line in center.
    // Plays once when race concludes.
    void showGameEnd();

    // Someone won: trophy icon with pulsing celebration, confetti burst.
    // Plays when winner is determined.
    void showSomeoneWon(uint8_t r, uint8_t g, uint8_t b);

    // Apply an updated LedConfig — reinitialises strip if ledCount or pin changed.
    void applyConfig(const LedConfig& cfg);

    // Set the device identity colour used by the idle animation.
    void setDeviceColor(uint8_t r, uint8_t g, uint8_t b);

    bool isAvailable() const { return _available; }

private:
    LedConfig  _cfg       = ledConfigDefaults();
    LedStrip*  _strip     = nullptr;    // instance-owned; freed in destructor / begin()
    bool       _available = false;
    uint8_t    _rows      = 8;
    uint8_t    _cols      = 8;
    uint16_t   _pixelCount = 0;

    // Simple pixel buffer (max 256 pixels; enough for 16×16)
    static const uint16_t MAX_PIXELS = 256;

    // Identity colour for ambient effects
    uint8_t _devR = 0, _devG = 0, _devB = 0;

    // Scrolling text state
    char          _scrollText[64]   = {};
    uint8_t       _scrollR          = 255;
    uint8_t       _scrollG          = 255;
    uint8_t       _scrollB          = 255;
    uint16_t      _scrollSpeed      = 80;
    bool          _scrolling        = false;
    int16_t       _scrollOffset     = 0;     // current column offset into text bitmap
    unsigned long _scrollLastStepMs = 0;
    int16_t       _scrollTotalCols  = 0;     // total column width of text

    enum class Mode { IDLE, STATIC, NUMBER, SCROLLING, WINNER, BLINK, PULSE, CHASE, SPARKLE, BALLROLL, CAMELCHEW, CAMELSPEEDUP, SCOREHIGH, TOLEAD, FARBEHIND, GAMESTART, GAMEEND, SOMEONEWON };
    Mode _mode = Mode::IDLE;

    // Animated effect state (BLINK / PULSE / CHASE / SPARKLE)
    uint8_t       _animR           = 0;
    uint8_t       _animG           = 0;
    uint8_t       _animB           = 255;
    uint16_t      _animSpeedMs     = 500;
    unsigned long _animLastStepMs  = 0;
    bool          _animOn          = true;    // BLINK on/off toggle
    uint16_t      _animPhase       = 0;       // PULSE triangle 0..511
    uint16_t      _animChasePos    = 0;       // CHASE pixel index

    void _freeStrip();
    void _setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
    void _show();
    void _stepScroll();
    void _stepBlink();
    void _stepPulse();
    void _stepChase();
    void _stepSparkle();
    void _stepBallRoll();
    void _stepCamelChew();
    void _stepCamelSpeedup();
    void _stepScoreHigh();
    void _stepToLead();
    void _stepFarBehind();
    void _stepGameStart();
    void _stepGameEnd();
    void _stepSomeoneWon();

    // ── Ball-roll state ──────────────────────────────────────────────────
    enum class BallPhase : uint8_t { ROLLING, DROPPING, CELEBRATING, PAUSE };

    uint8_t       _brR           = 0;      // ball colour red
    uint8_t       _brG           = 200;    // ball colour green
    uint8_t       _brB           = 0;      // ball colour blue
    uint16_t      _brSpeedMs     = 2000;   // roll phase duration ms

    // Fixed-point ball position (x256 to avoid float)
    int32_t       _brBallRowFP   = 0;  // current row  << 8
    int32_t       _brBallColFP   = 0;  // current col  << 8
    int32_t       _brStartRowFP  = 0;
    int32_t       _brStartColFP  = 0;
    int32_t       _brTargRowFP   = 0;
    int32_t       _brTargColFP   = 0;

    // Three V-shaped holes: computed in showBallRoll
    struct BrHole { uint8_t row; uint8_t col; };
    BrHole        _brHoles[3]    = {};
    uint8_t       _brTarget      = 0;   // chosen hole index 0-2

    BallPhase     _brPhase       = BallPhase::ROLLING;
    unsigned long _brPhaseStartMs = 0;

    // ── Camel-chew state ─────────────────────────────────────────────────
    uint8_t       _ccHayR        = 120;   // hay/grass colour
    uint8_t       _ccHayG        = 197;
    uint8_t       _ccHayB        = 32;
    uint16_t      _ccChewCycleMs = 2400;  // full chew cycle duration

    unsigned long _ccChewStartMs = 0;     // when this chew cycle started
    unsigned long _ccBlinkTimer  = 0;     // next blink time
    unsigned long _ccEarTimer    = 0;     // next ear-flick time
    uint8_t       _ccEarSide     = 0;     // 0=left  1=right  2=both

    // ── Camel-speedup state ──────────────────────────────────────────
    uint8_t       _csR           = 255;
    uint8_t       _csG           = 107;
    uint8_t       _csB           = 0;
    uint16_t      _csSpeedMs     = 1500;
    unsigned long _csPhaseStartMs = 0;
    uint8_t       _csPhase       = 0;   // 0=fart  1=burst

    // ── Score-high state ────────────────────────────────────────────
    uint8_t       _shR           = 255;
    uint8_t       _shG           = 215;
    uint8_t       _shB           = 0;
    uint16_t      _shSpeedMs     = 1200;
    unsigned long _shPhaseStartMs = 0;

    // ── To-lead state ───────────────────────────────────────────────
    uint8_t       _tlR           = 255;
    uint8_t       _tlG           = 215;
    uint8_t       _tlB           = 0;
    uint16_t      _tlSpeedMs     = 1000;
    unsigned long _tlPhaseStartMs = 0;

    // ── Far-behind state ────────────────────────────────────────────
    uint8_t       _fbR           = 68;
    uint8_t       _fbG           = 136;
    uint8_t       _fbB           = 255;
    uint16_t      _fbSpeedMs     = 1800;
    unsigned long _fbPhaseStartMs = 0;

    // ── Game-start state ────────────────────────────────────────────
    unsigned long _gsPhaseStartMs = 0;
    uint8_t       _gsPhase       = 0;   // 0=checkerboard  1=ready  2=set  3=go

    // ── Game-end state ──────────────────────────────────────────────
    unsigned long _gePhaseStartMs = 0;

    // ── Someone-won state ───────────────────────────────────────────
    uint8_t       _swR           = 255;
    uint8_t       _swG           = 215;
    uint8_t       _swB           = 0;
    unsigned long _swPhaseStartMs = 0;
};
