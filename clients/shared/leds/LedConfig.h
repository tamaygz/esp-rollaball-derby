#pragma once
#include <stdint.h>

/**
 * LedConfig — shared LED runtime configuration.
 *
 * Populated from led_config WebSocket message; defaults come from
 * LED_DEFAULT_* defines in each client's config.h.
 * Shared because MatrixDisplay, LedController callers, and both firmware
 * clients all use the same struct.
 */

// ─── LED Topology ─────────────────────────────────────────────────────────────
enum class LedTopology : uint8_t {
    STRIP              = 0,   // Linear arrangement
    RING               = 1,   // Circular arrangement
    MATRIX_ZIGZAG      = 2,   // 2-D grid, zigzag wiring
    MATRIX_PROGRESSIVE = 3    // 2-D grid, progressive wiring
};

// ─── LED Runtime Configuration ────────────────────────────────────────────────
struct LedConfig {
    uint16_t    ledCount;
    uint8_t     pin;
    uint8_t     brightness;
    LedTopology topology;
    uint8_t     matrixRows;     // Only used for MATRIX topologies
    uint8_t     matrixCols;     // Only used for MATRIX topologies
    uint8_t     deviceColorR;   // Device identity colour (from server)
    uint8_t     deviceColorG;
    uint8_t     deviceColorB;
    bool        hasDeviceColor; // true once server has assigned a colour
};

// ─── Default factory ──────────────────────────────────────────────────────────
// Clients override LED_DEFAULT_COUNT / LED_DEFAULT_PIN / LED_DEFAULT_BRIGHTNESS
// in their own config.h; otherwise these sensible defaults are used.
#ifndef LED_DEFAULT_COUNT
#define LED_DEFAULT_COUNT 30
#endif
#ifndef LED_DEFAULT_PIN
#define LED_DEFAULT_PIN 2
#endif
#ifndef LED_DEFAULT_BRIGHTNESS
#define LED_DEFAULT_BRIGHTNESS 128
#endif

inline LedConfig ledConfigDefaults() {
    LedConfig cfg;
    cfg.ledCount       = LED_DEFAULT_COUNT;
    cfg.pin            = LED_DEFAULT_PIN;
    cfg.brightness     = LED_DEFAULT_BRIGHTNESS;
    cfg.topology       = LedTopology::STRIP;
    cfg.matrixRows     = 8;
    cfg.matrixCols     = 8;
    cfg.deviceColorR   = 0;
    cfg.deviceColorG   = 0;
    cfg.deviceColorB   = 0;
    cfg.hasDeviceColor = false;
    return cfg;
}
