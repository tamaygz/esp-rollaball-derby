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

    enum class Mode { IDLE, NUMBER, SCROLLING, WINNER };
    Mode _mode = Mode::IDLE;

    void _freeStrip();
    void _setPixel(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b);
    void _show();
    void _stepScroll();
};
