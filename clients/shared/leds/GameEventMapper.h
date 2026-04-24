/**
 * Game Event Mapper
 * Maps game events from server to LED effects.
 *
 * Events are split into two categories:
 *   - Local:  only the owning device reacts  (scoring, streaks, rank changes)
 *   - Global: all devices react              (countdown, game lifecycle, winner)
 *
 * Each event owns a dedicated effect instance so effects never clobber each
 * other's configuration mid-animation.  Dispatch tables built in begin() make
 * adding events a one-line change.
 */

#ifndef GAME_EVENT_MAPPER_H
#define GAME_EVENT_MAPPER_H

#include "GameEvents.h"
#include "AnimationManager.h"
#include "effects/BlinkEffect.h"
#include "effects/ChaseEffect.h"
#include "effects/PulseEffect.h"
#include "effects/RainbowEffect.h"
#include "effects/SparkleEffect.h"
#ifndef NATIVE_TEST
#  include <derby_logger.h>
#endif

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
    // ── Local effect instances (one per LocalEventType) ──────────────────
    , _localBlink1(controller)    ///< SCORE_PLUS1  — device-color single flash
    , _localBlink2(controller)    ///< SCORE_PLUS2  — device-color double flash
    , _localSparkle1(controller)  ///< SCORE_PLUS3  — gold sparkle burst
    , _localPulse1(controller)    ///< ZERO_ROLL    — red pulse
    , _localChase(controller)     ///< TOOK_LEAD    — gold chase
    , _localPulse2(controller)    ///< BECAME_LAST  — dim red pulse
    , _localPulse3(controller)    ///< STREAK_ZERO  — dark red slow pulse
    , _localSparkle2(controller)  ///< STREAK_THREE — intense gold sparkle
    // ── Global effect instances (one per GlobalEventType) ────────────────
    , _globalBlink1(controller)   ///< COUNTDOWN_TICK — white blink
    , _globalChase(controller)    ///< GAME_STARTED   — green chase
    , _globalPulse1(controller)   ///< GAME_PAUSED    — dim amber pulse
    , _globalBlink2(controller)   ///< GAME_RESUMED   — green flash
    , _globalPulse2(controller)   ///< GAME_RESET     — white fade
    , _globalRainbow(controller)  ///< WINNER_SELF    — rainbow
    , _globalPulse3(controller)   ///< WINNER_OTHER   — red pulse
  {}

  void begin() {
    _configureEffects();
    DERBY_LOG_LN("[GameEventMapper] Initialized");
  }

  /**
   * Update the device identity color used for local scoring effects.
   * Must be called after begin() to take effect.
   */
  void setDeviceColor(RgbColor color) {
    _deviceColor    = color;
    _hasDeviceColor = true;
    _configureDeviceColorEffects();
  }

  // ── Device-local events ─────────────────────────────────────────────────

  void onLocalEvent(LocalEventType event) {
    for (uint8_t i = 0; i < LOCAL_EFFECT_COUNT; ++i) {
      if (_localEffects[i].event == event) {
        _animator->playEffect(_localEffects[i].effect, _localEffects[i].priority);
        return;
      }
    }
    // NONE or unknown event — no-op.
  }

  // ── Game-global events ──────────────────────────────────────────────────

  void onGlobalEvent(GlobalEventType event) {
    for (uint8_t i = 0; i < GLOBAL_EFFECT_COUNT; ++i) {
      if (_globalEffects[i].event == event) {
        _animator->playEffect(_globalEffects[i].effect, _globalEffects[i].priority);
        return;
      }
    }
    // NONE or unknown event — no-op.
  }

