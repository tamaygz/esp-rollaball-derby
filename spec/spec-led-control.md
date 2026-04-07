---
title: LED Control Layer Technical Specification
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: Roll-a-Ball Derby Engineering Team
tags: [architecture, embedded, hardware, led, esp8266, esp32]
---

# LED Control Layer Technical Specification

A comprehensive technical specification for implementing a shared LED control abstraction layer for ESP8266 and ESP32 clients in the Roll-a-Ball Derby system, enabling arcade-style visual effects synchronized with game events.

## 1. Purpose & Scope

This specification defines the architecture, interfaces, and implementation details for a unified LED control system that:

- Provides a hardware-abstracted C++ API for controlling WS2812B addressable LEDs
- Supports multiple physical topologies (strips, matrices, rings)
- Enables server-driven configuration and synchronization
- Works seamlessly across ESP8266 and ESP32 platforms with automatic optimization
- Delivers sub-50ms latency from game events to LED response
- Maintains 30+ FPS animation performance while preserving WiFi connectivity

**Intended Audience**: Embedded firmware developers, backend developers, hardware integrators

**Assumptions**:
- All clients use PlatformIO with Arduino framework
- Server is Node.js with Express and `ws` library
- Web clients use vanilla JavaScript (no build tooling)
- WS2812B LEDs are powered externally (5V supply), data pin connected to ESP GPIO
- Network operates on local LAN with <50ms typical latency

## 2. Definitions

| Term | Definition |
|------|------------|
| **WS2812B** | Addressable RGB LED chipset using 800kHz single-wire protocol |
| **DMA** | Direct Memory Access - hardware method for bit-banging LED data without CPU intervention (ESP8266) |
| **RMT** | Remote Control Transceiver - ESP32 hardware peripheral for precise timing |
| **UART** | Universal Asynchronous Receiver/Transmitter - alternative LED control method using serial TX pin |
| **HSV** | Hue, Saturation, Value - cylindrical color space, easier for animations than RGB |
| **Topology** | Physical arrangement of LEDs: strip (linear), matrix (2D grid), ring (circular) |
| **Keyframe** | Animation frame defining color/brightness at specific time offset |
| **Effect** | Parameterized LED animation pattern (pulse, chase, rainbow, etc.) |
| **Yield Point** | Code location where ESP8266 allows WiFi stack to execute (prevents watchdog reset) |

## 3. Requirements, Constraints & Guidelines

### Core Requirements

- **REQ-001**: Support WS2812B LEDs (800kHz protocol) exclusively
- **REQ-002**: Maximum 300 LEDs per ESP8266 device, 1000 LEDs per ESP32 device
- **REQ-003**: Animation loop must yield to WiFi stack every 50ms maximum
- **REQ-004**: LED update latency from WebSocket message receipt to first LED change: <50ms (p95)
- **REQ-005**: Maintain minimum 30 FPS animation rate under WiFi load
- **REQ-006**: Memory footprint: LED subsystem <40KB RAM on ESP8266, <80KB on ESP32
- **REQ-007**: Support strip, matrix (8x8, 16x16), and ring topologies via unified API
- **REQ-008**: Automatic platform detection (ESP8266 vs ESP32) at compile time
- **REQ-009**: Non-blocking operation - no `delay()` calls in LED subsystem
- **REQ-010**: Graceful degradation if server configuration unavailable (use defaults)

### Security Requirements

- **SEC-001**: Validate all LED count parameters (1-300 for ESP8266, 1-1000 for ESP32)
- **SEC-002**: Sanitize custom effect JSON uploads (max 10KB, max 100 keyframes)
- **SEC-003**: Rate limit LED effect WebSocket messages (max 20/second per client)
- **SEC-004**: Reject GPIO pins that conflict with critical functions (GPIO0, GPIO2, GPIO15)

### Constraints

- **CON-001**: ESP8266 has 80KB user RAM; LED buffer dominates memory budget (300 LEDs × 3 bytes = 900 bytes minimum)
- **CON-002**: ESP32 has 520KB RAM; can support larger buffers and double-buffering
- **CON-003**: WS2812B timing requires <1% jitter; WiFi interrupts on ESP8266 cause glitches without DMA/UART
- **CON-004**: GPIO3 (RX) is optimal for DMA on ESP8266 but prevents Serial debugging
- **CON-005**: NeoPixelBus library required for hardware-accelerated LED control
- **CON-006**: Matrix zigzag wiring pattern varies by manufacturer (must support both)
- **CON-007**: Animation effects must be pre-defined or validated server-side (no code upload to firmware)
- **CON-008**: ESP8266 single-core CPU must timeslice: WiFi, LED animation, sensor sampling

### Guidelines

- **GUD-001**: Prefer HSV for animations (smoother color transitions), convert to RGB at output
- **GUD-002**: Use gamma correction table for perceptually linear brightness
- **GUD-003**: Implement effect base class with virtual methods for extensibility
- **GUD-004**: Pre-compute animation frames where possible to reduce runtime CPU load
- **GUD-005**: Document power requirements in admin UI (60mA per LED at full white)
- **GUD-006**: Use platform-specific conditional compilation blocks clearly marked
- **GUD-007**: Log all LED configuration changes to serial console for debugging

### Patterns

- **PAT-001**: Factory pattern for creating platform-specific `NeoPixelBus<>` instances
- **PAT-002**: Strategy pattern for topology mapping (strip, matrix, ring coordinate systems)
- **PAT-003**: Observer pattern for game event → LED effect triggering
- **PAT-004**: State pattern for LED lifecycle (initializing, connected, animating, error)

## 4. Interfaces & Data Contracts

### 4.1 C++ API - LedController Class

