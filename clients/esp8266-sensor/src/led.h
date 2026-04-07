#pragma once
#include <Arduino.h>
#include <LedController.h>
#include <AnimationManager.h>
#include <GameEventMapper.h>
#include <effects/BlinkEffect.h>
#include <effects/PulseEffect.h>
#include <effects/RainbowEffect.h>
#include <effects/SolidEffect.h>
#include <effects/SparkleEffect.h>
#include "config.h"
#include "websocket.h"  // for LedTestEffectMessage

// Visual status states driven by connection and game state.
enum class LedState {
    NO_WIFI,       // Red fast-blink    — no WiFi connectivity
    WIFI_ONLY,     // Orange slow-blink — WiFi up, WebSocket disconnected
    WS_CONNECTED   // Slow green pulse  — fully connected, idle
};

// Manages the LED strip on the sensor.
// Wraps LedController + AnimationManager + GameEventMapper and provides
// the sensor firmware's three consumption points:
//   1. Connection-state ambient effects (setState)
//   2. Game-event one-shot effects      (onGameEvent)
//   3. Admin test effects               (playTestEffect)
class LedManager {
public:
    LedManager();

    // Initialise the LED strip. Call once in setup() BEFORE setState().
    void begin(const LedConfig& cfg);

    // Hot-reload LED configuration (e.g. received via led_config WebSocket message).
    // Diffs against the current config and only re-initialises the strip when the
    // LED count changes, avoiding unnecessary memory operations.
    void applyConfig(const LedConfig& cfg);

    // Change the displayed connection state. Idempotent for same-state calls.
    void setState(LedState state);

    // Trigger a one-shot game-event effect (delegates to GameEventMapper).
    void onGameEvent(GameEventType event);

    // Play an admin-requested test effect from the server.
    void playTestEffect(const LedTestEffectMessage& msg);

    // Must be called every loop() iteration.
    void loop();

private:
    // Declaration order = initialisation order; _controller must be first.
    LedController    _controller;
    AnimationManager _animator;
    GameEventMapper  _mapper;

    // Pre-allocated effect instances reused for ambient/test effects to avoid
    // heap allocations in loop().
    BlinkEffect      _blinkEffect;
    PulseEffect      _pulseEffect;
    RainbowEffect    _rainbowEffect;
    SolidEffect      _solidEffect;
    SparkleEffect    _sparkleEffect;

    LedConfig        _config  = {};
    LedState         _state   = LedState::NO_WIFI;
    bool             _begun   = false;

    // Start the ambient effect for the given connection state.
    void _playAmbient(LedState state);
};
