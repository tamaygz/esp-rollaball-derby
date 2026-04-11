/**
 * Blink Effect
 * Alternates between color and off with configurable frequency
 * Useful for attention-getting alerts and status indicators
 */

#ifndef BLINK_EFFECT_H
#define BLINK_EFFECT_H

#include "../LedEffect.h"

/**
 * Blink effect with configurable on/off durations
 * 
 * Alternates all LEDs between configured color and off state.
 * Supports infinite blinking or finite blink count.
 * 
 * Extended parameters (use setBlinkParams after construction):
 * - onDurationMs: Duration LEDs are on (milliseconds)
 * - offDurationMs: Duration LEDs are off (milliseconds)
 * - blinkCount: Number of blinks (0 = infinite)
 * 
 * Standard parameters:
 * - color: RGB color to display when on
 * - brightness: Overall brightness (0-255)
 * 
 * Usage example:
 * ```
 * BlinkEffect blink(&controller);
 * EffectParams params;
 * params.color = RgbColor(0, 0, 255); // Blue
 * blink.setParams(params);
 * blink.setBlinkParams(200, 200, 3); // 3 blinks, 200ms on/off
 * blink.begin();
 * ```
 */
class BlinkEffect : public LedEffect {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  BlinkEffect(LedController* controller) 
    : LedEffect(controller)
    , _onDurationMs(500)
    , _offDurationMs(500)
    , _blinkCount(0)
    , _currentBlinks(0)
    , _isOn(false)
    , _stateElapsedMs(0) {
    _type = EFFECT_BLINK;
  }
  
  /**
   * Set blink-specific parameters
   * @param onDurationMs Duration LEDs are on (milliseconds)
   * @param offDurationMs Duration LEDs are off (milliseconds)
   * @param blinkCount Number of complete on/off cycles (0 = infinite)
   */
  void setBlinkParams(uint16_t onDurationMs, uint16_t offDurationMs, uint16_t blinkCount = 0) {
    _onDurationMs = onDurationMs;
    _offDurationMs = offDurationMs;
    _blinkCount = blinkCount;
  }
  
  /**
   * Initialize effect
   */
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    _currentBlinks = 0;
    _isOn = false;
    _stateElapsedMs = 0;
    
    // Start with off state
    turnOff();
    
    Serial.print("[BlinkEffect] Started: ");
    Serial.print(_onDurationMs);
    Serial.print("ms on, ");
    Serial.print(_offDurationMs);
    Serial.print("ms off, ");
    if (_blinkCount == 0) {
      Serial.println("infinite");
    } else {
      Serial.print(_blinkCount);
      Serial.println(" blinks");
    }
  }
  
  /**
   * Update effect - toggle on/off based on timing
   * @param deltaMs Time elapsed since last update
   */
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    _stateElapsedMs += deltaMs;
    
    // Check if it's time to toggle state
    uint16_t stateDuration = _isOn ? _onDurationMs : _offDurationMs;
    
    if (_stateElapsedMs >= stateDuration) {
      // Toggle state
      _isOn = !_isOn;
      _stateElapsedMs = 0;
      
      if (_isOn) {
        turnOn();
        _currentBlinks++;
      } else {
        turnOff();
      }
    }
  }
  
  /**
   * Check if effect completed
   * @return true if blink count reached, false if infinite
   */
  bool isComplete() const override {
    // If blinkCount is 0, effect is infinite
    if (_blinkCount == 0) {
      return false;
    }
    
    // Complete when we've done the requested number of blinks and are off
    return (_currentBlinks >= _blinkCount) && !_isOn;
  }
  
  /**
   * Reset effect state
   */
  void reset() override {
    LedEffect::reset();
    _currentBlinks = 0;
    _isOn = false;
    _stateElapsedMs = 0;
  }
  
  /**
   * Get effect name
   * @return "Blink"
   */
  const char* getName() const override {
    return "Blink";
  }

private:
  uint16_t _onDurationMs;      // Duration LEDs are on
  uint16_t _offDurationMs;     // Duration LEDs are off
  uint16_t _blinkCount;        // Number of blinks (0 = infinite)
  uint16_t _currentBlinks;     // Current blink count
  bool _isOn;                  // Current state (on/off)
  uint32_t _stateElapsedMs;    // Time in current state
  
  /**
   * Turn all LEDs on to configured color
   */
  void turnOn() {
    uint16_t ledCount = _controller->getLedCount();
    
    // Apply brightness scaling
    uint8_t r = (_params.color.R * _params.brightness) / 255;
    uint8_t g = (_params.color.G * _params.brightness) / 255;
    uint8_t b = (_params.color.B * _params.brightness) / 255;
    RgbColor scaledColor(r, g, b);
    
    for (uint16_t i = 0; i < ledCount; i++) {
      _controller->setPixel(i, scaledColor);
    }
    
    _controller->show();
  }
  
  /**
   * Turn all LEDs off
   */
  void turnOff() {
    uint16_t ledCount = _controller->getLedCount();
    RgbColor black(0, 0, 0);
    
    for (uint16_t i = 0; i < ledCount; i++) {
      _controller->setPixel(i, black);
    }
    
    _controller->show();
  }
};

#endif // BLINK_EFFECT_H