```cpp
// Platform-abstracted LED controller (clients/shared/leds/LedController.h)
// Supports ESP8266 (DMA/UART) and ESP32 (RMT) automatically

class LedController {
public:
    // Initialize with pin, LED count, topology. Automatically selects optimal
    // hardware method based on platform and pin.
    // Throws runtime error if pin invalid or LED count exceeds platform limits.
    void begin(uint8_t pin, uint16_t ledCount, LedTopology topology);

    // Set single LED by logical index (topology-independent)
    void setPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void setPixelHSV(uint16_t index, uint16_t hue, uint8_t sat, uint8_t val);

    // Set all LEDs to same color
    void fill(uint8_t r, uint8_t g, uint8_t b);
    void fillHSV(uint16_t hue, uint8_t sat, uint8_t val);

    // Clear all LEDs (set to black)
    void clear();

    // Commit changes to LEDs (non-blocking, uses DMA/RMT)
    void show();

    // Set global brightness 0-255
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const;

    // Get platform capabilities
    uint16_t getMaxLeds() const;      // 300 (ESP8266) or 1000 (ESP32)
    const char* getChipType() const;  // "ESP8266" or "ESP32"
    const char* getMethod() const;    // "DMA", "UART", or "RMT"

    // Topology coordinate mapping
    bool setPixelXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
    bool setPixelPolar(uint16_t angle, uint8_t r, uint8_t g, uint8_t b);

private:
    // Platform-specific implementation hidden by abstraction
    void* _busPtr;           // Pointer to NeoPixelBus<> instance
    uint16_t _ledCount;
    uint8_t _brightness;
    LedTopology _topology;

    // Coordinate mapping helpers
    uint16_t _xyToIndex(uint8_t x, uint8_t y);
    uint16_t _polarToIndex(uint16_t angle);
};

// Topology configuration
enum class LedTopologyType {
    STRIP,         // Linear arrangement
    MATRIX_ZIGZAG, // 2D grid with alternating row direction
    MATRIX_PROG,   // 2D grid with consistent row direction
    RING           // Circular arrangement
};

struct LedTopology {
    LedTopologyType type;
    uint16_t width;   // For matrix: columns; for ring: diameter (unused for strip)
    uint16_t height;  // For matrix: rows; others: 1
    uint16_t firstLedAngle; // For ring: rotation offset (0-359 degrees)
};
```

### 4.2 C++ API - LedEffect Base Class

```cpp
// Base class for all LED animation effects (clients/shared/leds/LedEffect.h)

class LedEffect {
public:
    LedEffect(LedController& controller) : _controller(controller) {}
    virtual ~LedEffect() = default;

    // Called once when effect starts. Initialize state here.
    virtual void begin() {}

    // Called every frame. Return false when effect complete (for one-shot effects).
    // dt = milliseconds since last update.
    virtual bool update(unsigned long dt) = 0;

    // Called when effect interrupted. Clean up resources.
    virtual void end() {}

    // Set effect parameters (speed, color, etc.)
    virtual void setParam(const char* key, float value) {}

protected:
    LedController& _controller;
    unsigned long _elapsedTime = 0;
};

// Derived effect classes (examples)
class SolidColorEffect : public LedEffect {
public:
    SolidColorEffect(LedController& controller, uint8_t r, uint8_t g, uint8_t b);
    bool update(unsigned long dt) override;
private:
    uint8_t _r, _g, _b;
};

class PulseEffect : public LedEffect {
public:
    PulseEffect(LedController& controller, uint16_t hue, uint16_t periodMs);
    bool update(unsigned long dt) override;
private:
    uint16_t _hue, _periodMs;
    float _phase;
};

class RainbowEffect : public LedEffect {
public:
    RainbowEffect(LedController& controller, uint16_t speedDegPerSec);
    bool update(unsigned long dt) override;
private:
    uint16_t _speedDegPerSec;
    uint16_t _hueOffset;
};

// ... additional effects: BlinkEffect, ChaseEffect, SparkleEffect, FireEffect, WaveEffect
```

### 4.3 C++ API - LedAnimator Class

```cpp
// Frame-based animation engine with effect transitions
// (clients/shared/leds/LedAnimator.h)

class LedAnimator {
public:
    LedAnimator(LedController& controller);

    // Start effect immediately, replacing current effect
    void playEffect(LedEffect* effect, bool loop = false);

    // Crossfade to new effect over durationMs
    void transitionTo(LedEffect* effect, unsigned long durationMs, bool loop = false);

    // Stop current effect, clear LEDs
    void stop();

    // Call every loop(). Returns false if effect complete and not looping.
    bool loop();

    // Get target frame rate (default 30 FPS)
    void setTargetFps(uint8_t fps);
    uint8_t getTargetFps() const;

private:
    LedController& _controller;
    LedEffect* _currentEffect;
    LedEffect* _nextEffect;
    bool _looping;
    bool _transitioning;
    unsigned long _transitionStartMs;
    unsigned long _transitionDurationMs;
    unsigned long _lastFrameMs;
    uint8_t _targetFps;

    void _updateTransition();
};
```

### 4.4 WebSocket Protocol Extensions

#### 4.4.1 Client Registration Extension

**Message**: `register` (client → server)

Existing payload extended with LED capabilities:

```json
{
  "type": "register",
  "payload": {
    "type": "sensor",
    "playerName": "Alice",
    "chipType": "ESP8266",
    "ledCount": 50,
    "ledCapabilities": {
      "maxLeds": 300,
      "method": "DMA",
      "pin": 3
    }
  }
}
```

**Fields**:
- `chipType`: `"ESP8266"` or `"ESP32"`
- `ledCount`: Auto-detected LED count (0 if detection failed)
- `ledCapabilities.maxLeds`: Platform-specific maximum (300 or 1000)
- `ledCapabilities.method`: `"DMA"`, `"UART"`, or `"RMT"`
- `ledCapabilities.pin`: GPIO pin connected to LED data line

#### 4.4.2 Server Configuration Push

**Message**: `led_config` (server → client, broadcast to device type)

```json
{
  "type": "led_config",
  "payload": {
    "topology": {
      "type": "STRIP",
      "width": 1,
      "height": 50,
      "firstLedAngle": 0
    },
    "expectedLedCount": 50,
    "brightness": 200,
    "effects": {
      "idle": "rainbow_slow",
      "countdown": "pulse_blue",
      "score_1": "flash_blue",
      "score_2": "flash_purple_2x",
      "score_3": "flash_gold_sparkle",
      "winner": "rainbow_explosion",
      "loser": "fade_red"
    }
  }
}
```

**Validation**:
- `expectedLedCount` ±5 tolerance against client-reported `ledCount`
- `brightness` range 0-255
- Effect names must reference built-in or uploaded custom effects

#### 4.4.3 Trigger LED Effect

**Message**: `led_effect` (server → client, targeted or broadcast)

```json
{
  "type": "led_effect",
  "payload": {
    "effect": "flash_gold_sparkle",
    "target": "player-id-123",
    "params": {
      "speed": 1.5,
      "color_hue": 45
    }
  }
}
```

**Fields**:
- `effect`: Effect name (built-in or custom)
- `target`: Optional player ID; omit for broadcast to all devices
- `params`: Key-value pairs passed to effect via `setParam()`

#### 4.4.4 Test Pattern Request

**Message**: `led_test` (server → client, targeted)

```json
{
  "type": "led_test",
  "payload": {
    "pattern": "rainbow_cycle",
    "duration_ms": 5000
  }
}
```

