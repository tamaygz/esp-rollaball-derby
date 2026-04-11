#include "LedController.h"

// WS2812B timing: requires 50µs reset time between frames
static constexpr unsigned long WS2812_RESET_TIME_US = 50;

// WiFi yield interval: call yield() every 50ms to keep WiFi stack responsive
static constexpr unsigned long WIFI_YIELD_INTERVAL_MS = 50;

LedController::LedController()
    :
#ifdef LED_PLATFORM_ESP8266
      _stripUart1(nullptr)
    , _stripDma(nullptr)
    , _method(Esp8266Method::METHOD_UART1)
#else
      _strip(nullptr)
#endif
    , _ledCount(0)
    , _pin(0)
    , _brightness(255)
    , _lastShow(0)
    , _lastYield(0)
{
}

LedController::~LedController() {
#ifdef LED_PLATFORM_ESP8266
    _stripDelete();
#else
    if (_strip) {
        delete _strip;
        _strip = nullptr;
    }
#endif
}

bool LedController::begin(uint16_t ledCount, uint8_t pin) {
    // Validate parameters
    if (!_validateLedCount(ledCount)) {
        Serial.printf("[LedController] ERROR: Invalid LED count %u (max %u for %s)\n",
                      ledCount, LED_MAX_COUNT, LED_PLATFORM_NAME);
        return false;
    }

    if (!_validatePin(pin)) {
        Serial.printf("[LedController] ERROR: Invalid GPIO pin %u\n", pin);
        return false;
    }

    _ledCount = ledCount;
    _pin      = pin;

    // Free any previous strip before allocating a new one
#ifdef LED_PLATFORM_ESP8266
    _stripDelete();

    // Select method based on pin: GPIO3 → DMA, anything else → UART1 (fixed to GPIO2)
    if (pin == 3) {
        _method   = Esp8266Method::METHOD_DMA;
        _stripDma = new LedStripDma(_ledCount, _pin);
        if (!_stripDma) {
            Serial.printf("[LedController] ERROR: Failed to allocate DMA strip (%u LEDs)\n", _ledCount);
            return false;
        }
    } else {
        _method     = Esp8266Method::METHOD_UART1;
        _pin        = 2;  // UART1 is hardwired to GPIO2
        _stripUart1 = new LedStripUart1(_ledCount, _pin);
        if (!_stripUart1) {
            Serial.printf("[LedController] ERROR: Failed to allocate UART1 strip (%u LEDs)\n", _ledCount);
            return false;
        }
    }
#else
    if (_strip) {
        delete _strip;
        _strip = nullptr;
    }

    // Create platform-specific NeoPixelBus instance
    _strip = new LedStrip(_ledCount, _pin);
    if (!_strip) {
        Serial.printf("[LedController] ERROR: Failed to allocate NeoPixelBus (%u LEDs)\n", _ledCount);
        return false;
    }
#endif

    // Initialize hardware
    _stripBegin();
    _stripShow();  // Initialize to all LEDs off

    _logInit();
    return true;
}

