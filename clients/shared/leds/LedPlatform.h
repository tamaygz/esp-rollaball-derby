#pragma once

// Platform detection and NeoPixelBus method selection for ESP8266/ESP32.
// This header abstracts platform-specific LED control methods.

#include <NeoPixelBus.h>
#include <NeoPixelBusLg.h>

// ─── Platform Detection ───────────────────────────────────────────────────────
#if defined(ESP8266)
    #define LED_PLATFORM_ESP8266
    #define LED_PLATFORM_NAME "ESP8266"
    
    // ESP8266 constraints
    #define LED_MAX_COUNT 300
    // ESP8266 valid LED GPIO pins are enumerated: GPIO2 (UART1) and GPIO3 (DMA) only.
    // There is no LED_GPIO_MAX — a numeric upper bound is meaningless for this platform.
    // Pin validation must use an explicit set check — see ledPinIsValid() below.
    
    // ESP8266 has two reliable hardware-timed LED methods.
    // The correct one is chosen at runtime based on the configured GPIO pin:
    //   GPIO2 (D4 / TX1) → UART1   — leaves Serial (UART0) free for debugging
    //   GPIO3 (RX)       → DMA     — occupies the RX pin, no Serial input
    using LedMethodUart1 = NeoEsp8266Uart1800KbpsMethod;
    using LedMethodDma   = NeoEsp8266Dma800KbpsMethod;

    // NeoPixelBusLg provides luminance control used by MatrixDisplay.
    using LedStripUart1 = NeoPixelBusLg<NeoGrbFeature, LedMethodUart1, NeoGammaNullMethod>;
    using LedStripDma   = NeoPixelBusLg<NeoGrbFeature, LedMethodDma, NeoGammaNullMethod>;

    // Default method alias (used by ESP32 path where only one type exists)
    using LedMethod = LedMethodUart1;
    using LedStrip  = LedStripUart1;
    
#elif defined(ESP32)
    #define LED_PLATFORM_ESP32
    #define LED_PLATFORM_NAME "ESP32"
    
    // ESP32 constraints
    #define LED_MAX_COUNT 1000
    // Maximum GPIO pin index on standard ESP32 modules (GPIO0–GPIO39).
    // All output-capable GPIOs are valid for RMT-driven LED strips.
    #define LED_GPIO_MAX  39

    // Use RMT channel 0 for rock-solid timing with WiFi active.
    // ESP32 has 8 RMT channels; channel 0 is safe for most applications.
    // NeoPixelBusLg provides luminance control for brightness adjustment.
    using LedMethod = NeoEsp32Rmt0Ws2812xMethod;
    using LedStrip  = NeoPixelBusLg<NeoGrbFeature, LedMethod, NeoGammaNullMethod>;

#else
    #error "Unsupported platform. This code requires ESP8266 or ESP32."
#endif

// Color types for convenience
using RgbColor = ::RgbColor;
using HsvColor = ::HsbColor;  // NeoPixelBus v2.7+ renamed HsvColor to HsbColor

// ─── Platform LED Pin Validation ─────────────────────────────────────────────
// Returns true if `pin` is a valid data pin for hardware-timed WS2812B output.
//   ESP8266: GPIO2 (UART1, leaves Serial free) or GPIO3 (DMA, occupies RX pin).
//   ESP32:   RMT-capable; any GPIO 0–LED_GPIO_MAX is valid.
inline bool ledPinIsValid(int pin) {
#if defined(ESP8266)
    return pin == 2 || pin == 3;
#elif defined(ESP32)
    return pin >= 0 && pin <= LED_GPIO_MAX;
#else
    return false;
#endif
}

// ─── Compile-Time Assertions ──────────────────────────────────────────────────
// Sanity checks to prevent accidental limit changes without also updating
// the NeoPixelBus buffer allocations and display math that depend on these values.
// Update both the #define and the assert together.
#ifdef LED_PLATFORM_ESP8266
    static_assert(LED_MAX_COUNT == 300, "ESP8266 LED limit must be 300");
#endif

#ifdef LED_PLATFORM_ESP32
    static_assert(LED_MAX_COUNT == 1000, "ESP32 LED limit must be 1000");
#endif
