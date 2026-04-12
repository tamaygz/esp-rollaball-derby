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

---

## Animation Engine (Phase 2)

The animation engine provides non-blocking LED effects with smooth transitions, game event integration, and FPS control.

### Features

- **6 Core Effects**: Solid, Blink, Pulse, Rainbow, Chase, Sparkle
- **Non-Blocking**: Effects run in background without blocking main loop
- **FPS Control**: Configurable 15-60 FPS with automatic frame limiting
- **Smooth Transitions**: Crossfade between effects with configurable duration
- **Game Integration**: Pre-configured effects for game events (scoring, countdown, winner)
- **Performance**: 30+ FPS sustained with 50 LEDs on ESP8266

### Quick Start - Animations

```cpp
#include <leds/LedController.h>
#include <leds/AnimationManager.h>
#include <leds/effects/RainbowEffect.h>

LedController leds;
AnimationManager animator(&leds);
RainbowEffect rainbow(&leds);

void setup() {
    leds.begin(50, 4);
    animator.begin();
    animator.setTargetFPS(30);
    
    // Start rainbow effect
    EffectParams params;
    params.brightness = 200;
    rainbow.setParams(params);
    rainbow.setCycleSpeed(3000);  // 3-second cycle
    animator.playEffect(&rainbow);
}

void loop() {
    animator.loop();  // Update animation
    leds.loop();      // WiFi yield
}
```

### AnimationManager API

#### Initialization

```cpp
AnimationManager(LedController* controller)
void begin()
```

Create and initialize the animation manager.

#### Effect Control

```cpp
void playEffect(LedEffect* effect)
```

Play an effect immediately (replaces current effect).

```cpp
void transitionTo(LedEffect* effect, uint16_t durationMs = 500)
```

Smoothly transition to new effect with crossfade.

- **durationMs**: Transition duration (0 = immediate)

```cpp
void stop()
bool isPlaying() const
LedEffect* getCurrentEffect() const
```

Stop current effect or check playback status.

#### Performance Tuning

```cpp
void setTargetFPS(uint8_t fps)
```

Set target frame rate (15-60 FPS). Higher FPS = smoother animation but more CPU usage.

**Recommended**:
- ESP8266: 30 FPS
- ESP32: 60 FPS

```cpp
AnimationStats getStats() const
void resetStats()
```

Get performance diagnostics (frame count, dropped frames, average frame time).

### Effect Classes

All effects inherit from `LedEffect` base class and support common parameters:

```cpp
struct EffectParams {
  RgbColor color;              // Primary color
  uint16_t color2Hue;          // Secondary color (HSV hue)
  float speed;                 // Speed multiplier (0.1-10.0)
  uint8_t brightness;          // Brightness (0-255)
  EffectDirection direction;   // FORWARD or REVERSE
  uint16_t durationMs;         // Effect duration (0 = infinite)
};
```

#### SolidEffect

Single static color across all LEDs.

```cpp
SolidEffect solid(&leds);
EffectParams params;
params.color = RgbColor(255, 0, 0);  // Red
params.brightness = 150;
solid.setParams(params);
solid.begin();
```

#### BlinkEffect

Alternates between color and off with configurable frequency.

```cpp
BlinkEffect blink(&leds);
EffectParams params;
params.color = RgbColor(0, 0, 255);  // Blue
blink.setParams(params);
blink.setBlinkParams(200, 200, 3);  // 200ms on, 200ms off, 3 blinks
blink.begin();
```

**Parameters**:
- `onDurationMs`: Duration LEDs are on
- `offDurationMs`: Duration LEDs are off
- `blinkCount`: Number of blinks (0 = infinite)

#### PulseEffect

Smooth sinusoidal brightness variation (breathing effect).

```cpp
PulseEffect pulse(&leds);
EffectParams params;
params.color = RgbColor(0, 255, 0);  // Green
params.brightness = 200;
pulse.setParams(params);
pulse.setPeriod(2000);  // 2-second pulse cycle
pulse.begin();
```

**Parameters**:
- `periodMs`: Duration of one complete pulse cycle

#### RainbowEffect

Displays full HSV spectrum across LED strip with rotation.