**Patterns**:
- `rainbow_cycle`: Full HSV spectrum across all LEDs, rotating
- `white_flash`: All LEDs white 100% brightness, 500ms
- `pixel_test`: Sequential pixel test (one LED at a time, red → green → blue)

#### 4.4.5 Emergency LED Control

**Message**: `led_emergency_stop` (server → client, broadcast)

```json
{
  "type": "led_emergency_stop",
  "payload": {}
}
```

Immediately clears all LEDs, halts animation loop. Used for safety (e.g., power supply fault).

**Message**: `led_resume` (server → client, broadcast)

```json
{
  "type": "led_resume",
  "payload": {}
}
```

Resumes animation from idle state.

### 4.5 Server REST API Specifications

#### 4.5.1 GET /api/leds/config

Retrieve LED configurations for all device types.

**Response**:
```json
{
  "sensor": {
    "topology": { "type": "STRIP", "width": 1, "height": 50, "firstLedAngle": 0 },
    "expectedLedCount": 50,
    "brightness": 200,
    "effects": { "idle": "rainbow_slow", ... }
  },
  "motor": {
    "topology": { "type": "RING", "width": 24, "height": 1, "firstLedAngle": 0 },
    "expectedLedCount": 24,
    "brightness": 255,
    "effects": { "idle": "pulse_amber", ... }
  }
}
```

**Status Codes**:
- `200 OK`: Configurations returned
- `500 Internal Server Error`: Failed to read config file

#### 4.5.2 PUT /api/leds/config

Update LED configuration for a device type. Only allowed in `idle` game state.

**Request**:
```json
{
  "deviceType": "sensor",
  "config": {
    "topology": { "type": "MATRIX_ZIGZAG", "width": 8, "height": 8, "firstLedAngle": 0 },
    "expectedLedCount": 64,
    "brightness": 180,
    "effects": { "idle": "fire", ... }
  }
}
```

**Response**:
```json
{
  "success": true,
  "validated": true,
  "connectedDevices": 4,
  "warnings": []
}
```

**Validation Rules**:
- `deviceType` must be `"sensor"` or `"motor"`
- `topology.type` must be valid `LedTopologyType`
- `expectedLedCount` must be 1-300 for ESP8266, 1-1000 for ESP32
- All effect names in `effects` map must exist in built-in or custom library
- `brightness` range 0-255

**Status Codes**:
- `200 OK`: Configuration updated and broadcast
- `400 Bad Request`: Validation failed (returns `{ success: false, error: "..." }`)
- `409 Conflict`: Game not in idle state

#### 4.5.3 GET /api/leds/effects

List all available LED effects (built-in + custom).

**Response**:
```json
{
  "builtin": [
    { "name": "solid", "params": ["r", "g", "b"] },
    { "name": "pulse", "params": ["hue", "period_ms"] },
    { "name": "rainbow", "params": ["speed_deg_per_sec"] },
    { "name": "chase", "params": ["color", "speed", "tail_length"] },
    { "name": "sparkle", "params": ["density", "fade_rate"] }
  ],
  "custom": [
    { "id": "custom-abc123", "name": "team_colors", "uploadedAt": "2026-04-07T10:30:00Z" }
  ]
}
```

#### 4.5.4 POST /api/leds/effects

Upload a custom LED effect. Effect is validated and assigned a unique ID.

**Request**:
```json
{
  "name": "blink_triple",
  "keyframes": [
    { "time_ms": 0, "fill": { "r": 255, "g": 0, "b": 0 } },
    { "time_ms": 200, "fill": { "r": 0, "g": 0, "b": 0 } },
    { "time_ms": 300, "fill": { "r": 255, "g": 0, "b": 0 } },
    { "time_ms": 500, "fill": { "r": 0, "g": 0, "b": 0 } },
    { "time_ms": 600, "fill": { "r": 255, "g": 0, "b": 0 } },
    { "time_ms": 800, "fill": { "r": 0, "g": 0, "b": 0 } }
  ],
  "loop": false
}
```

**Validation Rules**:
- `name` must be 1-32 characters, alphanumeric + underscore
- `keyframes` array max 100 entries
- Total JSON size max 10KB
- `time_ms` must be monotonically increasing
- Color values 0-255

**Response**:
```json
{
  "success": true,
  "id": "custom-def456",
  "name": "blink_triple"
}
```

**Status Codes**:
- `201 Created`: Effect uploaded successfully
- `400 Bad Request`: Validation failed

#### 4.5.5 DELETE /api/leds/effects/:id

Remove a custom effect. Built-in effects cannot be deleted.

**Response**:
```json
{
  "success": true,
  "deletedId": "custom-def456"
}
```

**Status Codes**:
- `200 OK`: Effect deleted
- `403 Forbidden`: Attempted to delete built-in effect
- `404 Not Found`: Effect ID does not exist

#### 4.5.6 POST /api/leds/effects/test

Send test effect to a specific device or all devices of a type.

**Request**:
```json
{
  "effect": "rainbow_cycle",
  "target": "player-id-123",
  "duration_ms": 5000
}
```

**Response**:
```json
{
  "success": true,
  "sentTo": ["player-id-123"]
}
```

**Status Codes**:
- `200 OK`: Test effect sent
- `404 Not Found`: Target device not connected

## 5. Acceptance Criteria

### LED Hardware Support

- **AC-001**: Given an ESP8266 client with 50 WS2812B LEDs connected to GPIO4, when firmware boots, then LEDs display boot sequence (red pulse → yellow → green)
- **AC-002**: Given an ESP32 client with 200 WS2812B LEDs connected to GPIO16, when firmware boots, then LEDs display boot sequence using RMT method
- **AC-003**: Given a matrix topology 8x8 with zigzag wiring, when `setPixelXY(3, 5, 255, 0, 0)` is called, then the correct physical LED at coordinate (3, 5) lights red
- **AC-004**: Given a ring topology with 24 LEDs and `firstLedAngle=90`, when `setPixelPolar(180, 0, 255, 0)` is called, then the LED at 270° physical position lights green

### Animation Engine

- **AC-005**: Given `RainbowEffect` running at 30 FPS target, when measured over 10 seconds, then average frame rate is 28-32 FPS
- **AC-006**: Given `PulseEffect` with 2000ms period, when effect runs for one complete cycle, then brightness smoothly varies from 0% to 100% and back to 0% over 2 seconds
- **AC-007**: Given `ChaseEffect` active with 5-LED tail, when effect updates, then exactly 5 consecutive LEDs are lit with fading trail
- **AC-008**: Given `LedAnimator.transitionTo(newEffect, 1000, false)`, when transition starts, then both old and new effects blend over 1 second before old effect ends

### Event-Driven Feedback

