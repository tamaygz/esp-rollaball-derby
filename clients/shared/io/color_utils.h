#pragma once
#include <Arduino.h>

// ─── Hex Colour Parsing ───────────────────────────────────────────────────────
// Parses a 7-character "#RRGGBB" hex colour string into r, g, b byte components.
// Writes r/g/b only on success; returns true if the string is valid.
// Shared by sensor and motor firmware to eliminate duplicate parsing code.
inline bool derbyParseHexColor(const char* str, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (!str || str[0] != '#' || strlen(str) != 7) return false;
    char hex[3] = {0};  // 2 hex chars + null terminator
    hex[0] = str[1]; hex[1] = str[2]; r = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    hex[0] = str[3]; hex[1] = str[4]; g = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    hex[0] = str[5]; hex[1] = str[6]; b = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    return true;
}
