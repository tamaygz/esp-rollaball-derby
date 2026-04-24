/**
 * Chase Effect
 * Moving light pattern with fading tail (theater marquee style)
 * Creates an illusion of motion with configurable speed and direction
 */

#ifndef CHASE_EFFECT_H
#define CHASE_EFFECT_H

#include "../LedEffect.h"
#include <Arduino.h>

/**
 * Chase effect with moving pixel and fading tail
 * 
 * A bright pixel moves along the strip with a trailing fade effect.
 * Configurable tail length, speed, and direction.
 * 
 * Extended parameters (use setChaseParams after construction):
 * - tailLength: Number of trailing pixels (1-50)
 * - speedPixelsPerSec: Movement speed in pixels per second (1-100)
 * 
 * Standard parameters:
 * - color: RGB color for the chase
 * - brightness: Maximum brightness (0-255)
 * - direction: FORWARD or REVERSE
 * - durationMs: Total effect duration (0 = infinite)
 * 
 * Usage example:
 * ```
 * ChaseEffect chase(&controller);
 * EffectParams params;
 * params.color = RgbColor(255, 128, 0); // Orange
 * params.brightness = 200;
 * params.direction = DIRECTION_FORWARD;
 * chase.setParams(params);
 * chase.setChaseParams(5, 20); // 5-pixel tail, 20 pixels/sec
 * chase.begin();
 * ```
 */
class ChaseEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  ChaseEffect(LedController* controller) 
    : LedEffect(controller)
    , _tailLength(3)
    , _speedPixelsPerSec(10)
    , _currentPosition(0.0f) {
    _type = EFFECT_CHASE;
  }
  
  /**
   * Set chase-specific parameters
   * @param tailLength Number of trailing pixels (1-50)
   * @param speedPixelsPerSec Movement speed in pixels per second (1-100)
   */
  void setChaseParams(uint8_t tailLength, uint8_t speedPixelsPerSec) {
    // Clamp tail length
    if (tailLength < 1) tailLength = 1;
    if (tailLength > 50) tailLength = 50;
    _tailLength = tailLength;
    
    // Clamp speed
    if (speedPixelsPerSec < 1) speedPixelsPerSec = 1;
    if (speedPixelsPerSec > 100) speedPixelsPerSec = 100;
    _speedPixelsPerSec = speedPixelsPerSec;
  }
  
  /**
   * Initialize effect
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    _currentPosition = 0.0f;
    
    DERBY_LOG_F("[ChaseEffect] Started: tail=%u pixels, speed=%u px/s\n",
                _tailLength, _speedPixelsPerSec);
  }
  
  /**
   * Update effect - move chase pattern
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    
    // Calculate position delta
    // Adjust speed by speed parameter
    float adjustedSpeed = _speedPixelsPerSec * _params.speed;
    float positionDelta = (adjustedSpeed * deltaMs) / 1000.0f;
    
    // Update position
    if (_params.direction == DIRECTION_FORWARD) {
      _currentPosition += positionDelta;
    } else {
      _currentPosition -= positionDelta;
    }
    
    // Wrap position around strip length
    uint16_t ledCount = _controller->getLedCount();
    while (_currentPosition >= ledCount) {
      _currentPosition -= ledCount;
    }
    while (_currentPosition < 0) {
      _currentPosition += ledCount;
    }
    
    // Update LEDs with chase pattern
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
    _currentPosition = 0.0f;
  }
  
  /**
   * Get effect name
   * @return "Chase"
   */
  const char* getName() const override {
    return "Chase";
  }

private:
  uint8_t _tailLength;         // Tail length in pixels
  uint8_t _speedPixelsPerSec;  // Movement speed
  float _currentPosition;      // Current head position (fractional)
  
  /**
   * Update all LEDs with chase pattern
   */
  void updateLeds() {
    uint16_t ledCount = _controller->getLedCount();
    uint16_t headPosition = (uint16_t)_currentPosition;
    
    // Clear all LEDs first
    RgbColor black(0, 0, 0);
    for (uint16_t i = 0; i < ledCount; i++) {
      _controller->setPixel(i, black);
    }
    
    // Draw head at full brightness
    RgbColor headColor(
      (_params.color.R * _params.brightness) / 255,
      (_params.color.G * _params.brightness) / 255,
      (_params.color.B * _params.brightness) / 255
    );
    _controller->setPixel(headPosition, headColor);
    
    // Draw tail with exponential fade
    for (uint8_t i = 1; i <= _tailLength; i++) {
      // Calculate tail pixel position (with wraparound)
      int16_t tailPos;
      if (_params.direction == DIRECTION_FORWARD) {
        tailPos = headPosition - i;
        if (tailPos < 0) tailPos += ledCount;
      } else {
        tailPos = headPosition + i;
        if (tailPos >= ledCount) tailPos -= ledCount;
      }
      
      // Calculate exponential fade: brightness decreases exponentially with distance
      float fadeRatio = (float)(_tailLength - i) / (float)_tailLength;
      fadeRatio = fadeRatio * fadeRatio; // Exponential curve
      uint8_t fadedBrightness = (uint8_t)(_params.brightness * fadeRatio);
      
      RgbColor tailColor(
        (_params.color.R * fadedBrightness) / 255,
        (_params.color.G * fadedBrightness) / 255,
        (_params.color.B * fadedBrightness) / 255
      );
      
      _controller->setPixel(tailPos, tailColor);
    }
    
    _controller->show();
  }
};

#endif // CHASE_EFFECT_H
