#pragma once
#include <Arduino.h>

// Max chip ID string length including null terminator.
// ESP8266: 8 hex chars + null (9), ESP32: 16 hex chars + null (17).
constexpr size_t DERBY_CHIP_ID_HEX_MAX_LEN = 17;

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

inline void derbyChipIdHex(char* out, size_t outSize) {
    if (!out || outSize == 0) return;
#if defined(ESP8266)
    if (outSize < 9) return;  // 8 hex chars + null terminator
    snprintf(out, outSize, "%08X", ESP.getChipId());
#elif defined(ESP32)
    if (outSize < DERBY_CHIP_ID_HEX_MAX_LEN) return; // 16 hex chars + null terminator
    uint64_t id = ESP.getEfuseMac();
    snprintf(out, outSize, "%016llX", static_cast<unsigned long long>(id));
#else
    snprintf(out, outSize, "00000000");
#endif
}
