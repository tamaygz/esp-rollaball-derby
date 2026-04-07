#include "LedController.h"

// WS2812B timing: requires 50µs reset time between frames
static constexpr unsigned long WS2812_RESET_TIME_US = 50;

// WiFi yield interval: call yield() every 50ms to keep WiFi stack responsive
static constexpr unsigned long WIFI_YIELD_INTERVAL_MS = 50;

LedController::LedController()
    : _strip(nullptr)
    , _ledCount(0)
    , _pin(0)
    , _brightness(255)
    , _lastShow(0)
    , _lastYield(0)
{
}

LedController::~LedController() {
    if (_strip) {
        delete _strip;
        _strip = nullptr;
    }
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

    // Create platform-specific NeoPixelBus instance
    _strip = new LedStrip(_ledCount, _pin);
    if (!_strip) {
        Serial.printf("[LedController] ERROR: Failed to allocate NeoPixelBus (%u LEDs)\n", _ledCount);
        return false;
    }

    // Initialize hardware
    _strip->Begin();
    _strip->Show();  // Initialize to all LEDs off

    _logInit();
    return true;
}

void LedController::setPixel(uint16_t index, RgbColor color) {
    if (index >= _ledCount) {
        Serial.printf("[LedController] ERROR: Pixel index %u out of bounds (count=%u)\n", 
                      index, _ledCount);
        return;
    }

    if (!_strip) {
        Serial.println("[LedController] ERROR: setPixel() called before begin()");
        return;
    }

    // Apply brightness scaling
    if (_brightness < 255) {
        color = RgbColor::LinearBlend(RgbColor(0, 0, 0), color, _brightness);
    }

    _strip->SetPixelColor(index, color);
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
    if (!_strip) {
        Serial.println("[LedController] ERROR: clear() called before begin()");
        return;
    }

    for (uint16_t i = 0; i < _ledCount; i++) {
        _strip->SetPixelColor(i, RgbColor(0, 0, 0));
    }
}

void LedController::show() {
    if (!_strip) {
        Serial.println("[LedController] ERROR: show() called before begin()");
        return;
    }

    // Check if we can safely update (WS2812B reset time)
    if (!canShow()) {
        // Minor delay to reach minimum reset time
        delayMicroseconds(WS2812_RESET_TIME_US);
    }

    _strip->Show();
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
    // ESP8266 DMA method requires GPIO3 (RX pin)
    // If using UART method in future, this validation can be relaxed
    if (pin != 3) {
        Serial.printf("[LedController] WARNING: ESP8266 DMA requires GPIO3, got GPIO%u. "
                      "Consider using UART method for other pins.\n", pin);
        // Allow it but warn — user might be using UART method
    }

    // Reject pins that conflict with boot mode
    if (pin == 0 || pin == 2 || pin == 15) {
        Serial.printf("[LedController] ERROR: GPIO%u conflicts with boot mode/flash\n", pin);
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
    Serial.printf("[LedController] LED Count: %u (max %u)\n", _ledCount, LED_MAX_COUNT);
    Serial.printf("[LedController] GPIO Pin:  %u\n", _pin);
    Serial.printf("[LedController] RAM Usage: ~%u bytes\n", ramUsage);
    Serial.printf("[LedController] Power Est: %.1f A @ 100%% brightness\n", 
                  (_ledCount * 60.0) / 1000.0);
    Serial.println("[LedController] ====================================");
}