private:
  // ── Dispatch table entry types ───────────────────────────────────────────

  struct LocalEffectEntry {
    LocalEventType event;
    LedEffect*     effect;
    uint8_t        priority;
  };

  struct GlobalEffectEntry {
    GlobalEventType event;
    LedEffect*      effect;
    uint8_t         priority;
  };

  static const uint8_t LOCAL_EFFECT_COUNT  = 8;
  static const uint8_t GLOBAL_EFFECT_COUNT = 7;

  // ── Per-event effect instances ───────────────────────────────────────────
  // Local
  BlinkEffect   _localBlink1;
  BlinkEffect   _localBlink2;
  SparkleEffect _localSparkle1;
  PulseEffect   _localPulse1;
  ChaseEffect   _localChase;
  PulseEffect   _localPulse2;
  PulseEffect   _localPulse3;
  SparkleEffect _localSparkle2;
  // Global
  BlinkEffect   _globalBlink1;
  ChaseEffect   _globalChase;
  PulseEffect   _globalPulse1;
  BlinkEffect   _globalBlink2;
  PulseEffect   _globalPulse2;
  RainbowEffect _globalRainbow;
  PulseEffect   _globalPulse3;

  // ── Dispatch tables (populated in _configureEffects) ────────────────────
  LocalEffectEntry  _localEffects[LOCAL_EFFECT_COUNT];
  GlobalEffectEntry _globalEffects[GLOBAL_EFFECT_COUNT];

  LedController*    _controller;
  AnimationManager* _animator;

  RgbColor _deviceColor    = RgbColor(255, 255, 255);
  bool     _hasDeviceColor = false;

  RgbColor _getDeviceColor() const {
    return _hasDeviceColor ? _deviceColor : RgbColor(255, 255, 255);
  }

  // ── Effect configuration helpers ─────────────────────────────────────────

  /**
   * Pre-configure all fixed-color effects and build dispatch tables.
   * Called once from begin().
   */
  void _configureEffects() {
    // ── Fixed-color local effects ─────────────────────────────────────────
    {
      EffectParams p; p.brightness = 255; p.durationMs = 300;
      _localSparkle1.setParams(p);
      _localSparkle1.setSparkleParams(RgbColor(50, 25, 0), RgbColor(255, 215, 0), 0.3f, 15);
    }
    {
      EffectParams p; p.color = RgbColor(255, 0, 0); p.brightness = 200; p.durationMs = 500;
      _localPulse1.setParams(p);
      _localPulse1.setPeriod(500);
    }
    {
      EffectParams p; p.color = RgbColor(255, 215, 0); p.brightness = 255; p.durationMs = 1000;
      _localChase.setParams(p);
      _localChase.setChaseParams(5, 30);
    }
    {
      EffectParams p; p.color = RgbColor(255, 0, 0); p.brightness = 100; p.durationMs = 800;
      _localPulse2.setParams(p);
      _localPulse2.setPeriod(800);
    }
    {
      EffectParams p; p.color = RgbColor(180, 0, 0); p.brightness = 160; p.durationMs = 1500;
      _localPulse3.setParams(p);
      _localPulse3.setPeriod(1500);
    }
    {
      EffectParams p; p.brightness = 255; p.durationMs = 600;
      _localSparkle2.setParams(p);
      _localSparkle2.setSparkleParams(RgbColor(80, 40, 0), RgbColor(255, 255, 100), 0.4f, 12);
    }

    // ── Fixed-color global effects ────────────────────────────────────────
    {
      EffectParams p; p.color = RgbColor(255, 255, 255); p.brightness = 200;
      _globalBlink1.setParams(p);
      _globalBlink1.setBlinkParams(600, 400, 1);
    }
    {
      EffectParams p; p.color = RgbColor(0, 255, 0); p.brightness = 255; p.durationMs = 1200;
      _globalChase.setParams(p);
      _globalChase.setChaseParams(5, 25);
    }
    {
      EffectParams p; p.color = RgbColor(255, 160, 0); p.brightness = 80; p.durationMs = 0;
      _globalPulse1.setParams(p);
      _globalPulse1.setPeriod(3000);
    }
    {
      EffectParams p; p.color = RgbColor(0, 255, 0); p.brightness = 255;
      _globalBlink2.setParams(p);
      _globalBlink2.setBlinkParams(300, 0, 1);
    }
    {
      EffectParams p; p.color = RgbColor(255, 255, 255); p.brightness = 120; p.durationMs = 600;
      _globalPulse2.setParams(p);
      _globalPulse2.setPeriod(600);
    }
    {
      EffectParams p; p.brightness = 255; p.durationMs = 0;
      _globalRainbow.setParams(p);
      _globalRainbow.setCycleSpeed(1000);
    }
    {
      EffectParams p; p.color = RgbColor(255, 0, 0); p.brightness = 150; p.durationMs = 0;
      _globalPulse3.setParams(p);
      _globalPulse3.setPeriod(2000);
    }

    // Device-color local effects use current _deviceColor (white until setDeviceColor).
    _configureDeviceColorEffects();

    // ── Build dispatch tables ─────────────────────────────────────────────
    _localEffects[0] = { LocalEventType::SCORE_PLUS1,  &_localBlink1,   AnimationManager::PRIORITY_GAME };
    _localEffects[1] = { LocalEventType::SCORE_PLUS2,  &_localBlink2,   AnimationManager::PRIORITY_GAME };
    _localEffects[2] = { LocalEventType::SCORE_PLUS3,  &_localSparkle1, AnimationManager::PRIORITY_GAME };
    _localEffects[3] = { LocalEventType::ZERO_ROLL,    &_localPulse1,   AnimationManager::PRIORITY_GAME };
    _localEffects[4] = { LocalEventType::TOOK_LEAD,    &_localChase,    AnimationManager::PRIORITY_GAME };
    _localEffects[5] = { LocalEventType::BECAME_LAST,  &_localPulse2,   AnimationManager::PRIORITY_GAME };
    _localEffects[6] = { LocalEventType::STREAK_ZERO,  &_localPulse3,   AnimationManager::PRIORITY_GAME };
    _localEffects[7] = { LocalEventType::STREAK_THREE, &_localSparkle2, AnimationManager::PRIORITY_GAME };

    _globalEffects[0] = { GlobalEventType::COUNTDOWN_TICK, &_globalBlink1,  AnimationManager::PRIORITY_GAME };
    _globalEffects[1] = { GlobalEventType::GAME_STARTED,   &_globalChase,   AnimationManager::PRIORITY_GAME };
    _globalEffects[2] = { GlobalEventType::GAME_PAUSED,    &_globalPulse1,  AnimationManager::PRIORITY_GAME };
    _globalEffects[3] = { GlobalEventType::GAME_RESUMED,   &_globalBlink2,  AnimationManager::PRIORITY_GAME };
    _globalEffects[4] = { GlobalEventType::GAME_RESET,     &_globalPulse2,  AnimationManager::PRIORITY_GAME };
    _globalEffects[5] = { GlobalEventType::WINNER_SELF,    &_globalRainbow, AnimationManager::PRIORITY_GAME };
    _globalEffects[6] = { GlobalEventType::WINNER_OTHER,   &_globalPulse3,  AnimationManager::PRIORITY_GAME };
  }

  /**
   * Configure effects that use the device identity color.
   * Called from begin() (via _configureEffects) and from setDeviceColor().
   */
  void _configureDeviceColorEffects() {
    RgbColor c = _getDeviceColor();

    // SCORE_PLUS1 — device-color single flash (200 ms on, then off)
    {
      EffectParams p; p.color = c; p.brightness = 255;
      _localBlink1.setParams(p);
      _localBlink1.setBlinkParams(200, 0, 1);
    }
    // SCORE_PLUS2 — device-color double flash (200 ms on, 100 ms off)
    {
      EffectParams p; p.color = c; p.brightness = 255;
      _localBlink2.setParams(p);
      _localBlink2.setBlinkParams(200, 100, 2);
    }
  }
};

#endif // GAME_EVENT_MAPPER_H
