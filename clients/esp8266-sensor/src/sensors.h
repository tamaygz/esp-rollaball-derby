#pragma once
#include <Arduino.h>
#include "config.h"

// Reads two IR break-beam sensors via hardware interrupts.
// ISRs are marked ICACHE_RAM_ATTR so they execute from IRAM (ESP8266 requirement).
// Debounce is enforced inside the ISR using millis() — minimum DEBOUNCE_MS between
// accepted triggers per sensor.
class Sensors {
public:
    // Attach interrupts on PIN_SENSOR_1, PIN_SENSOR_2, and PIN_SENSOR_3. Call once in setup().
    void begin();

    // Poll triggered flags from loop(). Returns 0 (nothing), 1 (+1 sensor), 2 (+2 sensor),
    // or 3 (+3 sensor). Clears the flag atomically before returning.
    int check();

private:
    static void IRAM_ATTR _isr1();
    static void IRAM_ATTR _isr2();
    static void IRAM_ATTR _isr3();
};
