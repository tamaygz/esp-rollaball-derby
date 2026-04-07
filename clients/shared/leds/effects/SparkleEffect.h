/**
 * Sparkle Effect
 * Random LED twinkles (starfield style)
 * Creates a magical twinkling effect with configurable density and fade speed
 */

#ifndef SPARKLE_EFFECT_H
#define SPARKLE_EFFECT_H

#include "../LedEffect.h"
#include <Arduino.h>

/**
 * Sparkle effect with random twinkling LEDs
 * 
 * Random LEDs light up briefly then fade out, creating a starfield effect.
 * Configurable base color, sparkle color, density, and fade speed.
 * 
 * Extended parameters (use setSparkleParams after construction):
 * - baseColor: Background color when not sparkling (RGB)
 * - sparkleColor: Color when sparkling (RGB)
 * - density: Probability of LED sparkling per frame (0.0-1.0)
 * - fadeSpeed: Brightness reduction per frame (1-50)
 * 
 * Standard parameters:
 * - brightness: Maximum brightness (0-255)
 * - durationMs: Total effect duration (0 = infinite)
 * 
 * Usage example:
 * ```
 * SparkleEffect sparkle(&controller);
 * EffectParams params;
 * params.brightness = 200;
 * sparkle.setParams(params);
 * sparkle.setSparkleParams(
 *   RgbColor(0, 0, 50),    // Dark blue background
 *   RgbColor(255, 255, 255), // White sparkles
 *   0.1,                     // 10% sparkle density
 *   10                       // Fade speed
 * );
 * sparkle.begin();
 * ```
 */
class SparkleEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  SparkleEffect(LedController* controller) 
    : LedEffect(controller)
    , _baseColor(RgbColor(0, 0, 0))
    , _sparkleColor(RgbColor(255, 255, 255))
    , _density(0.05f)
    , _fadeSpeed(5)
    , _pixelBrightness(nullptr) {
    _type = EFFECT_SPARKLE;
  }
  
  /**
   * Destructor - clean up allocated memory
   */
  ~SparkleEffect() {
    if (_pixelBrightness != nullptr) {
      delete[] _pixelBrightness;
      _pixelBrightness = nullptr;
    }
  }
  
  /**
   * Set sparkle-specific parameters
   * @param baseColor Background color (RGB)
   * @param sparkleColor Sparkle color (RGB)
   * @param density Sparkle probability per frame (0.0-1.0)
   * @param fadeSpeed Brightness reduction per frame (1-50)
   */
  void setSparkleParams(RgbColor baseColor, RgbColor sparkleColor, float density, uint8_t fadeSpeed) {
    _baseColor = baseColor;
    _sparkleColor = sparkleColor;
    
    // Clamp density
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;
    _density = density;
    
    // Clamp fade speed
    if (fadeSpeed < 1) fadeSpeed = 1;
    if (fadeSpeed > 50) fadeSpeed = 50;
    _fadeSpeed = fadeSpeed;
  }
  
  /**
   * Initialize effect
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    
    // Allocate brightness tracking array
    uint16_t ledCount = _controller->getLedCount();
    if (_pixelBrightness != nullptr) {
      delete[] _pixelBrightness;
    }
    _pixelBrightness = new uint8_t[ledCount];
    
    // Initialize all LEDs to base color
    for (uint16_t i = 0; i < ledCount; i++) {
      _pixelBrightness[i] = 0;
      _controller->setPixel(i, _baseColor);
    }
    _controller->show();
    
    // Seed random number generator with microseconds
    randomSeed(micros());
    
    Serial.print("[SparkleEffect] Started: density=");
    Serial.print(_density);
    Serial.print(", fadeSpeed=");
    Serial.println(_fadeSpeed);
  }
  
  /**
   * Update effect - sparkle and fade LEDs
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    
    uint16_t ledCount = _controller->getLedCount();
    
    // Update each LED
    for (uint16_t i = 0; i < ledCount; i++) {
      // Check if this LED should sparkle (random probability)
      if (_pixelBrightness[i] == 0) {
        // LED is dark, check if it should sparkle
        float randVal = (float)random(1000) / 1000.0f;
        if (randVal < _density) {
          // Sparkle! Set to full brightness
          _pixelBrightness[i] = 255;
        }
      } else {
        // LED is sparkling, fade it out
        if (_pixelBrightness[i] > _fadeSpeed) {
          _pixelBrightness[i] -= _fadeSpeed;
        } else {
          _pixelBrightness[i] = 0;
        }
      }
      
      // Calculate pixel color based on brightness
      RgbColor pixelColor;
      if (_pixelBrightness[i] == 0) {
        // Base color
        pixelColor = scaleColor(_baseColor, _params.brightness);
      } else {
        // Blend between base and sparkle color based on brightness
        pixelColor = blendColors(_baseColor, _sparkleColor, _pixelBrightness[i]);
        pixelColor = scaleColor(pixelColor, _params.brightness);
      }
      
      _controller->setPixel(i, pixelColor);
    }
    
    _controller->show();
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
    
    if (_pixelBrightness != nullptr) {
      uint16_t ledCount = _controller->getLedCount();
      for (uint16_t i = 0; i < ledCount; i++) {
        _pixelBrightness[i] = 0;
      }
    }
  }
  
  /**
   * Get effect name
   * @return "Sparkle"
   */
  const char* getName() const override {
    return "Sparkle";
  }

private:
  RgbColor _baseColor;         // Background color
  RgbColor _sparkleColor;      // Sparkle color
  float _density;              // Sparkle probability (0.0-1.0)
  uint8_t _fadeSpeed;          // Fade speed (brightness units per frame)
  uint8_t* _pixelBrightness;   // Per-pixel brightness tracking
  
  /**
   * Scale color by brightness
   * @param color Input color
   * @param brightness Brightness scale (0-255)
   * @return Scaled color
   */
  RgbColor scaleColor(RgbColor color, uint8_t brightness) {
    return RgbColor(
      (color.R * brightness) / 255,
      (color.G * brightness) / 255,
      (color.B * brightness) / 255
    );
  }
  
  /**
   * Blend two colors
   * @param color1 First color
   * @param color2 Second color
   * @param blend Blend ratio (0-255, 0=color1, 255=color2)
   * @return Blended color
   */
  RgbColor blendColors(RgbColor color1, RgbColor color2, uint8_t blend) {
    uint8_t invBlend = 255 - blend;
    return RgbColor(
      (color1.R * invBlend + color2.R * blend) / 255,
      (color1.G * invBlend + color2.G * blend) / 255,
      (color1.B * invBlend + color2.B * blend) / 255
    );
  }
};

#endif // SPARKLE_EFFECT_H
