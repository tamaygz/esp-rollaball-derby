#pragma once
#include <stdint.h>

// 5-row × 3-column pixel font for digits 0–9 and selected uppercase letters.
// Each character is stored as 5 bytes; each byte encodes a 3-bit row (bits 2–0).
// Bit 2 = leftmost column, Bit 0 = rightmost column.
// Example:  0b110 = XX_  (two pixels left, one clear right)

static const uint8_t FONT5x3_CHARS = 36;  // digits 0–9 + A–Z

static const uint8_t FONT5x3[][5] = {
    // 0
    { 0b111, 0b101, 0b101, 0b101, 0b111 },
    // 1
    { 0b010, 0b110, 0b010, 0b010, 0b111 },
    // 2
    { 0b111, 0b001, 0b111, 0b100, 0b111 },
    // 3
    { 0b111, 0b001, 0b111, 0b001, 0b111 },
    // 4
    { 0b101, 0b101, 0b111, 0b001, 0b001 },
    // 5
    { 0b111, 0b100, 0b111, 0b001, 0b111 },
    // 6
    { 0b111, 0b100, 0b111, 0b101, 0b111 },
    // 7
    { 0b111, 0b001, 0b011, 0b010, 0b010 },
    // 8
    { 0b111, 0b101, 0b111, 0b101, 0b111 },
    // 9
    { 0b111, 0b101, 0b111, 0b001, 0b111 },
    // A (index 10)
    { 0b010, 0b101, 0b111, 0b101, 0b101 },
    // B
    { 0b110, 0b101, 0b110, 0b101, 0b110 },
    // C
    { 0b111, 0b100, 0b100, 0b100, 0b111 },
    // D
    { 0b110, 0b101, 0b101, 0b101, 0b110 },
    // E
    { 0b111, 0b100, 0b110, 0b100, 0b111 },
    // F
    { 0b111, 0b100, 0b110, 0b100, 0b100 },
    // G
    { 0b111, 0b100, 0b111, 0b101, 0b111 },
    // H
    { 0b101, 0b101, 0b111, 0b101, 0b101 },
    // I
    { 0b111, 0b010, 0b010, 0b010, 0b111 },
    // J  __X __X __X _XX _X_
    { 0b001, 0b001, 0b001, 0b011, 0b010 },
    // K
    { 0b101, 0b110, 0b100, 0b110, 0b101 },
    // L
    { 0b100, 0b100, 0b100, 0b100, 0b111 },
    // M
    { 0b101, 0b111, 0b101, 0b101, 0b101 },
    // N  X_X XX_ X_X _XX X_X  (diagonal stroke)
    { 0b101, 0b110, 0b101, 0b011, 0b101 },
    // O
    { 0b111, 0b101, 0b101, 0b101, 0b111 },
    // P
    { 0b111, 0b101, 0b111, 0b100, 0b100 },
    // Q
    { 0b111, 0b101, 0b101, 0b011, 0b111 },
    // R  XXX X_X XX_ X_X X_X
    { 0b111, 0b101, 0b110, 0b101, 0b101 },
    // S
    { 0b111, 0b100, 0b111, 0b001, 0b111 },
    // T
    { 0b111, 0b010, 0b010, 0b010, 0b010 },
    // U
    { 0b101, 0b101, 0b101, 0b101, 0b111 },
    // V
    { 0b101, 0b101, 0b101, 0b101, 0b010 },
    // W
    { 0b101, 0b101, 0b111, 0b111, 0b101 },
    // X  X_X X_X _X_ X_X X_X
    { 0b101, 0b101, 0b010, 0b101, 0b101 },
    // Y  X_X X_X _X_ _X_ _X_
    { 0b101, 0b101, 0b010, 0b010, 0b010 },
    // Z
    { 0b111, 0b001, 0b010, 0b100, 0b111 },
};

// Map a character to its FONT5x3 index, returns -1 for unsupported characters.
inline int font5x3Index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    return -1;
}
