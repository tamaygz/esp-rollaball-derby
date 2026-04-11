/**
 * Pulse Effect (Breathing)
 * Smooth sinusoidal brightness variation
 * Creates a breathing/pulsing effect with configurable period
 */

#ifndef PULSE_EFFECT_H
#define PULSE_EFFECT_H

#include "../LedEffect.h"
#include <Arduino.h>

/**
 * Pulse effect with smooth brightness variation
 * 
 * Uses pre-computed sine wave lookup table for smooth breathing animation.
 * Brightness varies sinusoidally from 0 to configured brightness and back.
 * 
 * Extended parameters (use setPeriod after construction):
 * - periodMs: Duration of one complete pulse cycle (milliseconds)
 * 
 * Standard parameters:
 * - color: RGB base color
 * - brightness: Maximum brightness (0-255)
 * - durationMs: Total effect duration (0 = infinite)
 * 
 * Usage example:
 * ```
 * PulseEffect pulse(&controller);
 * EffectParams params;
 * params.color = RgbColor(0, 255, 0); // Green
 * params.brightness = 200;
 * pulse.setParams(params);
 * pulse.setPeriod(2000); // 2-second pulse cycle
 * pulse.begin();
 * ```
 */
class PulseEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  PulseEffect(LedController* controller) 
    : LedEffect(controller)
    , _periodMs(1000) {
    _type = EFFECT_PULSE;
    initSineTable();
  }
  
  /**
   * Set pulse period
   * @param periodMs Duration of one complete pulse cycle (milliseconds)
   */
  void setPeriod(uint16_t periodMs) {
    if (periodMs < 100) {
      periodMs = 100; // Minimum 100ms period
      Serial.println("[PulseEffect] Period clamped to 100ms minimum");
    }
    _periodMs = periodMs;
  }
  
  /**
   * Initialize effect
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    
    Serial.print("[PulseEffect] Started with period ");
    Serial.print(_periodMs);
    Serial.println("ms");
  }
  
  /**
   * Update effect - calculate and apply brightness
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    
    // Calculate position in sine wave (0-255)
    // Map elapsed time to sine table index
    uint32_t adjustedPeriod = (uint32_t)(_periodMs / _params.speed);
    uint32_t timeInCycle = _elapsedTime % adjustedPeriod;
    uint8_t sineIndex = (uint8_t)((timeInCycle * 256UL) / adjustedPeriod);
    
    // Get brightness from sine table (0-255)
    uint8_t sineBrightness = _sineTable[sineIndex];
    
    // Scale by configured brightness
    uint8_t effectiveBrightness = (sineBrightness * _params.brightness) / 255;
    
    // Apply to all LEDs
    updateLeds(effectiveBrightness);
  }
  
  /**
   * Check if effect completed
   * @return true if duration exceeded, false if infinite
   */
  bool isComplete() const override {
    return isDurationExceeded();
  }
  
  /**
   * Get effect name
   * @return "Pulse"
   */
  const char* getName() const override {
    return "Pulse";
  }

private:
  uint16_t _periodMs;          // Pulse cycle duration
  uint8_t _sineTable[256];     // Pre-computed sine wave lookup table
  
  /**
   * Initialize sine wave lookup table
   * Pre-computes 256 brightness values for smooth sine wave
   */
  void initSineTable() {
    for (uint16_t i = 0; i < 256; i++) {
      // Calculate sine wave: sin(2π * i/256)
      // Map from [-1, 1] to [0, 255]
      float angle = (float)i * 2.0f * PI / 256.0f;
      float sineValue = sin(angle);
      // Map -1..1 to 0..255 with smooth curve
      _sineTable[i] = (uint8_t)((sineValue + 1.0f) * 127.5f);
    }
  }
  
  /**
   * Update all LEDs with current brightness
   * @param brightness Current effective brightness (0-255)
   */
  void updateLeds(uint8_t brightness) {
    uint16_t ledCount = _controller->getLedCount();
    
    // Scale color by brightness
    uint8_t r = (_params.color.R * brightness) / 255;
    uint8_t g = (_params.color.G * brightness) / 255;
    uint8_t b = (_params.color.B * brightness) / 255;
    RgbColor scaledColor(r, g, b);
    
    for (uint16_t i = 0; i < ledCount; i++) {
      _controller->setPixel(i, scaledColor);
    }
    
    _controller->show();
  }
};

#endif // PULSE_EFFECT_H