- **AC-009**: Given a connected sensor client, when server sends `led_effect` with `"effect": "flash_blue"`, then LEDs flash blue within 50ms of WebSocket message receipt
- **AC-010**: Given a game countdown from 3, when server broadcasts `countdown` messages, then all sensor clients show synchronized pulse effect with ±100ms variance
- **AC-011**: Given a player scores +3 roll, when server broadcasts scored event, then that player's sensor LEDs trigger `flash_gold_sparkle` effect lasting 300ms
- **AC-012**: Given a player wins, when server broadcasts winner event, then winner's sensor LEDs play `rainbow_explosion` for 7 seconds while other players show `fade_red` for 2 seconds

### Server Configuration & Sync

- **AC-013**: Given admin updates sensor LED count to 60 via `PUT /api/leds/config`, when configuration saved, then all connected sensor clients receive `led_config` message within 2 seconds
- **AC-014**: Given a sensor client reports `ledCount: 50` but server expects 64, when registration completes, then server logs warning and admin UI shows red indicator next to device
- **AC-015**: Given ESP8266 client with `maxLeds: 300`, when admin attempts to configure 400 LEDs, then PUT request returns 400 Bad Request with error message

### Memory and Performance

- **AC-016**: Given ESP8266 with 300 LEDs, when LED subsystem fully initialized, then free heap is ≥40KB
- **AC-017**: Given ESP32 with 500 LEDs, when LED subsystem fully initialized, then free heap is ≥400KB
- **AC-018**: Given animation loop running at 60 FPS, when LED subsystem executes, then WiFi stack receives yield every ≤50ms
- **AC-019**: Given `RainbowEffect` active, when effect update() executes, then CPU time consumed is <10ms per frame on ESP8266

### Platform Abstraction

- **AC-020**: Given ESP8266 with LED on GPIO3, when `LedController.begin(3, 50, stripTopology)` called, then `getMethod()` returns `"DMA"`
- **AC-021**: Given ESP8266 with LED on GPIO4, when `LedController.begin(4, 50, stripTopology)` called, then `getMethod()` returns `"UART"`
- **AC-022**: Given ESP32 with LED on GPIO16, when `LedController.begin(16, 200, stripTopology)` called, then `getMethod()` returns `"RMT"`
- **AC-023**: Given same effect code compiled for ESP8266 and ESP32, when effects run, then visual output is identical on both platforms

## 6. Test Automation Strategy

### Test Levels

- **Unit Tests**: C++ effect classes, topology mapping, HSV/RGB conversion
- **Integration Tests**: WebSocket protocol handling, server API endpoints
- **Hardware-in-Loop Tests**: Physical LED timing validation with oscilloscope
- **End-to-End Tests**: Full game scenario with LED effects synchronized

### Testing Frameworks

- **C++ Unit Tests**: PlatformIO native test framework (Unity)
- **Server Tests**: Node.js built-in test runner (`node --test`)
- **Hardware Tests**: Manual validation with documented procedures
- **Performance Tests**: Custom profiling with `micros()` timing on ESP8266/ESP32

### Test Data Management

- **Mock LED Configurations**: JSON fixtures in `tests/fixtures/led-configs/`
- **Effect Library**: Sample custom effects for validation testing
- **WebSocket Message Samples**: Valid and invalid message schemas for protocol testing

### CI/CD Integration

- **GitHub Actions**: Run unit tests on every commit (ESP8266/ESP32 emulation via Qemu)
- **Server Tests**: Automated in CI pipeline
- **Hardware Tests**: Manual gate before release (documented checklist)

### Coverage Requirements

- **C++ Code**: 70%+ line coverage for LED subsystem
- **Server Code**: 80%+ line coverage for LED API endpoints
- **Effect Library**: 100% of built-in effects tested with known-good outputs

### Performance Testing Approach

- **Frame Rate Profiling**: Log frame times over 60-second test runs, measure min/max/p50/p95/p99
- **Memory Profiling**: Track heap usage at initialization, during animation, after cleanup
- **Latency Measurement**: Oscilloscope measurement from WebSocket message arrival to first LED state change

## 7. Rationale & Context

### Architecture Decisions

**Q: Why abstract LED topology from effects?**

A: Physical LED arrangements vary widely (strips, matrices, rings). By providing a unified coordinate system (linear index, XY grid, polar angle), effect code remains reusable across all topologies. A rainbow chase works identically on a strip, matrix, or ring without modification.

**Q: Why use NeoPixelBus library instead of FastLED or Adafruit_NeoPixel?**

A: NeoPixelBus provides hardware-accelerated methods (DMA, UART, RMT) that eliminate timing glitches caused by WiFi interrupts. FastLED and Adafruit_NeoPixel use bit-banging which is incompatible with WiFi on ESP8266. NeoPixelBus also has better ESP32 support via RMT peripheral.

**Q: Why store LED configuration on server instead of in firmware?**

A: Centralizing configuration enables:
- Zero firmware reflashing to change LED counts or effects
- Synchronized configuration across multiple devices of same type
- Validation against detected hardware at runtime
- Easy reconfiguration via web interface
- Consistent state management (server is single source of truth)

**Q: Why limit custom effects to keyframe JSON instead of allowing code upload?**

A: Security and stability. Allowing arbitrary code execution on ESP8266 creates vulnerability. Keyframe-based effects are safe, validated server-side, and sufficient for 95% of use cases. Advanced users can add effects to the built-in library via firmware pull requests.

**Q: Why use HSV for animations instead of RGB?**

A: HSV color space makes animations more intuitive:
- Hue (0-360°) creates smooth rainbow cycles by incrementing one value
- Brightness fades are trivial (adjust V while keeping H and S constant)
- RGB requires three simultaneous channels for equivalent effects
- Human perception of color is closer to HSV model

### Platform Abstraction Strategy

ESP8266 and ESP32 have different capabilities:

| Feature | ESP8266 | ESP32 |
|---------|---------|-------|
| CPU Cores | 1 @ 80MHz | 2 @ 240MHz |
| User RAM | ~80KB | ~520KB |
| LED Method | DMA (GPIO3) or UART | RMT (any GPIO) |
| Max LEDs Practical | 300 | 1000+ |
| Serial Debug + LEDs | Incompatible (GPIO3) | Compatible |

The abstraction layer uses compile-time detection (`#ifdef ESP8266` / `#ifdef ESP32`) to select optimal hardware methods while maintaining identical API. This allows mixed deployments where some devices are ESP8266 and others ESP32, all running from the same server configuration.

## 8. Dependencies & External Integrations

### External Systems

- **EXT-001**: Node.js server WebSocket broadcasting system - Required for LED effect synchronization across devices

