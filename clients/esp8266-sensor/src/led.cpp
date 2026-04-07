#include "led.h"
#include <Arduino.h>

// ─── Constructor ──────────────────────────────────────────────────────────────
// Member init order mirrors declaration order in led.h:
//   _controller (no deps) → _animator (takes &_controller) → _mapper (takes both)
//   → per-effect instances (all take &_controller)

LedManager::LedManager()
    : _animator(&_controller)
    , _mapper(&_controller, &_animator)
    , _blinkEffect(&_controller)
    , _pulseEffect(&_controller)
    , _rainbowEffect(&_controller)
    , _solidEffect(&_controller)
    , _sparkleEffect(&_controller)
{}

// ─── begin ─────────────────────────────────────────────────────────────────────

void LedManager::begin(const LedConfig& cfg) {
    // Clamp LED count to ESP8266 DMA hardware maximum.
    uint16_t count = min(cfg.ledCount, static_cast<uint16_t>(300));

    if (!_controller.begin(count, cfg.pin)) {
        Serial.printf("[LED] ERROR: LedController::begin failed (%u LEDs, pin %u)\n",
                      count, cfg.pin);
        return;
    }

    _controller.setBrightness(cfg.brightness);
    _animator.begin();
    _mapper.begin();

    _config = cfg;
    _config.ledCount = count;
    _begun  = true;

    Serial.printf("[LED] Initialised: %u LEDs, pin=%u, brightness=%u\n",
                  count, cfg.pin, cfg.brightness);

    _playAmbient(_state);
}

// ─── applyConfig ───────────────────────────────────────────────────────────────

void LedManager::applyConfig(const LedConfig& cfg) {
    if (!_begun) {
        begin(cfg);
        return;
    }

    // Reject ESP8266 boot-strapping pins (0, 2, 15) which must be pulled
    // high/low at boot and cannot be used for DMA output reliably.
    uint8_t pin = cfg.pin;
    if (pin == 0 || pin == 2 || pin == 15) {
        Serial.printf("[LED] Ignoring invalid pin %u from led_config — keeping pin %u\n",
                      pin, _config.pin);
        pin = _config.pin;
    }

    uint16_t count = min(cfg.ledCount, static_cast<uint16_t>(300));

    // Only re-initialise the NeoPixelBus strip when the LED count changes.
    // LedController::begin() allocates a new strip without freeing the old one,
    // so calling it unnecessarily leaks memory.
    if (count != _config.ledCount) {
        if (!_controller.begin(count, pin)) {
            Serial.printf("[LED] ERROR: LedController::begin failed during applyConfig\n");
            return;
        }
        Serial.printf("[LED] LED count changed: %u → %u\n", _config.ledCount, count);
    }

    _controller.setBrightness(cfg.brightness);

    _config          = cfg;
    _config.ledCount = count;
    _config.pin      = pin;

    Serial.printf("[LED] Config applied: %u LEDs, brightness=%u\n", count, cfg.brightness);

    // Replay the current ambient effect so it fills the new LED count.
    _playAmbient(_state);
}

// ─── setState ──────────────────────────────────────────────────────────────────

void LedManager::setState(LedState state) {
    if (!_begun || state == _state) return;
    _state = state;
    _playAmbient(state);
}

// ─── onGameEvent ───────────────────────────────────────────────────────────────

void LedManager::onGameEvent(GameEventType event) {
    if (!_begun) return;
    _mapper.onEvent(event);
}

// ─── playTestEffect ────────────────────────────────────────────────────────────

void LedManager::playTestEffect(const LedTestEffectMessage& msg) {
    if (!_begun) return;

    EffectParams p;
    p.color      = RgbColor(msg.r, msg.g, msg.b);
    p.brightness = msg.brightness;
    p.durationMs = 0; // infinite — admin manually stops by changing state

    if (strcmp(msg.effectName, "blink") == 0) {
        uint16_t half = (msg.speedMs > 0) ? (msg.speedMs / 2) : 500;
        _blinkEffect.setParams(p);
        _blinkEffect.setBlinkParams(half, half, 0);
        _animator.playEffect(&_blinkEffect);

    } else if (strcmp(msg.effectName, "pulse") == 0) {
        uint16_t period = (msg.speedMs > 0) ? msg.speedMs : 1000;
        _pulseEffect.setParams(p);
        _pulseEffect.setPeriod(period);
        _animator.playEffect(&_pulseEffect);

    } else if (strcmp(msg.effectName, "rainbow") == 0) {
        uint16_t speed = (msg.speedMs > 0) ? msg.speedMs : 3000;
        p.brightness = (msg.brightness > 0) ? msg.brightness : 180;
        _rainbowEffect.setParams(p);
        _rainbowEffect.setCycleSpeed(speed);
        _animator.playEffect(&_rainbowEffect);

    } else if (strcmp(msg.effectName, "sparkle") == 0) {
        // Use the requested color as sparkle color; background stays dark.
        _sparkleEffect.setParams(p);
        _sparkleEffect.setSparkleParams(
            RgbColor(0, 0, 0),                     // dark background
            RgbColor(msg.r, msg.g, msg.b),          // sparkle color
            0.08f,                                  // 8% density per frame
            8                                       // fade speed
        );
        _animator.playEffect(&_sparkleEffect);

    } else {
        // "solid" and any unknown effect name → solid color
        _solidEffect.setParams(p);
        _animator.playEffect(&_solidEffect);
    }
}

// ─── loop ──────────────────────────────────────────────────────────────────────

void LedManager::loop() {
    if (!_begun) return;
    _controller.loop();
    _animator.loop();
}

// ─── _playAmbient ──────────────────────────────────────────────────────────────

void LedManager::_playAmbient(LedState state) {
    EffectParams p;
    p.durationMs = 0; // all ambient effects run indefinitely

    switch (state) {
        case LedState::NO_WIFI:
            // Fast red blink at 5 Hz (100 ms on / 100 ms off).
            p.color      = RgbColor(255, 0, 0);
            p.brightness = _config.brightness;
            _blinkEffect.setParams(p);
            _blinkEffect.setBlinkParams(100, 100, 0);
            _animator.playEffect(&_blinkEffect);
            break;

        case LedState::WIFI_ONLY:
            // Slow orange blink at 1 Hz (500 ms on / 500 ms off).
            p.color      = RgbColor(255, 128, 0);
            p.brightness = _config.brightness;
            _blinkEffect.setParams(p);
            _blinkEffect.setBlinkParams(500, 500, 0);
            _animator.playEffect(&_blinkEffect);
            break;

        case LedState::WS_CONNECTED:
            // Slow green breathing pulse (2-second cycle).
            p.color      = RgbColor(0, 255, 0);
            p.brightness = _config.brightness;
            _pulseEffect.setParams(p);
            _pulseEffect.setPeriod(2000);
            _animator.playEffect(&_pulseEffect);
            break;
    }
}
