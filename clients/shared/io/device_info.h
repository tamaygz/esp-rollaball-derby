#pragma once
#include <Arduino.h>

// Canonical chip-identity helpers shared across all Derby firmware targets.
//
// Max chip ID string length including null terminator.
// ESP8266: 8 hex chars + null (9), ESP32: 16 hex chars + null (17).
constexpr size_t DERBY_CHIP_ID_HEX_MAX_LEN    = 17;  // 16-char hex + null (ESP32 canonical size)
constexpr size_t DERBY_CHIP_ID_ESP8266_HEX_LEN = 9;  //  8-char hex + null (ESP8266 chip ID size)

inline const char* derbyChipType() {
#if defined(ESP8266)
    return "ESP8266";
#elif defined(ESP32)
    return "ESP32";
#else
    return "UNKNOWN";
#endif
}

inline uint16_t derbyChipSuffix16() {
#if defined(ESP8266)
    return static_cast<uint16_t>(ESP.getChipId() & 0xFFFF);
#elif defined(ESP32)
    return static_cast<uint16_t>(ESP.getEfuseMac() & 0xFFFF);
#else
    return 0;
#endif
}

// Write the chip ID as an uppercase hex string into `out`.
// ESP8266 produces an 8-char hex string; ESP32 produces a 16-char hex string.
// Use DERBY_CHIP_ID_HEX_MAX_LEN as the buffer size to handle either platform safely.
inline void derbyChipIdHex(char* out, size_t outSize) {
    if (!out || outSize == 0) return;
#if defined(ESP8266)
    if (outSize < DERBY_CHIP_ID_ESP8266_HEX_LEN) return;  // 8 hex chars + null
    snprintf(out, outSize, "%08X", ESP.getChipId());
#elif defined(ESP32)
    if (outSize < DERBY_CHIP_ID_HEX_MAX_LEN) return;      // 16 hex chars + null
    uint64_t id = ESP.getEfuseMac();
    snprintf(out, outSize, "%016llX", static_cast<unsigned long long>(id));
#else
    if (outSize < DERBY_CHIP_ID_ESP8266_HEX_LEN) return;  // 8 hex chars + null
    snprintf(out, outSize, "00000000");
#endif
}