### Third-Party Services

- **SVC-001**: None - System operates entirely on local network

### Infrastructure Dependencies

- **INF-001**: 5V DC power supply capable of delivering 60mA per LED at full brightness (e.g., 300 LEDs = 18A minimum)
- **INF-002**: WiFi network with <50ms typical latency, support for 20+ concurrent WebSocket connections
- **INF-003**: ESP8266/ESP32 GPIO pins capable of 3.3V output (WS2812B tolerant), or level shifter to 5V

### Data Dependencies

- **DAT-001**: Server configuration file `server/data/led-config.json` - Persistent storage of per-device-type LED configurations
- **DAT-002**: Custom effect library `server/data/led-effects/*.json` - User-uploaded animation patterns

### Technology Platform Dependencies

- **PLT-001**: PlatformIO with `espressif8266` platform v4.2.1+ or `espressif32` platform v6.0.0+ for Arduino framework support
- **PLT-002**: NeoPixelBus library v2.7.0+ - Hardware-accelerated WS2812B control library supporting DMA/UART/RMT methods
- **PLT-003**: Node.js v18+ with `ws` WebSocket library v8.0+ for server-side broadcasting
- **PLT-004**: Modern web browser with WebGL support for LED preview simulator (Chrome 90+, Firefox 88+, Safari 15+)

### Compliance Dependencies

- **COM-001**: None - This is a hobby/event system with no regulatory compliance requirements

## 9. Examples & Edge Cases

### 9.1 Coordinate Mapping Examples

#### Strip Topology

```cpp
// Linear strip: 50 LEDs in a row
LedTopology stripTopo = { LedTopologyType::STRIP, 1, 50, 0 };
controller.begin(4, 50, stripTopo);

// Index directly corresponds to physical LED
controller.setPixel(0, 255, 0, 0);    // First LED red
controller.setPixel(49, 0, 0, 255);   // Last LED blue
controller.show();
```

#### Matrix Topology (Zigzag)

```cpp
// 8x8 matrix with zigzag wiring (even rows reversed)
LedTopology matrixTopo = { LedTopologyType::MATRIX_ZIGZAG, 8, 8, 0 };
controller.begin(4, 64, matrixTopo);

// XY coordinate (0,0) is top-left, (7,7) is bottom-right
controller.setPixelXY(0, 0, 255, 0, 0);  // Top-left red
controller.setPixelXY(7, 0, 0, 255, 0);  // Top-right green (physical LED 7)
controller.setPixelXY(0, 1, 0, 0, 255);  // Second row, first col (physical LED 15)
controller.show();

// Internal mapping for zigzag:
// Row 0: LEDs 0-7 (left to right)
// Row 1: LEDs 8-15 (right to left) ← reversed
// Row 2: LEDs 16-23 (left to right)
// ...
```

#### Ring Topology

```cpp
// 24-LED ring, first LED at top (0°)
LedTopology ringTopo = { LedTopologyType::RING, 24, 1, 0 };
controller.begin(4, 24, ringTopo);

// Polar coordinates: 0° = top, clockwise
controller.setPixelPolar(0, 255, 0, 0);    // Top red
controller.setPixelPolar(90, 0, 255, 0);   // Right green
controller.setPixelPolar(180, 0, 0, 255);  // Bottom blue
controller.setPixelPolar(270, 255, 255, 0); // Left yellow
controller.show();
```

### 9.2 Effect Implementation Example

```cpp
// Fire effect: simulates flickering flames using HSV color space

class FireEffect : public LedEffect {
public:
    FireEffect(LedController& controller) 
        : LedEffect(controller), _cooling(50), _sparking(120) {}

    bool update(unsigned long dt) override {
        _elapsedTime += dt;
        
        // Allocate heat array (one byte per LED)
        uint8_t heat[_controller.getMaxLeds()];
        memset(heat, 0, sizeof(heat));
        
        // Step 1: Cool down cells based on cooling parameter
        for (uint16_t i = 0; i < _controller.getMaxLeds(); i++) {
            heat[i] = qsub8(heat[i], random8(0, ((_cooling * 10) / _controller.getMaxLeds()) + 2));
        }
        
        // Step 2: Heat from each cell drifts up and diffuses
        for (uint16_t k = _controller.getMaxLeds() - 1; k >= 2; k--) {
            heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
        }
        
        // Step 3: Randomly ignite new sparks near bottom
        if (random8() < _sparking) {
            uint16_t y = random8(7);
            heat[y] = qadd8(heat[y], random8(160, 255));
        }
        
        // Step 4: Convert heat to LED colors
        for (uint16_t j = 0; j < _controller.getMaxLeds(); j++) {
            // Map heat (0-255) to flame colors (red-orange-yellow-white)
            uint8_t t192 = scale8_video(heat[j], 191);
            uint8_t hue = t192 / 3;  // Hue 0-63 (red to yellow)
            uint8_t sat = 255;
            uint8_t val = heat[j];
            _controller.setPixelHSV(j, hue, sat, val);
        }
        
        _controller.show();
        return true;  // Loop forever
    }
    
    void setParam(const char* key, float value) override {
        if (strcmp(key, "cooling") == 0) _cooling = (uint8_t)value;
        else if (strcmp(key, "sparking") == 0) _sparking = (uint8_t)value;
    }
    
private:
    uint8_t _cooling;   // Rate of cooling (0-255)
    uint8_t _sparking;  // Probability of new sparks (0-255)
};
```

### 9.3 Platform Detection Logic

```cpp
// Automatic platform detection and method selection
// (clients/shared/leds/LedController.cpp)

void LedController::begin(uint8_t pin, uint16_t ledCount, LedTopology topology) {
    // Validate LED count against platform limits
    #ifdef ESP8266
        const uint16_t MAX_LEDS = 300;
        const char* chipType = "ESP8266";
    #elif defined(ESP32)
        const uint16_t MAX_LEDS = 1000;
        const char* chipType = "ESP32";
    #else
        #error "Unsupported platform: Must be ESP8266 or ESP32"
    #endif
    
    if (ledCount > MAX_LEDS) {
        Serial.printf("[LED] ERROR: %d LEDs exceeds platform limit %d\n", ledCount, MAX_LEDS);
        ledCount = MAX_LEDS;
    }
    
    _ledCount = ledCount;
    _topology = topology;
    _brightness = 255;
    
    // Select optimal hardware method based on platform and pin
    #ifdef ESP8266
        if (pin == 3) {
            // GPIO3 (RX) supports DMA method - best performance, no Serial debug
            Serial.println("[LED] Using DMA method on GPIO3 (no serial debug available)");
            auto* bus = new NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>(ledCount, pin);
            _busPtr = bus;
        } else {
            // Other pins use UART method on GPIO2 (D4)
            Serial.printf("[LED] Using UART method on GPIO%d\n", pin);
            auto* bus = new NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1800KbpsMethod>(ledCount);
            _busPtr = bus;
        }
    #elif defined(ESP32)
        // ESP32 uses RMT on any pin
        Serial.printf("[LED] Using RMT method on GPIO%d\n", pin);
        auto* bus = new NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(ledCount, pin);
        _busPtr = bus;
    #endif
    
    static_cast<NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>*>(_busPtr)->Begin();
}
```

