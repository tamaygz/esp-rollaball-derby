/**
 * Game Event Mapper
 * Maps game events from server to LED effects.
 *
 * Events are split into two categories:
 *   - Local:  only the owning device reacts  (scoring, streaks, rank changes)
 *   - Global: all devices react              (countdown, game lifecycle, winner)
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

// Device-local event types (mirror LocalEvent priority order in websocket.h).
enum class LocalEventType {
  NONE,
  BECAME_LAST,
  ZERO_ROLL,
  STREAK_ZERO,
  SCORE_PLUS1,
  SCORE_PLUS2,
  SCORE_PLUS3,
  STREAK_THREE,
  TOOK_LEAD,
};

// Game-global event types (mirror GlobalEvent in websocket.h).
enum class GlobalEventType {
  NONE,
  COUNTDOWN_TICK,
  GAME_STARTED,
  GAME_PAUSED,
  GAME_RESUMED,
  GAME_RESET,
  WINNER_SELF,
  WINNER_OTHER,
};

/**
 * Maps game events to LED effects.
 *
 * Local events use the device identity color; global events use fixed colors
 * so all devices show a coordinated response.
 */
class GameEventMapper {
public:
  GameEventMapper(LedController* controller, AnimationManager* animator)
    : _controller(controller)
    , _animator(animator)
    , _blinkEffect(controller)
    , _chaseEffect(controller)
    , _pulseEffect(controller)
    , _rainbowEffect(controller)
    , _sparkleEffect(controller) {
  }

  void begin() {
    Serial.println("[GameEventMapper] Initialized");
  }

  /**
   * Update the device identity color used for local scoring effects.
   */
  void setDeviceColor(RgbColor color) {
    _deviceColor    = color;
    _hasDeviceColor = true;
  }

  // ── Device-local events ─────────────────────────────────────────────────

  void onLocalEvent(LocalEventType event) {
    EffectParams params;

    switch (event) {
      case LocalEventType::SCORE_PLUS1:
        // Device-color flash (200 ms).
        params.color = _getDeviceColor();
        params.brightness = 255;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(200, 0, 1);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[EventMapper] LOCAL SCORE_PLUS1 -> Blink (device color)");
        break;

      case LocalEventType::SCORE_PLUS2:
        // Device-color flash twice (200 ms on, 100 ms off).
        params.color = _getDeviceColor();
        params.brightness = 255;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(200, 100, 2);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[EventMapper] LOCAL SCORE_PLUS2 -> Blink x2 (device color)");
        break;

      case LocalEventType::SCORE_PLUS3:
        // Gold sparkle burst (300 ms).
        params.brightness = 255;
        params.durationMs = 300;
        _sparkleEffect.setParams(params);
        _sparkleEffect.setSparkleParams(
          RgbColor(50, 25, 0),
          RgbColor(255, 215, 0),
          0.3f, 15
        );
        _animator->playEffect(&_sparkleEffect);
        Serial.println("[EventMapper] LOCAL SCORE_PLUS3 -> Sparkle (gold)");
        break;

      case LocalEventType::ZERO_ROLL:
        // Red pulse fade (500 ms).
        params.color = RgbColor(255, 0, 0);
        params.brightness = 200;
        params.durationMs = 500;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(500);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] LOCAL ZERO_ROLL -> Pulse (red)");
        break;

      case LocalEventType::TOOK_LEAD:
        // Gold chase (1 s) — celebratory lead takeover.
        params.color = RgbColor(255, 215, 0);
        params.brightness = 255;
        params.durationMs = 1000;
        _chaseEffect.setParams(params);
        _chaseEffect.setChaseParams(5, 30);
        _animator->playEffect(&_chaseEffect);
        Serial.println("[EventMapper] LOCAL TOOK_LEAD -> Chase (gold)");
        break;

      case LocalEventType::BECAME_LAST:
        // Dim red pulse (800 ms) — dropped to last.
        params.color = RgbColor(255, 0, 0);
        params.brightness = 100;
        params.durationMs = 800;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(800);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] LOCAL BECAME_LAST -> Pulse (dim red)");
        break;

