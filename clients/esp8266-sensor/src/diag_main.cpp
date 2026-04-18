// ─── Laser Sensor Diagnostic v3 — Analog A0 ─────────────────────────────────
// Tests A0 with the KY-008 bare-LDR receiver to find the threshold between
// "laser hitting receiver" and "beam blocked."
//
// Flash:  pio run -e diag -t upload && pio device monitor -b 115200 -e diag
// Delete this file + the [env:diag] section in platformio.ini when done.
// ──────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>

#define ANALOG_PIN  A0  // Only analog-capable pin on ESP8266
#define SAMPLES     5   // Averaged reads for stability

static int lastAvg       = -1;
static unsigned long lastPrint = 0;
static int minSeen = 1024;
static int maxSeen = 0;

static int readAvg() {
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(ANALOG_PIN);
        delayMicroseconds(200);
    }
    return sum / SAMPLES;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("═══════════════════════════════════════════════════════"));
    Serial.println(F("  Laser Sensor Diagnostic v3 — Analog A0"));
    Serial.println(F("═══════════════════════════════════════════════════════"));
    Serial.println();
    Serial.println(F("  1. Aim laser at receiver → note the A0 value"));
    Serial.println(F("  2. Block the beam        → note if value changes"));
    Serial.println(F("  3. We need a gap of ~100+ between the two states"));
    Serial.println();
    Serial.println(F("  A0 range: 0 (0V) to 1023 (3.3V)"));
    Serial.println(F("───────────────────────────────────────────────────────"));

    lastAvg = readAvg();
    lastPrint = millis();
}

void loop() {
    int avg = readAvg();
    unsigned long now = millis();

    if (avg < minSeen) minSeen = avg;
    if (avg > maxSeen) maxSeen = avg;

    // Print on significant change (>20 units)
    if (abs(avg - lastAvg) > 20) {
        Serial.printf("[%8lu ms] A0=%4d  (was %4d, delta=%+d)  min=%d max=%d\n",
                      now, avg, lastAvg, avg - lastAvg, minSeen, maxSeen);
        lastAvg   = avg;
        lastPrint = now;
    }

    // Heartbeat every 1s
    if (now - lastPrint >= 1000) {
        Serial.printf("[%8lu ms] A0=%4d  (steady)  min=%d max=%d  range=%d\n",
                      now, avg, minSeen, maxSeen, maxSeen - minSeen);
        lastPrint = now;
    }

    delay(10);  // yield
}
