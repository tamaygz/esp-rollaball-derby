/**
 * Solid Color Effect
 * Displays a single static color across all LEDs
 * Simplest effect, useful as a baseline and for testing
 */

#ifndef SOLID_EFFECT_H
#define SOLID_EFFECT_H

#include "../LedEffect.h"

/**
 * Solid color effect
 * 
 * Sets all LEDs to a single color and maintains it.
 * No animation - purely static display.
 * 
 * Parameters:
 * - color: RGB color to display
 * - brightness: Overall brightness (0-255)
 * - durationMs: How long to show color (0 = infinite)
 * 
 * Usage example:
 * ```
 * SolidEffect solid(&controller);
 * EffectParams params;
 * params.color = RgbColor(255, 0, 0); // Red
 * params.brightness = 200;
 * solid.setParams(params);
 * solid.begin();
 * ```
 */
class SolidEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  SolidEffect(LedController* controller) 
    : LedEffect(controller) {
    _type = EFFECT_SOLID;
  }
  
  /**
   * Initialize effect - set all LEDs to configured color
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    
    // Get LED count
    uint16_t ledCount = _controller->getLedCount();
    
    // Apply brightness scaling to color
    uint8_t r = (_params.color.R * _params.brightness) / 255;
    uint8_t g = (_params.color.G * _params.brightness) / 255;
    uint8_t b = (_params.color.B * _params.brightness) / 255;
    RgbColor scaledColor(r, g, b);
    
    // Set all LEDs to scaled color
    for (uint16_t i = 0; i < ledCount; i++) {
      _controller->setPixel(i, scaledColor);
    }
    
    // Display
    _controller->show();
    
    Serial.print("[SolidEffect] Started with color RGB(");
    Serial.print(scaledColor.R);
    Serial.print(",");
    Serial.print(scaledColor.G);
    Serial.print(",");
    Serial.print(scaledColor.B);
    Serial.println(")");
  }
  
  /**
   * Update effect - no-op for solid color
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    // Solid color is static, no updates needed
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
   * @return "Solid"
   */
  const char* getName() const override {
    return "Solid";
  }
};

#endif // SOLID_EFFECT_H
