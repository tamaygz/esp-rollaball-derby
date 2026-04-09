#pragma once

#include "LedPlatform.h"
#include <Arduino.h>

/**
 * LedController — Unified LED control for WS2812B addressable LEDs on ESP8266/ESP32.
 * 
 * Features:
 * - Platform abstraction: ESP8266 (DMA / UART1) and ESP32 (RMT) hardware acceleration
 * - HSV and RGB color support with automatic conversion
 * - Global brightness control
 * - Memory-safe with bounds checking
 * - WiFi-friendly non-blocking operation
 * 
 * Hardware requirements:
 * - WS2812B LEDs with external 5V power supply
 * - Data pin connected to ESP GPIO (ESP8266: GPIO2 for UART1 or GPIO3 for DMA, ESP32: any GPIO)
 * - Maximum LEDs: 300 (ESP8266), 1000 (ESP32)
 * - Power budget: 60mA per LED at full brightness
 * 
 * Example usage:
 * ```cpp
 * LedController leds;
 * leds.begin(50, 4);  // 50 LEDs on GPIO4
 * leds.setBrightness(128);  // 50% brightness
 * leds.setPixel(0, HsvColor(0, 255, 255));  // Red
 * leds.show();
 * ```
 */
class LedController {
public:
    LedController();
    ~LedController();

    /**
     * Initialize the LED strip.
     * 
     * @param ledCount Number of LEDs (1-300 for ESP8266, 1-1000 for ESP32)
     * @param pin GPIO pin for data output.
     *            ESP8266: GPIO2 → UART1 method, GPIO3 → DMA method.
     *            ESP32: any valid GPIO.
     * @return true if successful, false if validation failed
     */
    bool begin(uint16_t ledCount, uint8_t pin);

    /**
     * Set a single pixel to an RGB color.
     * 
     * @param index Pixel index (0-based, must be < ledCount)
     * @param color RGB color (0-255 per channel)
     */
    void setPixel(uint16_t index, RgbColor color);

    /**
     * Set a single pixel to an HSV color (auto-converts to RGB).
     * 
     * @param index Pixel index (0-based, must be < ledCount)
     * @param color HSV color (H: 0-360, S: 0-255, V: 0-255)
     */
    void setPixel(uint16_t index, HsvColor color);

    /**
     * Set global brightness multiplier.
     * 
     * @param brightness Brightness level (0-255, where 255 = 100%)
     */
    void setBrightness(uint8_t brightness);

    /**
     * Get current brightness level.
     * 
     * @return Brightness (0-255)
     */
    uint8_t getBrightness() const;

    /**
     * Turn off all LEDs (set to black).
     */
    void clear();

    /**
     * Push the LED buffer to hardware (actually update the physical LEDs).
     * Call this after setPixel() operations to make changes visible.
     */
    void show();

    /**
     * Check if enough time has passed since last show() to safely call show() again.
     * WS2812B requires 50µs reset time between frames.
     * 
     * @return true if show() can be called now, false if still in reset period
     */
    bool canShow() const;

    /**
     * Non-blocking loop() method for WiFi yield points.
     * Call this every iteration to maintain WiFi stability during LED updates.
     */
    void loop();

    /**
     * Get the number of LEDs configured.
     * 
     * @return LED count
     */
    uint16_t getLedCount() const;

private:
#ifdef LED_PLATFORM_ESP8266
    // ESP8266: two hardware-timed methods, selected at runtime by pin.
    enum class Esp8266Method : uint8_t { METHOD_UART1, METHOD_DMA };
    LedStripUart1* _stripUart1 = nullptr;
    LedStripDma*   _stripDma   = nullptr;
    Esp8266Method  _method     = Esp8266Method::METHOD_UART1;
#else
    LedStrip*     _strip;
#endif

    // Dispatch helpers — route calls to the active strip instance.
    void _stripBegin();
    void _stripShow();
    void _stripSetPixel(uint16_t i, RgbColor c);
    void _stripDelete();
    uint16_t      _ledCount;
    uint8_t       _pin;
    uint8_t       _brightness;
    unsigned long _lastShow;
    unsigned long _lastYield;

    bool _validateLedCount(uint16_t count) const;
    bool _validatePin(uint8_t pin) const;
    void _logInit() const;
};
