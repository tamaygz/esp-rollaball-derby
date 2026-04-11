#pragma once

// Platform detection and NeoPixelBus method selection for ESP8266/ESP32.
// This header abstracts platform-specific LED control methods.

#include <NeoPixelBus.h>
#include <NeoPixelBrightnessBus.h>

// ─── Platform Detection ───────────────────────────────────────────────────────
#if defined(ESP8266)
    #define LED_PLATFORM_ESP8266
    #define LED_PLATFORM_NAME "ESP8266"
    
    // ESP8266 constraints
    #define LED_MAX_COUNT 300
    
    // ESP8266 has two reliable hardware-timed LED methods.
    // The correct one is chosen at runtime based on the configured GPIO pin:
    //   GPIO2 (D4 / TX1) → UART1   — leaves Serial (UART0) free for debugging
    //   GPIO3 (RX)       → DMA     — occupies the RX pin, no Serial input
    using LedMethodUart1 = NeoEsp8266Uart1800KbpsMethod;
    using LedMethodDma   = NeoEsp8266Dma800KbpsMethod;

    using LedStripUart1 = NeoPixelBus<NeoGrbFeature, LedMethodUart1>;
    using LedStripDma   = NeoPixelBus<NeoGrbFeature, LedMethodDma>;

    // Default method alias (used by ESP32 path where only one type exists)
    using LedMethod = LedMethodUart1;
    using LedStrip  = LedStripUart1;
    
#elif defined(ESP32)
    #define LED_PLATFORM_ESP32
    #define LED_PLATFORM_NAME "ESP32"
    
    // ESP32 constraints
    #define LED_MAX_COUNT 1000
    
    // Use RMT channel 0 for rock-solid timing with WiFi active.
    // ESP32 has 8 RMT channels; channel 0 is safe for most applications.
    // NeoPixelBrightnessBus is a drop-in superset of NeoPixelBus that adds SetBrightness().
    using LedMethod = NeoEsp32Rmt0Ws2812xMethod;
    using LedStrip  = NeoPixelBrightnessBus<NeoGrbFeature, LedMethod>;

#else
    #error "Unsupported platform. This code requires ESP8266 or ESP32."
#endif

// Color types for convenience
using RgbColor = ::RgbColor;
using HsvColor = ::HsbColor;  // NeoPixelBus v2.7+ renamed HsvColor to HsbColor

// ─── Compile-Time Assertions ──────────────────────────────────────────────────
#ifdef LED_PLATFORM_ESP8266
    static_assert(LED_MAX_COUNT == 300, "ESP8266 LED limit must be 300");
#endif

#ifdef LED_PLATFORM_ESP32
    static_assert(LED_MAX_COUNT == 1000, "ESP32 LED limit must be 1000");
#endif