```cpp
RainbowEffect rainbow(&leds);
EffectParams params;
params.brightness = 150;
params.direction = DIRECTION_FORWARD;
rainbow.setParams(params);
rainbow.setCycleSpeed(3000);  // 3-second rainbow cycle
rainbow.begin();
```

**Parameters**:
- `cycleSpeedMs`: Time for one complete rainbow rotation

#### ChaseEffect

Moving light pattern with fading tail (theater marquee style).

```cpp
ChaseEffect chase(&leds);
EffectParams params;
params.color = RgbColor(255, 128, 0);  // Orange
params.brightness = 200;
params.direction = DIRECTION_FORWARD;
chase.setParams(params);
chase.setChaseParams(5, 20);  // 5-pixel tail, 20 pixels/sec
chase.begin();
```

**Parameters**:
- `tailLength`: Number of trailing pixels (1-50)
- `speedPixelsPerSec`: Movement speed (1-100)

#### SparkleEffect

Random LED twinkles (starfield style).

```cpp
SparkleEffect sparkle(&leds);
EffectParams params;
params.brightness = 200;
sparkle.setParams(params);
sparkle.setSparkleParams(
  RgbColor(0, 0, 50),       // Dark blue background
  RgbColor(255, 255, 255),  // White sparkles
  0.1,                      // 10% sparkle density
  10                        // Fade speed
);
sparkle.begin();
```

**Parameters**:
- `baseColor`: Background color
- `sparkleColor`: Color when sparkling
- `density`: Sparkle probability per frame (0.0-1.0)
- `fadeSpeed`: Brightness reduction per frame (1-50)

### Game Event Integration

`GameEventMapper` provides pre-configured effects for game events per PRD specifications.

```cpp
#include <leds/GameEventMapper.h>

LedController leds;
AnimationManager animator(&leds);
GameEventMapper mapper(&leds, &animator);

void setup() {
    leds.begin(50, 4);
    animator.begin();
    mapper.begin();
}

void onGameEvent(GameEventType event) {
    mapper.onEvent(event);  // Trigger appropriate LED effect
}
```

**Event Mappings**:

| Event | Effect | Duration | Description |
|-------|--------|----------|-------------|
| `COUNTDOWN_TICK` | Blue Blink | 600ms on, 400ms off | Countdown heartbeat |
| `SCORE_PLUS1` | Blue Blink | 200ms | Quick confirmation |
| `SCORE_PLUS2` | Purple Blink | 200ms × 2 | Double flash |
| `SCORE_PLUS3` | Gold Sparkle | 300ms | Celebration sparkles |
| `ZERO_ROLL` | Red Pulse | 500ms | Miss indication |
| `WINNER_SELF` | Rainbow | 7 seconds | Victory celebration |
| `WINNER_OTHER` | Red Fade | 2 seconds | Gentle defeat |

### Creating Custom Effects

Extend `LedEffect` base class to create custom animations:

```cpp
#include <leds/LedEffect.h>

class MyCustomEffect : public LedEffect {
public:
  MyCustomEffect(LedController* controller) 
    : LedEffect(controller) {
    _type = EFFECT_SOLID;  // Or define new EffectType
  }
  
  void begin() override {
    _startTime = millis();
    _elapsedTime = 0;
    // Initialize effect state
  }
  
  void update(uint32_t deltaMs) override {
    updateElapsedTime(deltaMs);
    
    // Calculate animation state
    // ...
    
    // Update LEDs
    for (uint16_t i = 0; i < _controller->getLedCount(); i++) {
      _controller->setPixel(i, RgbColor(...));
    }
    _controller->show();
  }
  
  bool isComplete() const override {
    return isDurationExceeded();  // Or custom completion logic
  }
  
  const char* getName() const override {
    return "MyCustomEffect";
  }
};
```

**Requirements**:
- Override `begin()`, `update()`, `isComplete()`, `getName()`
- Call `updateElapsedTime()` in `update()` to track time
- Complete within frame budget (16ms @ 60 FPS)
- No blocking operations (no `delay()`)

## Performance Tuning

### Frame Rate Optimization

Monitor performance with `AnimationStats`:

