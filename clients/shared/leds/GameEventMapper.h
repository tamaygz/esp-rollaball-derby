/**
 * Game Event Mapper
 * Maps game events from server to LED effects
 * Provides game logic integration for LED animations
 */

#ifndef GAME_EVENT_MAPPER_H
#define GAME_EVENT_MAPPER_H

#include "AnimationManager.h"
#include "effects/SolidEffect.h"
#include "effects/BlinkEffect.h"
#include "effects/PulseEffect.h"
#include "effects/RainbowEffect.h"
#include "effects/ChaseEffect.h"
#include "effects/SparkleEffect.h"

// Game event types (extend as needed)
enum class GameEventType {
  NONE,
  COUNTDOWN_TICK,
  SCORE_PLUS1,
  SCORE_PLUS2,
  SCORE_PLUS3,
  ZERO_ROLL,
  WINNER_SELF,
  WINNER_OTHER
};

/**
 * Maps game events to LED effects
 * 
 * Translates game state changes into visual feedback via LED animations.
 * Pre-configures effects for each event type based on PRD specifications.
 * 
 * Usage example:
 * ```
 * GameEventMapper mapper(&controller, &animator);
 * mapper.begin();
 * 
 * // In game loop:
 * mapper.onEvent(GameEventType::SCORE_PLUS1);
 * ```
 */
class GameEventMapper {
public:
  /**
   * Constructor
   * @param controller Pointer to LedController instance
   * @param animator Pointer to AnimationManager instance
   */
  GameEventMapper(LedController* controller, AnimationManager* animator)
    : _controller(controller)
    , _animator(animator)
    , _blinkEffect(controller)
    , _pulseEffect(controller)
    , _rainbowEffect(controller)
    , _sparkleEffect(controller) {
  }
  
  /**
   * Initialize mapper and pre-configure effects
   */
  void begin() {
    Serial.println("[GameEventMapper] Initialized");
  }
  
  /**
   * Handle a game event and trigger appropriate LED effect
   * @param event Game event type
   */
  void onEvent(GameEventType event) {
    EffectParams params;
    
    switch (event) {
      case GameEventType::COUNTDOWN_TICK:
        // Blue pulse (600ms) per PRD Section 10.4
        params.color = RgbColor(0, 0, 255);
        params.brightness = 200;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(600, 400, 1);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[GameEventMapper] COUNTDOWN_TICK -> BlinkEffect");
        break;
        
      case GameEventType::SCORE_PLUS1:
        // Blue flash (200ms) per PRD Section 10.3
        params.color = RgbColor(0, 0, 255);
        params.brightness = 255;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(200, 0, 1);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[GameEventMapper] SCORE_PLUS1 -> BlinkEffect (Blue)");
        break;
        
      case GameEventType::SCORE_PLUS2:
        // Purple flash twice (200ms on, 100ms off) per PRD Section 10.3
        params.color = RgbColor(128, 0, 255);
        params.brightness = 255;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(200, 100, 2);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[GameEventMapper] SCORE_PLUS2 -> BlinkEffect (Purple)");
        break;
        
      case GameEventType::SCORE_PLUS3:
        // Gold sparkle (300ms) per PRD Section 10.3
        params.brightness = 255;
        params.durationMs = 300;
        _sparkleEffect.setParams(params);
        _sparkleEffect.setSparkleParams(
          RgbColor(50, 25, 0),      // Dark gold background
          RgbColor(255, 215, 0),    // Bright gold sparkle
          0.3f,                     // 30% density for high sparkle
          15                        // Fast fade
        );
        _animator->playEffect(&_sparkleEffect);
        Serial.println("[GameEventMapper] SCORE_PLUS3 -> SparkleEffect (Gold)");
        break;
        
      case GameEventType::ZERO_ROLL:
        // Red pulse fade (500ms) per PRD Section 10.3
        params.color = RgbColor(255, 0, 0);
        params.brightness = 200;
        params.durationMs = 500;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(500);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[GameEventMapper] ZERO_ROLL -> PulseEffect (Red)");
        break;
        
      case GameEventType::WINNER_SELF:
        // Rainbow sparkle cascade (7 seconds) per PRD Section 10.5
        params.brightness = 255;
        params.durationMs = 7000;
        _rainbowEffect.setParams(params);
        _rainbowEffect.setCycleSpeed(1000); // Fast rainbow cycle
        _animator->playEffect(&_rainbowEffect);
        Serial.println("[GameEventMapper] WINNER_SELF -> RainbowEffect");
        break;
        
      case GameEventType::WINNER_OTHER:
        // Red fade (2 seconds) per PRD Section 10.5
        params.color = RgbColor(255, 0, 0);
        params.brightness = 150;
        params.durationMs = 2000;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(2000);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[GameEventMapper] WINNER_OTHER -> PulseEffect (Red)");
        break;
        
      case GameEventType::NONE:
      default:
        // No action
        break;
    }
  }

private:
  LedController* _controller;
  AnimationManager* _animator;
  
  // Pre-allocated effect instances (reused for different events)
  BlinkEffect _blinkEffect;
  PulseEffect _pulseEffect;
  RainbowEffect _rainbowEffect;
  SparkleEffect _sparkleEffect;
};

#endif // GAME_EVENT_MAPPER_H
