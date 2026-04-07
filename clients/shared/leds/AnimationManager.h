/**
 * Animation Manager
 * Non-blocking animation loop with FPS control and effect management
 * Handles effect lifecycle, frame timing, and performance diagnostics
 */

#ifndef ANIMATION_MANAGER_H
#define ANIMATION_MANAGER_H

#include "LedEffect.h"
#include <stdint.h>

// Transition states
enum TransitionState {
  TRANSITION_IDLE = 0,        // No transition in progress
  TRANSITION_FADING_OUT = 1,  // Fading out current effect
  TRANSITION_FADING_IN = 2,   // Fading in new effect
  TRANSITION_COMPLETE = 3     // Transition finished, cleanup needed
};

// Animation statistics
struct AnimationStats {
  uint32_t frameCount;          // Total frames rendered since begin()
  uint32_t droppedFrames;       // Frames skipped due to slow effects
  uint32_t avgFrameTimeUs;      // Average frame time in microseconds
  uint8_t currentFPS;           // Current measured FPS
  uint32_t totalElapsedMs;      // Total time since begin()
  
  AnimationStats() 
    : frameCount(0)
    , droppedFrames(0)
    , avgFrameTimeUs(0)
    , currentFPS(0)
    , totalElapsedMs(0) {}
};

/**
 * Animation manager with non-blocking loop
 * 
 * Manages effect lifecycle, frame timing, and yields to WiFi stack.
 * Designed for ESP8266/ESP32 with constrained resources and WiFi requirements.
 * 
 * Usage example:
 * ```
 * AnimationManager animator(&controller);
 * animator.setTargetFPS(30);
 * animator.begin();
 * 
 * SolidEffect solid(&controller);
 * EffectParams params;
 * params.color = RgbColor(255, 0, 0);
 * solid.setParams(params);
 * animator.playEffect(&solid);
 * 
 * void loop() {
 *   animator.loop(); // Call every loop iteration
 * }
 * ```
 */
class AnimationManager {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   */
  AnimationManager(LedController* controller);
  
  /**
   * Initialize animator
   * Call once in setup() before using
   */
  void begin();
  
  /**
   * Main animation loop
   * Call repeatedly in Arduino loop()
   * 
   * Responsibilities:
   * - Calculate delta time since last frame
   * - Update current effect with deltaMs
   * - Enforce FPS limit (skip frames if too fast)
   * - Yield to WiFi stack every 50ms
   * - Track performance statistics
   * - Handle effect completion and cleanup
   * 
   * Non-blocking: completes in <1ms typically
   */
  void loop();
  
  /**
   * Play an effect
   * @param effect Pointer to effect instance (must remain valid until complete)
   * 
   * If an effect is already playing, it will be replaced immediately.
   * For smooth transitions, use transitionTo() instead.
   * 
   * Effect lifecycle:
   * 1. Calls effect->begin()
   * 2. Calls effect->update(deltaMs) every frame
   * 3. When effect->isComplete() returns true, stops and cleans up
   */
  void playEffect(LedEffect* effect);
  
  /**
   * Transition to a new effect with crossfade
   * @param effect Pointer to new effect instance
   * @param durationMs Transition duration in milliseconds (0 = immediate)
   * 
   * If durationMs > 0, smoothly fades from current effect to new effect:
   * 1. Fade out current effect (reduce brightness)
   * 2. Fade in new effect (increase brightness)
   * 3. Clean up old effect when done
   * 
   * If durationMs = 0, behaves like playEffect() (immediate switch).
   * 
   * Transitions can be interrupted by new playEffect() or transitionTo() calls.
   */
  void transitionTo(LedEffect* effect, uint16_t durationMs = 500);
  
  /**
   * Stop current effect
   * Sets effect pointer to null, LEDs remain at last state
   */
  void stop();
  
  /**
   * Check if an effect is currently playing
   * @return true if effect active, false if idle
   */
  bool isPlaying() const;
  
  /**
   * Get current effect
   * @return Pointer to active effect, or nullptr if none
   */
  LedEffect* getCurrentEffect() const;
  
  /**
   * Set target frames per second
   * @param fps Target FPS (15-60), clamped to valid range
   * 
   * Higher FPS = smoother animation but more CPU usage
   * Lower FPS = less smooth but more headroom for WiFi
   * 
   * Recommended:
   * - 30 FPS for ESP8266 with WiFi
   * - 60 FPS for ESP32 or wired scenarios
   */
  void setTargetFPS(uint8_t fps);
  
  /**
   * Get target FPS
   * @return Current target FPS setting
   */
  uint8_t getTargetFPS() const;
  
  /**
   * Get performance statistics
   * @return AnimationStats struct with diagnostics
   * 
   * Useful for debugging performance issues:
   * - High droppedFrames → effect update() too slow
   * - currentFPS < targetFPS → CPU overloaded
   * - avgFrameTimeUs > frame budget → optimize effect
   */
  AnimationStats getStats() const;
  
  /**
   * Reset statistics
   * Clears frame counters and timing averages
   */
  void resetStats();

private:
  LedController* _controller;     // LED controller instance
  LedEffect* _currentEffect;      // Active effect (nullptr if idle)
  
  // Transition state
  LedEffect* _transitionEffect;   // Pending effect during transition (nullptr if no transition)
  TransitionState _transitionState; // Current transition state
  uint16_t _transitionDurationMs; // Total transition duration
  uint32_t _transitionElapsedMs;  // Time elapsed in current transition
  uint8_t _transitionBrightness;  // Brightness multiplier during transition (0-255)
  
  uint8_t _targetFPS;             // Target frames per second (15-60)
  uint32_t _frameIntervalUs;      // Microseconds per frame (1000000/fps)
  
  uint32_t _lastFrameTimeUs;      // micros() timestamp of last frame
  uint32_t _lastYieldTime;        // Last time yield() was called
  
  // Performance tracking
  AnimationStats _stats;
  uint32_t _frameTimeAccumUs;     // Accumulated frame times for averaging
  
  // Frame timing constants
  static const uint8_t MIN_FPS = 15;
  static const uint8_t MAX_FPS = 60;
  static const uint32_t YIELD_INTERVAL_MS = 50; // Yield every 50ms
  
  /**
   * Calculate frame interval in microseconds
   */
  void updateFrameInterval();
  
  /**
   * Update statistics after frame
   */
  void updateStats(uint32_t frameTimeUs);
  
  /**
   * Check if enough time has passed for next frame
   * @return true if ready to render next frame
   */
  bool isFrameReady();
  
  /**
   * Yield to WiFi stack if needed
   */
  void yieldIfNeeded();
  
  /**
   * Update transition state machine
   * @param deltaMs Time elapsed since last frame
   */
  void updateTransition(uint32_t deltaMs);
  
  /**
   * Calculate transition brightness multiplier
   * @return Brightness 0-255 based on transition progress
   */
  uint8_t calculateTransitionBrightness();
};

#endif // ANIMATION_MANAGER_H
