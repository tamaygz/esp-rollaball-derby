#pragma once
#include <Arduino.h>

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
    if (outSize < 13) return; // 12 hex chars + null terminator
    uint64_t id = ESP.getEfuseMac();
    snprintf(out, outSize, "%04X%08X",
             static_cast<uint16_t>((id >> 32) & 0xFFFF),
             static_cast<uint32_t>(id & 0xFFFFFFFF));
#else
    snprintf(out, outSize, "00000000");
#endif
}
