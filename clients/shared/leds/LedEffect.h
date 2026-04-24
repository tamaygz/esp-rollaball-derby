/**
 * LED Effect Base Class
 * Abstract base class for all LED animation effects
 * Platform-agnostic, works on ESP8266 and ESP32
 */

#ifndef LED_EFFECT_H
#define LED_EFFECT_H

#include "LedController.h"
#include <stdint.h>
#include <derby_logger.h>

// Effect type identifier enum
enum EffectType {
  EFFECT_SOLID = 0,
  EFFECT_BLINK = 1,
  EFFECT_PULSE = 2,
  EFFECT_RAINBOW = 3,
  EFFECT_CHASE = 4,
  EFFECT_SPARKLE = 5
};

// Effect direction enum
enum EffectDirection {
  DIRECTION_FORWARD = 0,
  DIRECTION_REVERSE = 1
};

// Common effect parameters
struct EffectParams {
  RgbColor color;              // Primary effect color (RGB)
  uint16_t color2Hue;          // Secondary color (HSV hue, 0-360) for multi-color effects
  float speed;                 // Speed multiplier (0.1-10.0, 1.0 = normal)
  uint8_t brightness;          // Brightness (0-255)
  EffectDirection direction;   // Animation direction
  uint16_t durationMs;         // Effect duration (0 = infinite)
  
  // Default constructor
  EffectParams() 
    : color(RgbColor(255, 255, 255))
    , color2Hue(0)
    , speed(1.0f)
    , brightness(255)
    , direction(DIRECTION_FORWARD)
    , durationMs(0) {}
};

/**
 * Abstract base class for LED effects
 * 
 * Effects follow a lifecycle:
 * 1. Construction
 * 2. setParams() - Configure effect parameters
 * 3. begin() - Initialize effect state
 * 4. update() - Called every frame with delta time
 * 5. isComplete() - Check if effect has finished
 * 6. reset() - Clean up and prepare for reuse
 * 
 * Usage example:
 * ```
 * SolidEffect solid(&controller);
 * EffectParams params;
 * params.color = RgbColor(255, 0, 0);
 * solid.setParams(params);
 * solid.begin();
 * while(!solid.isComplete()) {
 *   uint32_t deltaMs = 16; // ~60 FPS
 *   solid.update(deltaMs);
 *   delay(deltaMs);
 * }
 * ```
 */
class LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance (must remain valid for effect lifetime)
   */
  LedEffect(LedController* controller) 
    : _controller(controller)
    , _startTime(0)
    , _elapsedTime(0)
    , _type(EFFECT_SOLID) {}
  
  virtual ~LedEffect() {}
  
  /**
   * Initialize effect state
   * Called once before first update()
   * Override to set up effect-specific state
   */
  virtual void begin() = 0;
  
  /**
   * Update effect for one frame
   * @param deltaMs Time elapsed since last update (milliseconds)
   * 
   * Implementation must:
   * - Update internal animation state
   * - Call _controller->setPixel() to update LED colors
   * - Call _controller->show() to display changes
   * - Complete within frame budget (16ms @ 60 FPS)
   * - Not block (no delay() or long loops)
   * 
   * Called repeatedly by AnimationManager until isComplete() returns true
   */
  virtual void update(uint32_t deltaMs) = 0;
  
  /**
   * Reset effect to initial state
   * Override to clean up effect-specific resources
   * Default implementation resets timing
   */
  virtual void reset() {
    _elapsedTime = 0;
    _startTime = 0;
  }
  
  /**
   * Check if effect has completed
   * @return true if effect is done and should be removed, false if still running
   * 
   * For infinite effects (e.g. solid color), always return false
   * For finite effects (e.g. 3 blinks), return true when complete
   */
  virtual bool isComplete() const = 0;
  
  /**
   * Set effect parameters
   * @param params Effect configuration struct
   * @return true if parameters valid and accepted, false if invalid
   * 
   * Validates parameters against constraints:
   * - speed: 0.1-10.0
   * - brightness: 0-255
   * - durationMs: 0-65535
   */
  bool setParams(const EffectParams& params) {
    // Validate speed
    if (params.speed < 0.1f || params.speed > 10.0f) {
      DERBY_LOG_LN("[LedEffect] Invalid speed, must be 0.1-10.0");
      return false;
    }
    
    // Brightness is uint8_t, always valid 0-255
    
    // Store validated parameters
    _params = params;
    return true;
  }
  
  /**
   * Get effect parameters
   * @return Current effect parameters
   */
  const EffectParams& getParams() const {
    return _params;
  }
  
  /**
   * Get effect type
   * @return EffectType enum value
   */
  EffectType getType() const {
    return _type;
  }
  
  /**
   * Get effect name string
   * @return Human-readable effect name for logging
   */
  virtual const char* getName() const = 0;
  
  /**
   * Get elapsed time since effect started
   * @return Milliseconds since begin() was called
   */
  uint32_t getElapsedTime() const {
    return _elapsedTime;
  }

protected:
  LedController* _controller;     // LED controller instance
  uint32_t _startTime;            // Effect start time (millis())
  uint32_t _elapsedTime;          // Total elapsed time (milliseconds)
  EffectParams _params;           // Effect configuration
  EffectType _type;               // Effect type identifier
  
  /**
   * Update elapsed time tracker
   * Call this at the start of update() in derived classes
   */
  void updateElapsedTime(uint32_t deltaMs) {
    _elapsedTime += deltaMs;
  }
  
  /**
   * Check if effect duration exceeded
   * @return true if durationMs > 0 and elapsed time exceeds it
   */
  bool isDurationExceeded() const {
    if (_params.durationMs == 0) {
      return false; // Infinite duration
    }
    return _elapsedTime >= _params.durationMs;
  }
};

#endif // LED_EFFECT_H