### 9.4 Edge Case: LED Count Mismatch

**Scenario**: Admin configures 64 LEDs but device detects 60 LEDs

**Handling**:
```javascript
// Server validation logic (server/src/ws/ConnectionManager.js)
function handleLedRegistration(client, payload) {
    const { ledCount, chipType } = payload;
    const config = getLedConfig(client.type);  // Get expected configuration
    const expected = config.expectedLedCount;
    const tolerance = 5;
    
    if (Math.abs(ledCount - expected) > tolerance) {
        console.warn(`[LED] Device ${client.id} reports ${ledCount} LEDs, expected ${expected}`);
        client.ledWarning = `Count mismatch: detected ${ledCount}, expected ${expected}`;
        
        // Send warning but allow device to continue with detected count
        client.ws.send(JSON.stringify({
            type: 'led_config_warning',
            payload: {
                message: `LED count mismatch (±${tolerance} tolerance)`,
                detected: ledCount,
                expected: expected,
                action: 'using_detected'
            }
        }));
    }
    
    // Adjust configuration to match detected hardware
    client.ledCount = ledCount;
}
```

### 9.5 Edge Case: WebSocket Disconnection During Animation

**Scenario**: Device loses WiFi while playing winner celebration

**Handling**:
```cpp
// Firmware logic (clients/esp8266-sensor/src/main.cpp)
void loop() {
    // Check WebSocket connection status
    if (!wsClient.isConnected() && ledAnimator.isPlaying()) {
        unsigned long disconnectedMs = millis() - wsClient.getLastMessageTime();
        
        if (disconnectedMs > 5000) {
            // Switch to disconnected status pattern after 5 seconds
            statusLed.setState(LedState::WIFI_ONLY);
            
            // Keep current animation running from local state
            ledAnimator.loop();  // Continue until effect completes
            
            Serial.println("[LED] WebSocket disconnected, animation continues from cache");
        }
    }
    
    // Normal WebSocket reconnection logic
    wsClient.loop();
    
    // Animation loop always executes
    if (!ledAnimator.loop()) {
        // Effect complete and not looping - switch to idle default
        ledAnimator.playEffect(new RainbowEffect(controller, 60), true);
    }
}
```

### 9.6 Edge Case: Power Supply Brownout

**Scenario**: 300 LEDs at full white (18A) exceeds 15A power supply capacity

**Mitigation**:
```cpp
// Power-aware brightness limiting
class PowerLimiter {
public:
    static uint8_t calculateSafeBrightness(uint16_t ledCount, uint16_t maxCurrentMa) {
        const uint16_t MA_PER_LED_FULL_WHITE = 60;
        uint32_t fullCurrentMa = ledCount * MA_PER_LED_FULL_WHITE;
        
        if (fullCurrentMa <= maxCurrentMa) {
            return 255;  // No limiting needed
        }
        
        // Scale brightness to stay within power budget
        uint8_t safeBrightness = (maxCurrentMa * 255UL) / fullCurrentMa;
        Serial.printf("[LED] Power limit: %dmA available, %dmA required at full brightness\n", 
                      maxCurrentMa, fullCurrentMa);
        Serial.printf("[LED] Brightness limited to %d/255 (%.1f%%)\n", 
                      safeBrightness, safeBrightness * 100.0 / 255.0);
        return safeBrightness;
    }
};

// Usage in begin()
_brightness = PowerLimiter::calculateSafeBrightness(_ledCount, 15000);  // 15A supply
```

### 9.7 Edge Case: Multiple Rapid Scoring Events

**Scenario**: Player scores 3 times within 1 second (network queuing)

**Handling**:
```cpp
// Effect queue with priority
class EffectQueue {
public:
    void enqueue(LedEffect* effect, uint8_t priority) {
        _queue.push_back({ effect, priority, millis() });
    }
    
    LedEffect* dequeueNext() {
        if (_queue.empty()) return nullptr;
        
        // Sort by priority (high first), then timestamp (FIFO)
        std::sort(_queue.begin(), _queue.end(), 
            [](const QueuedEffect& a, const QueuedEffect& b) {
                if (a.priority != b.priority) return a.priority > b.priority;
                return a.timestamp < b.timestamp;
            });
        
        auto effect = _queue.front().effect;
        _queue.erase(_queue.begin());
        return effect;
    }
    
private:
    struct QueuedEffect {
        LedEffect* effect;
        uint8_t priority;
        unsigned long timestamp;
    };
    std::vector<QueuedEffect> _queue;
};
```

## 10. Validation Criteria

### Functional Validation

- **VAL-001**: LED boot sequence displays correctly on ESP8266 and ESP32 hardware
- **VAL-002**: All 8 built-in effects render correctly on strip, matrix, and ring topologies
- **VAL-003**: WebSocket protocol extension messages parse and validate successfully
- **VAL-004**: Server REST API endpoints return correct responses for valid and invalid inputs
- **VAL-005**: Admin UI LED configurator saves changes and devices receive updates
- **VAL-006**: Custom effect upload validates JSON schema and rejects invalid patterns

### Performance Validation

- **VAL-007**: LED update latency measured with oscilloscope: <50ms from WebSocket RX to LED state change
- **VAL-008**: Animation frame rate logged over 60-second test: ≥30 FPS sustained
- **VAL-009**: Memory profiling shows LED subsystem uses <40KB RAM on ESP8266
- **VAL-010**: WiFi stability test: No WebSocket disconnections during 10-minute animation loop

### Integration Validation

- **VAL-011**: Game countdown triggers synchronized LED pulse across 4+ sensor clients (±100ms)
- **VAL-012**: Scoring event triggers correct LED flash effect on triggering sensor within 50ms
- **VAL-013**: Winner celebration plays for 7 seconds on winner's device, 2 seconds on losers
- **VAL-014**: Configuration change via admin UI reaches all devices within 2 seconds

### Compatibility Validation

