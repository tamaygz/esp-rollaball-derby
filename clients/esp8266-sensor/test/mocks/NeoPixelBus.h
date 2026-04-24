/**
 * NeoPixelBus.h — minimal stub for PlatformIO native unit tests (NATIVE_TEST).
 *
 * Defines only the types referenced by LedPlatform.h so that platform-specific
 * using-declarations compile without the real NeoPixelBus library.
 */
#pragma once
#include <stdint.h>

// ─── Color types ──────────────────────────────────────────────────────────────
struct RgbColor {
    uint8_t r, g, b;
    RgbColor()                          : r(0), g(0), b(0) {}
    RgbColor(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

    /** Stub HSV→RGB conversion (returns black; sufficient for logic tests). */
    template<typename HsvT>
    explicit RgbColor(HsvT)             : r(0), g(0), b(0) {}

    static RgbColor LinearBlend(RgbColor, RgbColor b, uint8_t) { return b; }
};

struct HsbColor {
    uint8_t h, s, b;
    HsbColor()                          : h(0), s(0), b(0) {}
    HsbColor(uint8_t h, uint8_t s, uint8_t b) : h(h), s(s), b(b) {}
};

// ─── Method / feature stubs ───────────────────────────────────────────────────
// These are referenced as template arguments in LedPlatform.h's using-declarations.
struct NeoGrbFeature              {};
struct NeoGammaNullMethod         {};
struct NeoEsp8266Uart1800KbpsMethod {};
struct NeoEsp8266Dma800KbpsMethod   {};
struct NeoEsp32Rmt0Ws2812xMethod    {};

// ─── NeoPixelBus template stub ────────────────────────────────────────────────
template<typename F, typename M>
class NeoPixelBus {
public:
    NeoPixelBus(uint16_t count, uint8_t) : _count(count) {}
    void Begin()  {}
    void Show()   {}
    bool IsDirty() const  { return false; }
    uint16_t PixelCount() const { return _count; }
    void SetPixelColor(uint16_t, RgbColor) {}
    RgbColor GetPixelColor(uint16_t) const { return RgbColor(); }
    void ClearTo(RgbColor) {}
private:
    uint16_t _count;
};