void LedController::setPixel(uint16_t index, RgbColor color) {
    if (index >= _ledCount) {
        Serial.printf("[LedController] ERROR: Pixel index %u out of bounds (count=%u)\n", 
                      index, _ledCount);
        return;
    }

#ifdef LED_PLATFORM_ESP8266
    if (!_stripUart1 && !_stripDma) {
#else
    if (!_strip) {
#endif
        Serial.println("[LedController] ERROR: setPixel() called before begin()");
        return;
    }

    // Apply brightness scaling
    if (_brightness < 255) {
        color = RgbColor::LinearBlend(RgbColor(0, 0, 0), color, _brightness);
    }

    _stripSetPixel(index, color);
}

void LedController::setPixel(uint16_t index, HsvColor color) {
    // Convert HSV to RGB and delegate
    RgbColor rgb = RgbColor(color);
    setPixel(index, rgb);
}

void LedController::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

uint8_t LedController::getBrightness() const {
    return _brightness;
}

void LedController::clear() {
#ifdef LED_PLATFORM_ESP8266
    if (!_stripUart1 && !_stripDma) {
#else
    if (!_strip) {
#endif
        Serial.println("[LedController] ERROR: clear() called before begin()");
        return;
    }

    for (uint16_t i = 0; i < _ledCount; i++) {
        _stripSetPixel(i, RgbColor(0, 0, 0));
    }
}

void LedController::show() {
#ifdef LED_PLATFORM_ESP8266
    if (!_stripUart1 && !_stripDma) {
#else
    if (!_strip) {
#endif
        Serial.println("[LedController] ERROR: show() called before begin()");
        return;
    }

    // Check if we can safely update (WS2812B reset time)
    if (!canShow()) {
        // Minor delay to reach minimum reset time
        delayMicroseconds(WS2812_RESET_TIME_US);
    }

    _stripShow();
    _lastShow = micros();

    // Yield to WiFi stack if it's been too long
    unsigned long now = millis();
    if (now - _lastYield >= WIFI_YIELD_INTERVAL_MS) {
        yield();
        _lastYield = now;
    }
}

bool LedController::canShow() const {
    return (micros() - _lastShow) >= WS2812_RESET_TIME_US;
}

void LedController::loop() {
    unsigned long now = millis();
    if (now - _lastYield >= WIFI_YIELD_INTERVAL_MS) {
        yield();
        _lastYield = now;
    }
}

uint16_t LedController::getLedCount() const {
    return _ledCount;
}

bool LedController::_validateLedCount(uint16_t count) const {
    return (count >= 1 && count <= LED_MAX_COUNT);
}

bool LedController::_validatePin(uint8_t pin) const {
#ifdef LED_PLATFORM_ESP8266
    // Only GPIO2 (UART1) and GPIO3 (DMA) have hardware-timed output.
    if (pin != 2 && pin != 3) {
        Serial.printf("[LedController] ERROR: ESP8266 only supports GPIO2 (UART1) or GPIO3 (DMA), got GPIO%u\n", pin);
        return false;
    }
#endif

#ifdef LED_PLATFORM_ESP32
    // ESP32 RMT can use any valid GPIO
    // Avoid input-only pins (34-39 on some ESP32 variants)
    if (pin >= 34 && pin <= 39) {
        Serial.printf("[LedController] WARNING: GPIO%u is input-only on some ESP32 variants\n", pin);
    }
#endif

    return true;
}

void LedController::_logInit() const {
    size_t ramUsage = _ledCount * 3;  // 3 bytes per LED (RGB)
    
    Serial.println("[LedController] ====================================");
    Serial.printf("[LedController] Platform:  %s\n", LED_PLATFORM_NAME);
#ifdef LED_PLATFORM_ESP8266
    Serial.printf("[LedController] Method:    %s\n",
                  _method == Esp8266Method::METHOD_DMA ? "DMA (GPIO3/RX)" : "UART1 (GPIO2/D4)");
#endif
    Serial.printf("[LedController] LED Count: %u (max %u)\n", _ledCount, LED_MAX_COUNT);
    Serial.printf("[LedController] GPIO Pin:  %u\n", _pin);
    Serial.printf("[LedController] RAM Usage: ~%u bytes\n", ramUsage);
    Serial.printf("[LedController] Power Est: %.1f A @ 100%% brightness\n", 
                  (_ledCount * 60.0) / 1000.0);
    Serial.println("[LedController] ====================================");
}

// ─── ESP8266 strip dispatch helpers ───────────────────────────────────────────
#ifdef LED_PLATFORM_ESP8266

void LedController::_stripBegin() {
    if (_method == Esp8266Method::METHOD_DMA)
        _stripDma->Begin();
    else
        _stripUart1->Begin();
}

void LedController::_stripShow() {
    if (_method == Esp8266Method::METHOD_DMA)
        _stripDma->Show();
    else
        _stripUart1->Show();
}

void LedController::_stripSetPixel(uint16_t i, RgbColor c) {
    if (_method == Esp8266Method::METHOD_DMA)
        _stripDma->SetPixelColor(i, c);
    else
        _stripUart1->SetPixelColor(i, c);
}

void LedController::_stripDelete() {
    if (_stripDma)   { delete _stripDma;   _stripDma   = nullptr; }
    if (_stripUart1) { delete _stripUart1; _stripUart1 = nullptr; }
}

#else
// ─── Non-ESP8266: thin wrappers around the single _strip pointer ──────────────

void LedController::_stripBegin()                      { _strip->Begin(); }
void LedController::_stripShow()                       { _strip->Show(); }
void LedController::_stripSetPixel(uint16_t i, RgbColor c) { _strip->SetPixelColor(i, c); }
void LedController::_stripDelete()                     { if (_strip) { delete _strip; _strip = nullptr; } }

#endif
