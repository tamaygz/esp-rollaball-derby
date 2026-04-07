#include "sensors.h"

// ─── ISR-shared state ─────────────────────────────────────────────────────────
// Accessed from both ISR and loop() — declared volatile.
// millis() is safe to call from ISR context on ESP8266 (timer-based, not yield-based).

static volatile bool          s_triggered1    = false;
static volatile bool          s_triggered2    = false;
static volatile bool          s_triggered3    = false;
static volatile unsigned long s_lastTrigger1  = 0;
static volatile unsigned long s_lastTrigger2  = 0;
static volatile unsigned long s_lastTrigger3  = 0;

// ─── ISR handlers ─────────────────────────────────────────────────────────────

void IRAM_ATTR Sensors::_isr1() {
    unsigned long now = millis();
    // Arithmetic handles millis() 49-day rollover correctly for unsigned long.
    if (now - s_lastTrigger1 >= DEBOUNCE_MS) {
        s_lastTrigger1 = now;
        s_triggered1   = true;
    }
}

void IRAM_ATTR Sensors::_isr2() {
    unsigned long now = millis();
    if (now - s_lastTrigger2 >= DEBOUNCE_MS) {
        s_lastTrigger2 = now;
        s_triggered2   = true;
    }
}

void IRAM_ATTR Sensors::_isr3() {
    unsigned long now = millis();
    if (now - s_lastTrigger3 >= DEBOUNCE_MS) {
        s_lastTrigger3 = now;
        s_triggered3   = true;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void Sensors::begin() {
    pinMode(PIN_SENSOR_1, INPUT_PULLUP);
    pinMode(PIN_SENSOR_2, INPUT_PULLUP);
    pinMode(PIN_SENSOR_3, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR_1), _isr1, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR_2), _isr2, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR_3), _isr3, FALLING);
}

int Sensors::check() {
    // Disable interrupts while reading+clearing flags to ensure atomicity.
    if (s_triggered1) {
        noInterrupts();
        s_triggered1 = false;
        interrupts();
        return 1;
    }
    if (s_triggered2) {
        noInterrupts();
        s_triggered2 = false;
        interrupts();
        return 2;
    }
    if (s_triggered3) {
        noInterrupts();
        s_triggered3 = false;
        interrupts();
        return 3;
    }
    return 0;
}