- **VAL-015**: Same firmware binary works on Wemos D1 Mini and NodeMCU v2 (ESP8266)
- **VAL-016**: Same firmware binary works on ESP32 DevKit v1 and ESP32-WROOM-32
- **VAL-017**: Mixed deployment: 4 ESP8266 sensors + 2 ESP32 motors receive coordinated effects
- **VAL-018**: Web admin UI LED configurator works in Chrome, Firefox, Safari (latest versions)

## 11. Migration Path

### Phase 1: Foundation (Week 1)

**Objective**: Create platform-abstracted LED controller with basic functionality

**Steps**:
1. Create directory structure:
   ```
   clients/shared/leds/
       LedController.h
       LedController.cpp
       LedTopology.h
   ```

2. Implement `LedController` class:
   - Constructor, `begin()`, `setPixel()`, `fill()`, `show()`, `setBrightness()`
   - Platform detection logic (`#ifdef ESP8266` / `#ifdef ESP32`)
   - NeoPixelBus integration (DMA/UART/RMT method selection)

3. Add to PlatformIO `lib_deps`:
   ```ini
   lib_deps =
       makuna/NeoPixelBus@^2.7.0
   ```

4. Create simple test firmware:
   ```cpp
   // Test: Boot sequence on 10-LED strip
   void setup() {
       Serial.begin(115200);
       controller.begin(4, 10, { LedTopologyType::STRIP, 1, 10, 0 });
       
       // Red pulse
       for (int i = 0; i < 255; i++) {
           controller.fill(i, 0, 0);
           controller.show();
           delay(5);
       }
   }
   ```

5. Validate on physical hardware:
   - ESP8266 (Wemos D1 Mini) with 10-LED strip on GPIO4
   - ESP32 (DevKit v1) with 10-LED strip on GPIO16
   - Log method selection to serial console

**Deliverables**:
- `LedController.h`/`.cpp` with strip topology support
- Working boot sequence on both platforms
- Documentation: Pin wiring diagram, power supply requirements

