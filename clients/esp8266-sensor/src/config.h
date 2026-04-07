#pragma once

// ─── Pin Definitions ──────────────────────────────────────────────────────────
// Wemos D1 Mini pin mapping:  D1 = GPIO5,  D2 = GPIO4
#define PIN_SENSOR_1  5           // D1 → GPIO5 → +1 hole IR break-beam
#define PIN_SENSOR_3  4           // D2 → GPIO4 → +3 hole IR break-beam
#define PIN_LED       LED_BUILTIN // Built-in LED (active LOW on Wemos D1 Mini)

// ─── Sensor Debounce ──────────────────────────────────────────────────────────
#define DEBOUNCE_MS   500UL       // Minimum ms between valid triggers per sensor

// ─── WebSocket ────────────────────────────────────────────────────────────────
#define WS_PATH            "/"
#define WS_BACKOFF_MIN_MS  1000UL
#define WS_BACKOFF_MAX_MS  30000UL

// ─── WiFiManager AP ───────────────────────────────────────────────────────────
#define WIFIMANAGER_AP_PREFIX  "Derby-Sensor-"

// ─── LittleFS ─────────────────────────────────────────────────────────────────
#define CONFIG_FILE  "/config.json"

// ─── Serial ───────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200
