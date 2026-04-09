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
    , _chaseEffect(&_controller)
    , _pulseEffect(&_controller)
    , _rainbowEffect(&_controller)
    , _solidEffect(&_controller)
    , _sparkleEffect(&_controller)
    , _startupColor(
        static_cast<uint8_t>(random(64, 256)),
        static_cast<uint8_t>(random(64, 256)),
        static_cast<uint8_t>(random(64, 256)))
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

    // Sync the device identity color so scoring effects use it.
    _mapper.setDeviceColor(_getDeviceColor());

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

    uint8_t  pin   = cfg.pin;
    uint16_t count = min(cfg.ledCount, static_cast<uint16_t>(300));

    // Re-initialise the NeoPixelBus strip when LED count or pin changes
    // (different pin may switch between DMA and UART1 methods).
    if (count != _config.ledCount || pin != _config.pin) {
        if (!_controller.begin(count, pin)) {
            Serial.printf("[LED] ERROR: LedController::begin failed during applyConfig\n");
            return;
        }
        Serial.printf("[LED] Strip re-initialised: %u LEDs, pin=%u\n", count, pin);
    }

    _controller.setBrightness(cfg.brightness);

    _config          = cfg;
    _config.ledCount = count;
    _config.pin      = pin;

    // Sync the device identity color so scoring effects use it.
    _mapper.setDeviceColor(_getDeviceColor());

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

// ─── onLocalEvent / onGlobalEvent ──────────────────────────────────────────────

void LedManager::onLocalEvent(LocalEventType event) {
    if (!_begun) return;
    _mapper.onLocalEvent(event);
}

void LedManager::onGlobalEvent(GlobalEventType event) {
    if (!_begun) return;

    // Track game lifecycle for ambient suppression.
    switch (event) {
        case GlobalEventType::GAME_STARTED:
        case GlobalEventType::GAME_RESUMED:
            _gameActive = true;
            break;
        case GlobalEventType::GAME_RESET:
        case GlobalEventType::WINNER_SELF:
        case GlobalEventType::WINNER_OTHER:
            _gameActive = false;
            break;
        default: break;
    }

    _mapper.onGlobalEvent(event);
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

    } else if (strcmp(msg.effectName, "chase") == 0) {
        uint8_t speed = (msg.speedMs > 0) ? min((uint16_t)100, (uint16_t)(1000 / msg.speedMs * 10)) : 20;
        _chaseEffect.setParams(p);
        _chaseEffect.setChaseParams(5, speed);
        _animator.playEffect(&_chaseEffect);

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

    // When no effect is playing (one-shot completed / strip cleared),
    // restart the ambient pulse — but only when no game is active.
    // During gameplay the strip stays dark between events.
    if (!_animator.isPlaying() && !_gameActive) {
        _playAmbient(_state);
    }
}

// ─── _playAmbient ──────────────────────────────────────────────────────────────

void LedManager::_playAmbient(LedState state) {
    EffectParams p;
    p.durationMs = 0; // all ambient effects run indefinitely

    // Resolve the device identity color (falls back to a random color when no
    // server config has been received yet — REQ-007).
    RgbColor devColor = _getDeviceColor();

    switch (state) {
        case LedState::NO_WIFI:
            // Fast red blink at 5 Hz (100 ms on / 100 ms off).
            // NO_WIFI always stays red for clear error visibility.
            p.color      = RgbColor(255, 0, 0);
            p.brightness = _config.brightness;
            _blinkEffect.setParams(p);
            _blinkEffect.setBlinkParams(100, 100, 0);
            _animator.playEffect(&_blinkEffect);
            break;

        case LedState::WIFI_ONLY:
            // Slow device-color blink at 1 Hz.
            p.color      = devColor;
            p.brightness = _config.brightness;
            _blinkEffect.setParams(p);
            _blinkEffect.setBlinkParams(500, 500, 0);
            _animator.playEffect(&_blinkEffect);
            break;

        case LedState::WS_CONNECTED:
            // Slow device-color breathing pulse (2-second cycle).
            p.color      = devColor;
            p.brightness = _config.brightness;
            _pulseEffect.setParams(p);
            _pulseEffect.setPeriod(2000);
            _animator.playEffect(&_pulseEffect);
            break;
    }
}

RgbColor LedManager::_getDeviceColor() {
    if (_config.hasDeviceColor) {
        return RgbColor(_config.deviceColorR, _config.deviceColorG, _config.deviceColorB);
    }
    // No server-assigned color yet — use the random startup color (REQ-007).
    return _startupColor;
}