      case LocalEventType::STREAK_ZERO:
        // Slow dark-red pulse (1.5 s) — 3 consecutive zeros.
        params.color = RgbColor(180, 0, 0);
        params.brightness = 160;
        params.durationMs = 1500;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(1500);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] LOCAL STREAK_ZERO -> Pulse (dark red, slow)");
        break;

      case LocalEventType::STREAK_THREE:
        // Intense gold sparkle (600 ms) — 2 consecutive +3 rolls.
        params.brightness = 255;
        params.durationMs = 600;
        _sparkleEffect.setParams(params);
        _sparkleEffect.setSparkleParams(
          RgbColor(80, 40, 0),
          RgbColor(255, 255, 100),
          0.4f, 12
        );
        _animator->playEffect(&_sparkleEffect);
        Serial.println("[EventMapper] LOCAL STREAK_THREE -> Sparkle (intense gold)");
        break;

      default: break;
    }
  }

  // ── Game-global events ──────────────────────────────────────────────────

  void onGlobalEvent(GlobalEventType event) {
    EffectParams params;

    switch (event) {
      case GlobalEventType::COUNTDOWN_TICK:
        // White blink (600 ms on, 400 ms off).
        params.color = RgbColor(255, 255, 255);
        params.brightness = 200;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(600, 400, 1);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[EventMapper] GLOBAL COUNTDOWN_TICK -> Blink (white)");
        break;

      case GlobalEventType::GAME_STARTED:
        // Green chase (1.2 s) — race is on.
        params.color = RgbColor(0, 255, 0);
        params.brightness = 255;
        params.durationMs = 1200;
        _chaseEffect.setParams(params);
        _chaseEffect.setChaseParams(5, 25);
        _animator->playEffect(&_chaseEffect);
        Serial.println("[EventMapper] GLOBAL GAME_STARTED -> Chase (green)");
        break;

      case GlobalEventType::GAME_PAUSED:
        // Dim amber pulse — game on hold.
        params.color = RgbColor(255, 160, 0);
        params.brightness = 80;
        params.durationMs = 0;  // runs until resumed/reset
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(3000);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] GLOBAL GAME_PAUSED -> Pulse (dim amber)");
        break;

      case GlobalEventType::GAME_RESUMED:
        // Green flash — back in action.
        params.color = RgbColor(0, 255, 0);
        params.brightness = 255;
        _blinkEffect.setParams(params);
        _blinkEffect.setBlinkParams(300, 0, 1);
        _animator->playEffect(&_blinkEffect);
        Serial.println("[EventMapper] GLOBAL GAME_RESUMED -> Blink (green)");
        break;

      case GlobalEventType::GAME_RESET:
        // Brief white fade — back to idle (ambient will take over).
        params.color = RgbColor(255, 255, 255);
        params.brightness = 120;
        params.durationMs = 600;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(600);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] GLOBAL GAME_RESET -> Pulse (white fade)");
        break;

      case GlobalEventType::WINNER_SELF:
        // Rainbow — runs until game_reset arrives.
        params.brightness = 255;
        params.durationMs = 0;
        _rainbowEffect.setParams(params);
        _rainbowEffect.setCycleSpeed(1000);
        _animator->playEffect(&_rainbowEffect);
        Serial.println("[EventMapper] GLOBAL WINNER_SELF -> Rainbow (until reset)");
        break;

      case GlobalEventType::WINNER_OTHER:
        // Red pulse — runs until game_reset arrives.
        params.color = RgbColor(255, 0, 0);
        params.brightness = 150;
        params.durationMs = 0;
        _pulseEffect.setParams(params);
        _pulseEffect.setPeriod(2000);
        _animator->playEffect(&_pulseEffect);
        Serial.println("[EventMapper] GLOBAL WINNER_OTHER -> Pulse red (until reset)");
        break;

      default: break;
    }
  }

private:
  LedController* _controller;
  AnimationManager* _animator;

  RgbColor _getDeviceColor() const {
    return _hasDeviceColor ? _deviceColor : RgbColor(255, 255, 255);
  }

  // Pre-allocated effect instances (reused across events)
  BlinkEffect   _blinkEffect;
  ChaseEffect   _chaseEffect;
  PulseEffect   _pulseEffect;
  RainbowEffect _rainbowEffect;
  SparkleEffect _sparkleEffect;

  RgbColor _deviceColor    = RgbColor(255, 255, 255);
  bool     _hasDeviceColor = false;
};

#endif // GAME_EVENT_MAPPER_H
