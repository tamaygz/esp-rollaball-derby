/**
 * NeoPixelBusLg.h — minimal stub for PlatformIO native unit tests (NATIVE_TEST).
 *
 * Adds the luminance API on top of NeoPixelBus so that LedPlatform.h's
 * NeoPixelBusLg<…> template instantiations compile without the real library.
 */
#pragma once
#include "NeoPixelBus.h"

template<typename F, typename M, typename G>
class NeoPixelBusLg : public NeoPixelBus<F, M> {
public:
    NeoPixelBusLg(uint16_t count, uint8_t pin)
        : NeoPixelBus<F, M>(count, pin), _luminance(255) {}

    void    SetLuminance(uint8_t l) { _luminance = l; }
    uint8_t GetLuminance() const    { return _luminance; }
private:
    uint8_t _luminance;
};
