#pragma once

// Platform detection and NeoPixelBus method selection for ESP8266/ESP32.
// This header abstracts platform-specific LED control methods.

#include <NeoPixelBus.h>

// ─── Platform Detection ───────────────────────────────────────────────────────
#if defined(ESP8266)
    #define LED_PLATFORM_ESP8266
    #define LED_PLATFORM_NAME "ESP8266"
    
    // ESP8266 constraints
    #define LED_MAX_COUNT 300
    
    // Use DMA method on GPIO3 (RX pin) for best performance and WiFi compatibility.
    // Alternative: Use NeoEsp8266Uart1Ws2812xMethod for configurable GPIO pin.
    using LedMethod = NeoEsp8266Dma800KbpsMethod;
    
#elif defined(ESP32)
    #define LED_PLATFORM_ESP32
    #define LED_PLATFORM_NAME "ESP32"
    
    // ESP32 constraints
    #define LED_MAX_COUNT 1000
    
    // Use RMT channel 0 for rock-solid timing with WiFi active.
    // ESP32 has 8 RMT channels; channel 0 is safe for most applications.
    using LedMethod = NeoEsp32Rmt0Ws2812xMethod;
    
#else
    #error "Unsupported platform. This code requires ESP8266 or ESP32."
#endif

// ─── Type Aliases ─────────────────────────────────────────────────────────────
// WS2812B uses GRB color order (not RGB). NeoPixelBus handles this automatically.
using LedStrip = NeoPixelBus<NeoGrbFeature, LedMethod>;

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