```cpp
AnimationStats stats = animator.getStats();
Serial.print("FPS: ");
Serial.println(stats.currentFPS);
Serial.print("Dropped frames: ");
Serial.println(stats.droppedFrames);
Serial.print("Avg frame time: ");
Serial.print(stats.avgFrameTimeUs / 1000);
Serial.println("ms");
```

**Warning Signs**:
- `droppedFrames` increasing → Effect `update()` too slow
- `currentFPS < targetFPS` → CPU overloaded
- `avgFrameTimeUs > frame budget` → Optimize effect calculations

### Memory Usage

**Per Effect** (approximate):
- SolidEffect: ~50 bytes
- BlinkEffect: ~80 bytes
- PulseEffect: ~350 bytes (sine table)
- RainbowEffect: ~100 bytes
- ChaseEffect: ~100 bytes
- SparkleEffect: ~100 bytes + `ledCount` bytes

**System Overhead**:
- AnimationManager: ~200 bytes
- LedController: ~100 bytes + `ledCount * 3` bytes (RGB buffer)

**Total for 50 LEDs**: ~2KB (ESP8266 has ~40KB free after WiFi stack)

## Platform Differences

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
- `LedController.h` — Main LED strip API (sensor and motor strip effects)
- `LedController.cpp` — Implementation
- `MatrixDisplay.h` — WS2812B matrix renderer (ESP32 motor client)
- `MatrixDisplay.cpp` — Non-blocking matrix animations

---

## MatrixDisplay (ESP32 Motor Client)

`MatrixDisplay` drives the WS2812B LED matrix on the ESP32 motor node. It is
configured via `LedConfig` received from the server and animated by calling
`loop()` every iteration of `main()`.

### Effects

Available effects are defined in `clients/shared/led-effects-manifest.json` —
the single source of truth consumed by the server (validation), admin UI
(dropdown), JS simulator (preview), and C++ firmware (dispatch).

#### Strip effects (platforms: sensor, motor)

| Name | Description | Params |
|------|-------------|--------|
| `solid` | Static fill colour | `color`, `brightness` |
| `blink` | On/off toggle | `color`, `speed` |
| `pulse` | Breathing brightness sine | `color`, `speed` |
| `rainbow` | Shifting HSV spectrum | `speed` |
| `chase` | Single pixel sweep | `color`, `speed` |
| `sparkle` | Random bright pixels | `color`, `speed` |

#### Matrix effects (platform: motor only)

| Name | Description | Params |
|------|-------------|--------|
| `countdown` | Displays 3 → 2 → 1 → GO | — |
| `text` | Scrolling text banner (default: `DERBY`) | `color`, `speed` |
| `winner` | Scrolling gold winner banner (loops) | — |
| `ballroll` | Ball rolls to random hole, ring celebration, loops | `color`, `speed` |
| `clear` | All LEDs off (one-shot) | — |

### Key API

```cpp
// Initialise from server-supplied LedConfig
bool begin(const LedConfig& cfg);

// Must be called every loop() — advances all non-blocking animations
void loop();

// Strip effects (solid, blink, pulse, chase, sparkle, rainbow)
void showEffect(const char* name, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs);

// Matrix-specific effects
void showCountdown(int n);           // n>0 = white digit, n==0 = green GO
void showText(const char* text, uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 80);
void showWinner(const char* name);   // gold scroll, loops
void showBallRoll(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 2000); // loops
void clear();
void showIdle();                     // ambient rainbow wave
```

### BallRoll animation

The `ballroll` effect runs four phases continuously:

1. **ROLLING** — ball travels from top-centre to a randomly chosen hole (ease-in, `speedMs`).
2. **DROPPING** — three quick flashes at the hole (~360 ms).
3. **CELEBRATING** — expanding ring from hole position, fades as it grows (~600 ms).
4. **PAUSE** — all off for 400 ms, then restarts with a different hole.



## Examples

See `examples/led-basic/` for more usage examples.

## License

Part of Roll-a-Ball Derby project. See root LICENSE file.

## Credits

Built on [NeoPixelBus library](https://github.com/Makuna/NeoPixelBus) by Makuna.
