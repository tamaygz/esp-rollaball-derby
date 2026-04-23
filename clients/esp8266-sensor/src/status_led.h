#pragma once
#include <Arduino.h>
#include "config.h"

/**
 * StatusLed — Onboard LED indicator for game events.
 *
 * On ESP8266: the WS2812B strip runs on UART1 which permanently owns GPIO2.
 * The UART1 inverted-TX idle state holds GPIO2 LOW, so the active-LOW onboard
 * LED glows ON between strip frames — producing a natural visible effect with
 * no extra code.  StatusLed therefore skips explicit GPIO control on ESP8266
 * (PIN_STATUS_LED == PIN_LED signals this at compile-time).
 *
 * On ESP32: the WS2812B strip runs on RMT.  RMT claims the strip GPIO
 * exclusively, so the status LED pin must be DIFFERENT from the strip pin.
 * The default strip pin (GPIO4) leaves GPIO2 (onboard LED, active-HIGH) free
 * for explicit blink control.  If the strip is later reconfigured onto GPIO2
 * (e.g. via the admin panel), StatusLed detects the conflict via setStripPin()
 * and silently disables explicit control.
 *
 * Usage:
 *   StatusLed statusLed;
 *   // in setup():
 *   statusLed.begin(currentStripPin);
 *   // when server pushes led_config:
 *   statusLed.setStripPin(newPin);
 *   // on game events:
 *   statusLed.blink();
 *   // in loop():
 *   statusLed.loop();
 */
class StatusLed {
public:
    StatusLed() : _stripPin(PIN_LED), _active(false), _ledOn(false), _onUntil(0) {}

    /**
     * Initialise the status LED.  Call once in setup() after the LED strip has
     * been initialised, passing the strip's current GPIO pin.
     */
    void begin(uint8_t stripPin) {
        _stripPin = stripPin;
        _updateActive();
        if (_active) {
            pinMode(PIN_STATUS_LED, OUTPUT);
            _write(false);
            Serial.printf("[StatusLed] Explicit control active on GPIO%u "
                          "(strip on GPIO%u)\n", PIN_STATUS_LED, _stripPin);
        }
    }

    /**
     * Update the strip pin.  Call whenever the server pushes a new LED config.
     * StatusLed will re-evaluate whether explicit control is safe and
     * reconfigure the pin if necessary.
     */
    void setStripPin(uint8_t pin) {
        if (pin == _stripPin) return;
        _stripPin = pin;
        _updateActive();
        if (_active) {
            pinMode(PIN_STATUS_LED, OUTPUT);
            _write(_ledOn);
            Serial.printf("[StatusLed] Explicit control active on GPIO%u "
                          "(strip moved to GPIO%u)\n", PIN_STATUS_LED, _stripPin);
        } else {
            Serial.printf("[StatusLed] Explicit control disabled "
                          "(strip now on same GPIO%u as status LED)\n", _stripPin);
        }
    }

    /**
     * Trigger a brief non-blocking blink.
     * @param durationMs  How long to keep the LED on (default 150 ms).
     */
    void blink(uint16_t durationMs = 150) {
        if (!_active) return;
        _onUntil = millis() + durationMs;
        if (!_ledOn) {
            _ledOn = true;
            _write(true);
        }
    }

    /** Call every loop() iteration to advance the blink timer. */
    void loop() {
        if (!_active || !_ledOn) return;
        if ((long)(millis() - _onUntil) >= 0) {
            _ledOn = false;
            _write(false);
        }
    }

private:
    uint8_t       _stripPin;
    bool          _active;
    bool          _ledOn;
    unsigned long _onUntil;

    /**
     * Explicit GPIO control is safe only when PIN_STATUS_LED is not the same
     * pin as the strip (which would be owned by a hardware peripheral).
     *
     * ESP8266: always false — UART1 owns GPIO2 regardless.
     * ESP32:   true when strip pin ≠ PIN_STATUS_LED.
     */
    void _updateActive() {
#if defined(ESP8266)
        // UART1 peripheral owns GPIO2 at all times on ESP8266.
        _active = false;
#elif defined(ESP32)
        _active = (PIN_STATUS_LED != _stripPin);
#else
        _active = false;
#endif
    }

    void _write(bool on) {
#if defined(ESP32)
#  if STATUS_LED_ACTIVE_LOW
        digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);
#  else
        digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
#  endif
#else
        (void)on;  // ESP8266: UART1 owns GPIO2; explicit control disabled
#endif
    }
};
