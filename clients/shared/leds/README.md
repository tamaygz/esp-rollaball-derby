# LED Control Layer — Shared Library

Unified WS2812B addressable LED control for ESP8266 and ESP32 platforms.

## Overview

This shared library provides a platform-abstracted API for controlling WS2812B (NeoPixel) RGB LEDs on both ESP8266 and ESP32 microcontrollers. It uses hardware acceleration (DMA on ESP8266, RMT on ESP32) for flicker-free operation with WiFi active.

## Features

- **Platform Detection**: Automatic compile-time detection of ESP8266 vs ESP32
- **Hardware Acceleration**: DMA (ESP8266) or RMT (ESP32) for precise timing
- **Color Spaces**: HSV and RGB support with automatic conversion
- **Memory Safe**: Bounds checking and validation on all operations
- **WiFi Friendly**: Non-blocking with automatic yield points
- **Performance**: 30+ FPS sustained animation rates

## Hardware Requirements

- **LEDs**: WS2812B addressable RGB LEDs (800kHz protocol)
- **Power**: External 5V supply (60mA per LED at full brightness)
- **Data Pin**:
  - ESP8266: GPIO3 (RX pin) for DMA method
  - ESP32: Any valid GPIO (RMT method)
- **LED Count Limits**:
  - ESP8266: 1-300 LEDs (256KB RAM)
  - ESP32: 1-1000 LEDs (520KB RAM)

## Installation

### PlatformIO

Add to your `platformio.ini`:

```ini
[env]
build_flags = -I../../shared
lib_deps = makuna/NeoPixelBus@^2.7.0
```

## Quick Start

```cpp
#include <leds/LedController.h>

LedController leds;

void setup() {
    Serial.begin(115200);
    
    // Initialize 50 LEDs on GPIO4
    if (!leds.begin(50, 4)) {
        Serial.println("LED initialization failed!");
        return;
    }
    
    leds.setBrightness(128);  // 50% brightness
}

void loop() {
    // Rainbow animation
    static uint16_t hue = 0;
    for (uint16_t i = 0; i < leds.getLedCount(); i++) {
        leds.setPixel(i, HsvColor((hue + i * 5) % 360, 255, 255));
    }
    leds.show();
    
    hue = (hue + 1) % 360;
    leds.loop();  // WiFi yield point
    delay(16);    // ~60 FPS
}
```

## API Reference

### LedController Class

#### Initialization

```cpp
bool begin(uint16_t ledCount, uint8_t pin)
```

Initialize the LED strip.

- **ledCount**: Number of LEDs (1-300 for ESP8266, 1-1000 for ESP32)
- **pin**: GPIO pin for data output
- **Returns**: `true` if successful, `false` if validation failed

#### Color Control

```cpp
void setPixel(uint16_t index, RgbColor color)
void setPixel(uint16_t index, HsvColor color)
```

Set a single pixel color. HSV is automatically converted to RGB.

- **index**: Pixel index (0-based)
- **color**: RGB (0-255 per channel) or HSV (H: 0-360, S: 0-255, V: 0-255)

```cpp
void setBrightness(uint8_t brightness)
uint8_t getBrightness() const
```

Global brightness control (0-255, where 255 = 100%).

```cpp
void clear()
```

Turn off all LEDs (set to black).

#### Display Updates

```cpp
void show()
```

Push LED buffer to hardware. Call after `setPixel()` to make changes visible.

```cpp
bool canShow() const
```

Check if ready for next `show()` call (WS2812B requires 50µs reset time).

```cpp
void loop()
```

Non-blocking WiFi yield. Call every iteration to maintain WiFi stability.

#### Status

```cpp
uint16_t getLedCount() const
```

Get the number of configured LEDs.

## Platform Differences

### ESP8266

- **Method**: DMA on GPIO3 (RX pin)
- **Max LEDs**: 300 (RAM constraint)
- **Note**: GPIO3 conflicts with Serial debugging when using DMA
- **Alternative**: UART method for configurable pin (future enhancement)

### ESP32

- **Method**: RMT channel 0
- **Max LEDs**: 1000 (RAM constraint)
- **Note**: Can use any valid GPIO pin
- **Advantage**: 8 parallel RMT channels available for multiple LED strips

## Power Requirements

| LED Count | Current @ 100% | Recommended PSU |
|-----------|----------------|-----------------|
| 10        | 0.6 A          | 1 A             |
| 50        | 3.0 A          | 5 A             |
| 100       | 6.0 A          | 10 A            |
| 300       | 18.0 A         | 20 A            |

Calculate: `current = led_count × 60mA × (brightness / 255)`

**Important**: Always use external 5V power supply. Do NOT power more than 1-2 LEDs from ESP pins.

## Color Spaces

### RGB

```cpp
RgbColor color(255, 0, 0);  // Red
leds.setPixel(0, color);
```

### HSV (Recommended for Animations)

```cpp
HsvColor color(0, 255, 255);    // Red (Hue=0°)
HsvColor color(120, 255, 255);  // Green (Hue=120°)
HsvColor color(240, 255, 255);  // Blue (Hue=240°)
leds.setPixel(0, color);
```

HSV is ideal for smooth color transitions and rainbow effects.

## Performance Tips

1. **Batch Updates**: Call `show()` once per frame, not after each `setPixel()`
2. **WiFi Stability**: Call `leds.loop()` every iteration for yield points
3. **Frame Rate**: Target 30-60 FPS (16-33ms per frame)
4. **Brightness**: Lower brightness saves power and improves performance

## Troubleshooting

### LEDs Don't Light Up

- Verify external 5V power supply connected
- Check GPIO pin number (ESP8266: must use GPIO3 for DMA)
- Confirm WS2812B wiring: Data → ESP GPIO, GND → common ground
- Add 300-500Ω resistor in data line if experiencing flicker

### WiFi Disconnects

- Call `leds.loop()` every iteration
- Reduce frame rate if needed (add `delay()` in main loop)
- Verify `show()` is not called more than 60 times per second

### Flickering

- Use hardware acceleration (DMA/RMT) — never bit-bang
- Ensure stable power supply (voltage drop causes color errors)
- Check for electrical noise on data line (short wires < 1m recommended)

### Compilation Errors

```
error: #error "Unsupported platform..."
```

Ensure PlatformIO correctly detects ESP8266 or ESP32 board in `platformio.ini`.

## Files

- `LedPlatform.h` — Platform detection and NeoPixelBus method selection
- `LedController.h` — Main API declaration
- `LedController.cpp` — Implementation

## Examples

See `examples/led-basic/` for more usage examples.

## License

Part of Roll-a-Ball Derby project. See root LICENSE file.

## Credits

Built on [NeoPixelBus library](https://github.com/Makuna/NeoPixelBus) by Makuna.
