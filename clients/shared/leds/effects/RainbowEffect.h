/**
 * Rainbow Effect
 * Displays full HSV spectrum across LED strip
 * Creates a traveling rainbow pattern with configurable cycle speed
 */

#ifndef RAINBOW_EFFECT_H
#define RAINBOW_EFFECT_H

#include "../LedEffect.h"
#include <Arduino.h>

/**
 * Rainbow effect displaying full color spectrum
 * 
 * Each LED displays a different hue from the HSV color wheel.
 * The entire rainbow pattern rotates over time, creating a traveling effect.
 * 
 * Extended parameters (use setCycleSpeed after construction):
 * - cycleSpeedMs: Time for rainbow to complete one full rotation (milliseconds)
 * 
 * Standard parameters:
 * - brightness: Overall brightness (0-255)
 * - direction: FORWARD or REVERSE for rotation direction
 * - durationMs: Total effect duration (0 = infinite)
 * 
 * Usage example:
 * ```
 * RainbowEffect rainbow(&controller);
 * EffectParams params;
 * params.brightness = 150;
 * params.direction = DIRECTION_FORWARD;
 * rainbow.setParams(params);
 * rainbow.setCycleSpeed(3000); // 3-second rainbow cycle
 * rainbow.begin();
 * ```
 */
class RainbowEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  RainbowEffect(LedController* controller) 
    : LedEffect(controller)
    , _cycleSpeedMs(3000)
    , _hueOffset(0) {
    _type = EFFECT_RAINBOW;
  }
  
  /**
   * Set rainbow cycle speed
   * @param cycleSpeedMs Time for one complete rainbow rotation (milliseconds)
   */
  void setCycleSpeed(uint16_t cycleSpeedMs) {
    if (cycleSpeedMs < 100) {
      cycleSpeedMs = 100; // Minimum 100ms cycle
      DERBY_LOG_LN("[RainbowEffect] Cycle speed clamped to 100ms minimum");
    }
    _cycleSpeedMs = cycleSpeedMs;
  }
  
  /**
   * Initialize effect
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    _hueOffset = 0;
    
    DERBY_LOG_F("[RainbowEffect] Started with cycle speed %ums, direction %s\n",
                _cycleSpeedMs, _params.direction == DIRECTION_FORWARD ? "FORWARD" : "REVERSE");
  }
  
  /**
   * Update effect - rotate rainbow colors
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    
    // Calculate hue offset based on elapsed time
    // Adjust cycle speed by speed parameter
    uint32_t adjustedCycleMs = (uint32_t)(_cycleSpeedMs / _params.speed);
    
    // Calculate hue offset (0-360 degrees)
    _hueOffset = (_elapsedTime * 360UL) / adjustedCycleMs;
    _hueOffset = _hueOffset % 360;
    
    // Reverse direction if configured
    if (_params.direction == DIRECTION_REVERSE) {
      _hueOffset = 360 - _hueOffset;
    }
    
    // Update all LEDs with rainbow pattern
    updateLeds();
  }
  
  /**
   * Check if effect completed
   * @return true if duration exceeded, false if infinite
   */
  bool isComplete() const override {
    return isDurationExceeded();
  }
  
  /**
   * Reset effect state
   */
  void reset() override {
    LedEffect::reset();
    _hueOffset = 0;
  }
  
  /**
   * Get effect name
   * @return "Rainbow"
   */
  const char* getName() const override {
    return "Rainbow";
  }

private:
  uint16_t _cycleSpeedMs;      // Rainbow cycle duration
  uint16_t _hueOffset;         // Current hue offset (0-360 degrees)
  
  /**
   * Update all LEDs with rainbow pattern
   */
  void updateLeds() {
    uint16_t ledCount = _controller->getLedCount();
    
    // Calculate hue spacing between LEDs
    uint16_t hueStep = 360 / ledCount;
    
    // Set each LED to a different hue
    for (uint16_t i = 0; i < ledCount; i++) {
      // Calculate hue for this LED (0-360)
      uint16_t hue = (_hueOffset + (i * hueStep)) % 360;
      
      // Convert HSV to RGB
      // Using simplified HSV->RGB conversion optimized for ESP8266
      RgbColor color = hsvToRgb(hue, 255, _params.brightness);
      
      _controller->setPixel(i, color);
    }
    
    _controller->show();
  }
  
  /**
   * Convert HSV to RGB
   * @param hue Hue (0-360 degrees)
   * @param saturation Saturation (0-255)
   * @param value Brightness/Value (0-255)
   * @return RGB color
   * 
   * Optimized HSV to RGB conversion for embedded systems
   */
  RgbColor hsvToRgb(uint16_t hue, uint8_t saturation, uint8_t value) {
    uint8_t region, remainder, p, q, t;
    
    if (saturation == 0) {
      // Grayscale
      return RgbColor(value, value, value);
    }
    
    region = hue / 60;
    remainder = (hue % 60) * 255 / 60;
    
    p = (value * (255 - saturation)) / 255;
    q = (value * (255 - ((saturation * remainder) / 255))) / 255;
    t = (value * (255 - ((saturation * (255 - remainder)) / 255))) / 255;
    
    switch (region) {
      case 0:
        return RgbColor(value, t, p);
      case 1:
        return RgbColor(q, value, p);
      case 2:
        return RgbColor(p, value, t);
      case 3:
        return RgbColor(p, q, value);
      case 4:
        return RgbColor(t, p, value);
      default:
        return RgbColor(value, p, q);
    }
  }
};

#endif // RAINBOW_EFFECT_H
