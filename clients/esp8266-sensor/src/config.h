#pragma once

// ─── Pin Definitions ──────────────────────────────────────────────────────────
#if defined(ESP8266)
// Wemos D1 Mini / NodeMCU pin mapping: D1 = GPIO5, D2 = GPIO4, D5 = GPIO14
#define PIN_SENSOR_1  5           // D1 → GPIO5  → +1 hole IR break-beam
#define PIN_SENSOR_2  14          // D5 → GPIO14 → +2 hole IR break-beam
#define PIN_SENSOR_3  4           // D2 → GPIO4  → +3 hole IR break-beam
#define PIN_LED       2           // GPIO2 (D4 / TX1) → WS2812B LED strip
#if PIN_LED == 3
#define LED_CAPABILITIES_METHOD "DMA"
#else
#define LED_CAPABILITIES_METHOD "UART1"
#endif
#define LED_PLATFORM_MAX_LEDS   300
#define LED_GPIO_MAX            16
#elif defined(ESP32)
// ESP32 DevKit defaults (safe interrupt-capable input pins + common LED pin)
#define PIN_SENSOR_1  25          // GPIO25 → +1 hole IR break-beam
#define PIN_SENSOR_2  26          // GPIO26 → +2 hole IR break-beam
#define PIN_SENSOR_3  27          // GPIO27 → +3 hole IR break-beam
// GPIO4 is the default WS2812B data pin on ESP32.
// GPIO2 is kept free for the onboard status LED (see PIN_STATUS_LED below).
#define PIN_LED       4           // GPIO4   → WS2812B LED strip
#define LED_CAPABILITIES_METHOD "RMT"
#define LED_PLATFORM_MAX_LEDS   1000
#define LED_GPIO_MAX            39
#else
#error "Unsupported board: define ESP8266 or ESP32 target"
#endif

// ─── Onboard Status LED ───────────────────────────────────────────────────────
// Secondary visual indicator that blinks during game events.
//
// On ESP8266 (D1 Mini / NodeMCU): the WS2812B strip uses UART1 on GPIO2 in
// inverted mode, which idles the pin LOW — the active-LOW onboard LED therefore
// stays ON between frames.  This produces a natural status-LED effect with no
// extra code.  PIN_STATUS_LED equals PIN_LED to signal that explicit GPIO
// control must be skipped (it would conflict with the UART1 peripheral).
//
// On ESP32 DevKit: GPIO2 (the onboard LED, active-HIGH) is kept free from the
// WS2812B strip (which defaults to GPIO4).  StatusLed drives GPIO2 explicitly,
// producing a visible blink during countdown ticks and scoring events.
//
// If you move the WS2812B strip back to GPIO2 on ESP32 (e.g. via the admin
// panel gpioPin setting), StatusLed detects the conflict at runtime and falls
// back to the side-effect mode automatically.
#if defined(ESP8266)
#  define PIN_STATUS_LED  PIN_LED   // GPIO2 — side-effect mode only (UART1 owns pin)
#  define STATUS_LED_ACTIVE_LOW  true
#elif defined(ESP32)
#  define PIN_STATUS_LED  2         // GPIO2 — onboard blue LED on most DevKit boards
#  define STATUS_LED_ACTIVE_LOW  false
#endif

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