**Backward Compatibility**:
- Existing `StatusLed` class remains unchanged
- New LED controller is optional integration (doesn't break existing firmware)

### Phase 2: Animation Engine (Week 2)

**Objective**: Implement effect system and hook to game events

**Steps**:
1. Create effect class hierarchy:
   ```
   clients/shared/leds/
       LedEffect.h
       LedEffect.cpp
       LedAnimator.h
       LedAnimator.cpp
       effects/
           SolidColorEffect.h
           PulseEffect.h
           RainbowEffect.h
           ChaseEffect.h
           SparkleEffect.h
           FireEffect.h
   ```

2. Implement `LedAnimator`:
   - Frame rate control (target FPS)
   - Effect transitions (crossfade, instant)
   - Looping vs one-shot effects
   - Non-blocking operation with yield points

3. Integrate with existing `GameEvent` enum in `websocket.h`:
   ```cpp
   void onGameEvent(GameEvent event) {
       switch (event) {
           case GameEvent::COUNTDOWN_TICK:
               animator.playEffect(new PulseEffect(controller, 240, 1000), false);
               break;
           case GameEvent::WINNER_SELF:
               animator.playEffect(new RainbowExplosionEffect(controller), false);
               break;
           // ...
       }
   }
   ```

4. Test with physical hardware:
   - 50-LED strip on sensor client
   - Trigger effects manually via serial commands
   - Measure frame rate, CPU usage, memory footprint

**Deliverables**:
- Complete effect library (8 built-in effects)
- `LedAnimator` with transition support
- Integration with game event system
- Performance profiling report

**Migration Strategy**:
- Effects run in parallel with existing `StatusLed`
- Status LED continues to show connection state on built-in LED
- New addressable LEDs show game event animations

### Phase 3: Server Configuration & Sync (Week 3)

**Objective**: Add server-side management and WebSocket protocol

**Steps**:
1. Extend WebSocket protocol in `ConnectionManager.js`:
   ```javascript
   function handleRegister(client, payload) {
       // Parse new LED fields
       const { chipType, ledCount, ledCapabilities } = payload;
       client.chipType = chipType;
       client.ledCount = ledCount;
       
       // Send LED configuration to client
       const config = getLedConfig(client.type);
       client.ws.send(JSON.stringify({ type: 'led_config', payload: config }));
   }
   ```

2. Create server data files:
   ```
   server/data/
       led-config.json      # Per-device-type configurations
       led-effects/         # Custom effect library
           custom-abc123.json
   ```

3. Implement REST API routes in `server/src/routes/leds.js`:
   ```javascript
   router.get('/api/leds/config', handleGetConfig);
   router.put('/api/leds/config', handlePutConfig);
   router.get('/api/leds/effects', handleGetEffects);
   router.post('/api/leds/effects', handlePostEffect);
   router.delete('/api/leds/effects/:id', handleDeleteEffect);
   router.post('/api/leds/effects/test', handleTestEffect);
   ```

4. Add WebSocket message handlers in firmware:
   ```cpp
   void handleLedConfig(JsonDocument& doc) {
       auto topology = doc["topology"];
       _expectedLedCount = doc["expectedLedCount"];
       _brightness = doc["brightness"];
       
       // Validate and apply configuration
       if (validateLedCount()) {
           controller.setBrightness(_brightness);
           Serial.println("[LED] Configuration applied");
       }
   }
   ```

5. Test configuration sync:
   - Change LED count in `led-config.json`
   - Restart server, connect device
   - Verify device receives and applies configuration
   - Test validation warnings for mismatches

**Deliverables**:
- REST API endpoints with validation
- WebSocket protocol handlers
- Configuration persistence
- Auto-sync on device registration

**Migration Strategy**:
- Default configurations embedded in firmware as fallback
- Server configuration overrides defaults when available
- Devices function independently if server unreachable

### Phase 4: Web Admin Interface (Week 4)

**Objective**: Build LED configuration UI with live preview

**Steps**:
1. Create admin UI components in `clients/web/`:
   ```
   clients/web/
       led-config.html      # LED configuration page
       css/
           led-config.css
       js/
           led-config.js    # Configuration logic
           led-simulator.js # WebGL LED preview
   ```

2. Add LED configuration panel to `admin.html`:
   ```html
   <section id="led-config">
       <h2>LED Configuration</h2>
       <div id="device-selector">
           <label>Device Type:</label>
           <select id="device-type">
               <option value="sensor">Sensor Clients</option>
               <option value="motor">Motor Clients</option>
           </select>
       </div>
       <div id="topology-config">
           <label>Topology:</label>
           <select id="topology-type">
               <option value="STRIP">Strip (Linear)</option>
               <option value="MATRIX_ZIGZAG">Matrix 8x8 (Zigzag)</option>
               <option value="RING">Ring (Circular)</option>
           </select>
           <input type="number" id="led-count" min="1" max="300" placeholder="LED Count">
       </div>
       <div id="led-preview">
           <canvas id="led-canvas" width="800" height="600"></canvas>
       </div>
       <button id="save-led-config">Save Configuration</button>
       <button id="test-pattern">Test Pattern</button>
   </section>
   ```

3. Implement WebGL LED simulator:
   ```javascript
   class LedSimulator {
       constructor(canvas, topology) {
           this.canvas = canvas;
           this.ctx = canvas.getContext('2d');
           this.topology = topology;
           this.pixels = new Array(topology.ledCount).fill({ r: 0, g: 0, b: 0 });
       }
       
       render() {
           // Draw LEDs as circles with glow effect
           for (let i = 0; i < this.pixels.length; i++) {
               const pos = this.getPixelPosition(i);
               this.drawLed(pos.x, pos.y, this.pixels[i]);
           }
       }
       
       setPixel(index, r, g, b) {
           this.pixels[index] = { r, g, b };
           this.render();
       }
   }
   ```

4. Connect to WebSocket for live preview:
   ```javascript
   wsConnection.on('led_effect', (effect) => {
       // Play effect in simulator
       simulator.playEffect(effect);
   });
   ```

5. Implement save/load logic:
   ```javascript
   async function saveLedConfig() {
       const config = {
           deviceType: document.getElementById('device-type').value,
           config: {
               topology: getTopologyConfig(),
               expectedLedCount: parseInt(document.getElementById('led-count').value),
               brightness: parseInt(document.getElementById('brightness').value),
               effects: getEffectMappings()
           }
       };
       
       const response = await fetch('/api/leds/config', {
           method: 'PUT',
           headers: { 'Content-Type': 'application/json' },
           body: JSON.stringify(config)
       });
       
       if (response.ok) {
           showNotification('Configuration saved and synced to devices');
       }
   }
   ```

**Deliverables**:
- LED configuration UI with device selector
- WebGL simulator with real-time preview
- Test pattern button for hardware verification
- Visual device status indicators (connected, LED count, warnings)

### Phase 5: Matrix & Ring Support (Stretch Goal)

**Objective**: Add advanced topology support

**Steps**:
1. Implement XY mapping in `LedController`:
   ```cpp
   uint16_t LedController::_xyToIndex(uint8_t x, uint8_t y) {
       if (_topology.type == LedTopologyType::MATRIX_ZIGZAG) {
           // Even rows: left-to-right, odd rows: right-to-left
           if (y % 2 == 0) {
               return y * _topology.width + x;
           } else {
               return y * _topology.width + (_topology.width - 1 - x);
           }
       } else if (_topology.type == LedTopologyType::MATRIX_PROG) {
           // All rows left-to-right
           return y * _topology.width + x;
       }
       return 0;
   }
   ```

2. Implement polar mapping for rings:
   ```cpp
   uint16_t LedController::_polarToIndex(uint16_t angle) {
       // Map 0-359° to LED indices
       uint16_t offsetAngle = (angle + _topology.firstLedAngle) % 360;
       return (offsetAngle * _ledCount) / 360;
   }
   ```

3. Create matrix-specific effects:
   - Scrolling text
   - Pong-style ball animation
   - Conway's Game of Life

4. Create ring-specific effects:
   - Spinning chase
   - Radial pulse
   - Clock face (12 "hour" positions)

5. Test on physical hardware:
   - 8x8 matrix (Adafruit NeoPixel Shield)
   - 24-LED ring (Adafruit NeoPixel Ring)

**Deliverables**:
- Matrix/ring coordinate mapping
- Topology-specific effects
- Updated simulator with matrix/ring rendering
- Hardware validation report

### Testing Strategy per Phase

**Phase 1 Testing**:
- Unit test: Platform detection logic
- Unit test: Strip coordinate mapping (index = physical position)
- Hardware test: Boot sequence on ESP8266 and ESP32
- Hardware test: Serial console logs correct method selection

**Phase 2 Testing**:
- Unit test: Each effect class renders correctly (capture output frames)
- Unit test: Animator frame rate control (measure actual FPS)
- Unit test: Effect transitions (crossfade blending)
- Hardware test: 60-second animation loop with frame rate logging
- Hardware test: Memory profiling (heap usage before/during/after)

**Phase 3 Testing**:
- Integration test: WebSocket registration with LED fields
- Integration test: Server broadcasts led_config on registration
- Integration test: Firmware validates and applies configuration
- Integration test: Mismatch warning displayed in admin UI
- API test: All REST endpoints with valid/invalid inputs

**Phase 4 Testing**:
- UI test: Save configuration, verify broadcast to connected devices
- UI test: Test pattern button sends effect, visible on physical hardware
- UI test: LED simulator renders correctly for strip/matrix/ring
- UI test: WebSocket events update simulator in real-time
- Browser compatibility test: Chrome, Firefox, Safari (latest)

**Phase 5 Testing**:
- Unit test: XY mapping for zigzag and progressive matrices
- Unit test: Polar mapping for rings with different firstLedAngle
- Hardware test: Matrix effects on 8x8 physical hardware
- Hardware test: Ring effects on 24-LED physical ring
- Integration test: Topology selector in admin UI works with new types

### Rollback Plan

If critical issues discovered during migration:

1. **Firmware Rollback**: Revert to previous Git commit, reflash devices
2. **Server Rollback**: Remove LED routes from Express app, restart server
3. **Data Rollback**: Delete `led-config.json` and `led-effects/` directory

Devices without LED hardware continue working normally (LED subsystem is optional enhancement, not required for core game functionality).

## 12. Related Specifications / Further Reading

- **[PRD: LED Control Layer](../docs/PRD-LED-Control-Layer.md)** - Product requirements document for this feature
- **[GitHub Issues #4-#17](https://github.com/tamaygz/esp-rollaball-derby/issues?q=is%3Aissue+label%3Aled-control)** - User stories with acceptance criteria and spec cross-references
- **[NeoPixelBus Library Documentation](https://github.com/Makuna/NeoPixelBus/wiki)** - Hardware methods and API reference
- **[WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)** - 800kHz timing requirements
- **[ESP8266 Non-OS SDK Programming Guide](https://www.espressif.com/sites/default/files/documentation/2c-esp8266_non_os_sdk_api_reference_en.pdf)** - Yield points and WiFi integration
- **[FastLED XY Mapping Guide](https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples#using-multiple-controllers-with-xy-mapping)** - Matrix wiring patterns

---

> **Implementation ready**: All 14 user stories have been created as GitHub issues with cross-references to this specification. Proceed to Phase 1 implementation or create detailed implementation plan.
