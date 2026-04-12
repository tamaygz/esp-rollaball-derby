#pragma once

// ─── Pin Definitions ──────────────────────────────────────────────────────────
// Wemos D1 Mini pin mapping:  D1 = GPIO5,  D2 = GPIO4,  D5 = GPIO14
#define PIN_SENSOR_1  5           // D1 → GPIO5  → +1 hole IR break-beam
#define PIN_SENSOR_2  14          // D5 → GPIO14 → +2 hole IR break-beam
#define PIN_SENSOR_3  4           // D2 → GPIO4  → +3 hole IR break-beam
#define PIN_LED       2           // GPIO2 (D4 / TX1) → WS2812B LED strip (UART1 method)

// ─── Sensor Debounce ──────────────────────────────────────────────────────────
#define DEBOUNCE_MS   500UL       // Minimum ms between valid triggers per sensor

// ─── WebSocket ────────────────────────────────────────────────────────────────
#define WS_PATH            "/"
#define WS_BACKOFF_MIN_MS  1000UL
#define WS_BACKOFF_MAX_MS  30000UL

// ─── HTTP Config Server ───────────────────────────────────────────────────────
#define HTTP_CONFIG_PORT  80        // Accepts POST /config to update server IP/port/name

// ─── WiFiManager AP ───────────────────────────────────────────────────────────
#define WIFIMANAGER_AP_PREFIX  "Derby-Sensor-"

// ─── LittleFS ─────────────────────────────────────────────────────────────────
#define CONFIG_FILE  "/config.json"
#define STATE_FILE   "/state.json"   // Runtime state persisted across reboots
#define STATE_TMP    "/state.tmp"    // Temp file for atomic write

// ─── Serial ───────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ─── LED Strip Defaults ───────────────────────────────────────────────────────
// These are used on first boot or when no led_config has been received from server.
#define LED_DEFAULT_COUNT       30    // Number of WS2812B LEDs on the strip
#define LED_DEFAULT_PIN         PIN_LED
#define LED_DEFAULT_BRIGHTNESS  128   // 0-255 (50%)

// ─── LED Config ──────────────────────────────────────────────────────────────
// LedTopology, LedConfig and ledConfigDefaults() live in the shared leds lib.
// LED_DEFAULT_* are defined above so the shared header picks them up.
#include <leds/LedConfig.h>
