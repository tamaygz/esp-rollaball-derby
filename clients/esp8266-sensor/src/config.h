#pragma once

// ─── Pin Definitions ──────────────────────────────────────────────────────────
// Wemos D1 Mini pin mapping:  D1 = GPIO5,  D2 = GPIO4,  D5 = GPIO14
#define PIN_SENSOR_1  5           // D1 → GPIO5  → +1 hole IR break-beam
#define PIN_SENSOR_2  14          // D5 → GPIO14 → +2 hole IR break-beam
#define PIN_SENSOR_3  4           // D2 → GPIO4  → +3 hole IR break-beam
#define PIN_LED       3           // GPIO3 (RX) → WS2812B LED strip (DMA method)
                                  // Note: GPIO3 conflicts with Serial debugging on ESP8266

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

// ─── Serial ───────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ─── LED Strip Defaults ───────────────────────────────────────────────────────
// These are used on first boot or when no led_config has been received from server.
#define LED_DEFAULT_COUNT       30    // Number of WS2812B LEDs on the strip
#define LED_DEFAULT_PIN         PIN_LED
#define LED_DEFAULT_BRIGHTNESS  128   // 0-255 (50%)

// ─── LED Topology ─────────────────────────────────────────────────────────────
enum class LedTopology : uint8_t {
    STRIP              = 0,   // Linear arrangement
    RING               = 1,   // Circular arrangement
    MATRIX_ZIGZAG      = 2,   // 2D grid, zigzag wiring
    MATRIX_PROGRESSIVE = 3    // 2D grid, progressive wiring
};

// ─── LED Runtime Configuration ────────────────────────────────────────────────
// Populated from led_config WebSocket message; defaults come from LED_DEFAULT_* above.
struct LedConfig {
    uint16_t    ledCount;
    uint8_t     pin;
    uint8_t     brightness;
    LedTopology topology;
    uint8_t     matrixRows;   // Only used for MATRIX topologies
    uint8_t     matrixCols;   // Only used for MATRIX topologies
};

inline LedConfig ledConfigDefaults() {
    LedConfig cfg;
    cfg.ledCount   = LED_DEFAULT_COUNT;
    cfg.pin        = LED_DEFAULT_PIN;
    cfg.brightness = LED_DEFAULT_BRIGHTNESS;
    cfg.topology   = LedTopology::STRIP;
    cfg.matrixRows = 8;
    cfg.matrixCols = 8;
    return cfg;
}
