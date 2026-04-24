/**
 * Arduino.h — minimal mock for PlatformIO native unit tests (NATIVE_TEST).
 *
 * Provides stub implementations of the Arduino runtime functions used by
 * LedController.cpp and AnimationManager.cpp so they compile and link on the
 * host machine without actual hardware or the Arduino SDK.
 *
 * Time functions (_mock_millis / _mock_micros) can be advanced by tests to
 * exercise time-dependent logic without sleeping.
 */
#pragma once
#include <stdint.h>
#include <cstdarg>
#include <cstdio>

// ─── Mockable time ────────────────────────────────────────────────────────────
// Tests can advance these to simulate time passing.
static uint32_t _mock_millis = 0;
static uint32_t _mock_micros = 0;

inline uint32_t millis()                  { return _mock_millis; }
inline uint32_t micros()                  { return _mock_micros; }
inline void     yield()                   {}
inline void     delay(uint32_t)           {}
inline void     delayMicroseconds(uint32_t) {}

// ─── Math / random stubs ──────────────────────────────────────────────────────
// Arduino's random() and randomSeed() are not available on host — stub them.
inline long     random(long max)                     { return max > 0 ? (_mock_millis % max) : 0; }
inline long     random(long min, long max)           { return min < max ? (min + (_mock_millis % (max - min))) : min; }
inline void     randomSeed(unsigned long)            {}
// min/max/abs/constrain conflict with <algorithm>; define only if not already present.
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef abs
#define abs(x)    ((x) < 0 ? -(x) : (x))
#endif
#ifndef constrain
#define constrain(val, lo, hi) ((val) < (lo) ? (lo) : ((val) > (hi) ? (hi) : (val)))
#endif

// ─── Mock Serial ─────────────────────────────────────────────────────────────
struct _MockSerial {
    void print(const char*)    {}
    void print(int)            {}
    void print(uint8_t)        {}
    void print(uint16_t)       {}
    void print(uint32_t)       {}
    void print(float)          {}
    void print(double)         {}
    void println()             {}
    void println(const char*)  {}
    void println(int)          {}
    void println(uint8_t)      {}
    void println(uint16_t)     {}
    void println(uint32_t)     {}
    void println(float)        {}
    void println(double)       {}
    void printf(const char*, ...) {}
};
static _MockSerial Serial;
