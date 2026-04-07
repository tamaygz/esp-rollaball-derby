---
goal: 'LED Control Layer - Phase 2: Animation Engine & Effects Library'
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: Roll-a-Ball Derby Engineering Team
status: 'Planned'
tags: [feature, embedded, hardware, led, animation, effects, esp8266, esp32, phase2]
---

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

# Implementation Plan: LED Control Layer - Phase 2

This plan covers Phase 2 (Animation Engine & Effects) of the LED Control Layer feature, building upon the Phase 1 core abstraction to deliver a non-blocking animation engine with 6 core LED effects, game event integration, and smooth effect transitions.

---

## Section 1 — Requirements & Constraints

### Requirements

- **REQ-001**: Implement non-blocking animation loop with configurable FPS (15-60 FPS range)
- **REQ-002**: Support 6 core effects: Solid, Blink, Pulse, Rainbow, Chase, Sparkle
- **REQ-003**: Effect base class with virtual methods for extensibility
- **REQ-004**: Hook effects to game events: countdown tick, scoring (+1/+2/+3), winner/loser
- **REQ-005**: Effect transition system with crossfade support (configurable duration)
- **REQ-006**: Maintain minimum 30 FPS animation rate during WiFi activity
- **REQ-007**: Animation engine memory footprint: <10KB RAM (ESP8266), <20KB RAM (ESP32)
- **REQ-008**: Effect parameter configuration: color, speed, brightness, direction
- **REQ-009**: Support effect chaining (sequence multiple effects)
- **REQ-010**: Graceful fallback if effect unsupported (log warning, show solid color)

### Security Requirements

- **SEC-001**: Validate effect parameters (FPS: 15-60, speed: 0.1-10.0, brightness: 0-255)
- **SEC-002**: Bounds check all animation buffer accesses
- **SEC-003**: Prevent infinite loops in effect update() methods (max iteration limit)

### Constraints

- **CON-001**: Animation loop must call `yield()` every 50ms to prevent WiFi disconnects
- **CON-002**: Effect update calculations must complete within frame budget (16ms @ 60 FPS)
- **CON-003**: ESP8266 single-core: animation + WiFi + sensors must coexist
- **CON-004**: No dynamic memory allocation in animation loop (pre-allocate buffers)
- **CON-005**: Effects must work identically on ESP8266 and ESP32 (platform-agnostic)
- **CON-006**: StatusLed connection patterns must coexist with game effects

### Guidelines

- **GUD-001**: Use HSV color space for smooth color transitions in effects
- **GUD-002**: Pre-compute expensive math (sin/cos tables) at initialization
- **GUD-003**: Document each effect's visual behavior with ASCII art or diagrams
- **GUD-004**: Provide sensible default parameters for each effect
- **GUD-005**: Log effect transitions to serial console for debugging
- **GUD-006**: Use gamma correction for perceptually linear brightness changes

### Patterns

- **PAT-001**: Template Method pattern for effect base class with update() hook
- **PAT-002**: State pattern for animation lifecycle (idle, playing, transitioning, paused)
- **PAT-003**: Strategy pattern for effect selection and parameter passing
- **PAT-004**: Observer pattern for game event → effect mapping

---

## Section 2 — Implementation Steps

### Implementation Phase 2.1 — Effect Base Class Architecture

- **GOAL-001**: LedEffect base class with extensible interface for custom effects

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-001 | Create `clients/shared/leds/LedEffect.h` with abstract base class | | |
| TASK-002 | Define virtual methods: `begin()`, `update(deltaMs)`, `reset()`, `isComplete()` | | |
| TASK-003 | Add protected members: `_controller` pointer, `_startTime`, `_params` struct | | |
| TASK-004 | Create `EffectParams` struct with common parameters (color, speed, brightness, direction) | | |
| TASK-005 | Add effect identifier enum: `EffectType` (SOLID, BLINK, PULSE, RAINBOW, CHASE, SPARKLE) | | |
| TASK-006 | Implement `setParams(EffectParams params)` method with validation | | |
| TASK-007 | Add `getName()` method returning effect name string for logging | | |

### Implementation Phase 2.2 — Animation Loop Manager

- **GOAL-002**: Non-blocking animation manager with FPS control and effect transitions

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-008 | Create `clients/shared/leds/AnimationManager.h` class | | |
| TASK-009 | Add `setTargetFPS(uint8_t fps)` method with validation (15-60 range) | | |
| TASK-010 | Implement `loop()` method with frame timing and yield points | | |
| TASK-011 | Add `playEffect(LedEffect* effect)` method to start effect | | |
| TASK-012 | Implement active effect tracking with `_currentEffect` pointer | | |
| TASK-013 | Add frame timing: calculate `deltaMs` since last update | | |
| TASK-014 | Implement FPS limiter: skip frames if update takes too long | | |
| TASK-015 | Add diagnostic counters: `_frameCount`, `_droppedFrames`, `_avgFrameTime` | | |
| TASK-016 | Implement `getStats()` method returning performance metrics | | |

### Implementation Phase 2.3 — Effect Transition System

- **GOAL-003**: Smooth crossfade transitions between effects with configurable duration

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-017 | Add `_transitionEffect` pointer to AnimationManager for pending effect | | |
| TASK-018 | Implement `transitionTo(LedEffect* newEffect, uint16_t durationMs)` method | | |
| TASK-019 | Create transition state machine: IDLE, FADING_OUT, FADING_IN, COMPLETE | | |
| TASK-020 | Implement crossfade algorithm: blend old and new effect buffers | | |
| TASK-021 | Add brightness-based fade: reduce old effect brightness, increase new effect | | |
| TASK-022 | Handle immediate transition option (durationMs = 0) | | |
| TASK-023 | Clean up old effect after transition completes | | |
| TASK-024 | Test transition edge cases: transition during transition, effect completes during fade | | |

### Implementation Phase 2.4 — Core Effect: Solid Color

- **GOAL-004**: Solid color effect with single static color across all LEDs

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-025 | Create `clients/shared/leds/effects/SolidEffect.h` inheriting LedEffect | | |
| TASK-026 | Implement `begin()`: set all LEDs to configured color | | |
| TASK-027 | Implement `update()`: no-op (solid color is static) | | |
| TASK-028 | Implement `isComplete()`: always return false (infinite effect) | | |
| TASK-029 | Test with red, green, blue, white, custom HSV colors | | |

### Implementation Phase 2.5 — Core Effect: Blink

- **GOAL-005**: Blink effect alternating between color and off with configurable frequency

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-030 | Create `clients/shared/leds/effects/BlinkEffect.h` | | |
| TASK-031 | Add parameters: `onDurationMs`, `offDurationMs`, `blinkCount` (0 = infinite) | | |
| TASK-032 | Implement state toggle: track on/off state and elapsed time | | |
| TASK-033 | Implement `update()`: toggle all LEDs on/off based on timing | | |
| TASK-034 | Implement `isComplete()`: return true when `blinkCount` reached | | |
| TASK-035 | Test with 1 Hz, 5 Hz, 10 Hz blink rates | | |
| TASK-036 | Test finite blink count (3 blinks, 6 blinks) | | |

### Implementation Phase 2.6 — Core Effect: Pulse (Breathing)

- **GOAL-006**: Pulse effect with smooth sinusoidal brightness variation

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-037 | Create `clients/shared/leds/effects/PulseEffect.h` | | |
| TASK-038 | Pre-compute sine wave lookup table (256 values, 0-255 brightness) | | |
| TASK-039 | Add parameter: `periodMs` (duration of one pulse cycle) | | |
| TASK-040 | Implement `update()`: calculate brightness using sine wave based on elapsed time | | |
| TASK-041 | Apply brightness to all LEDs with base color | | |
| TASK-042 | Test with 1-second, 2-second, 5-second period | | |
| TASK-043 | Verify smooth ramp with no visible steps | | |

### Implementation Phase 2.7 — Core Effect: Rainbow

- **GOAL-007**: Rainbow effect displaying full HSV spectrum across LED strip

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-044 | Create `clients/shared/leds/effects/RainbowEffect.h` | | |
| TASK-045 | Add parameter: `cycleSpeedMs` (time for rainbow to complete one cycle) | | |
| TASK-046 | Calculate hue offset per LED: `360 / ledCount` | | |
| TASK-047 | Implement `update()`: increment global hue offset, set each LED to HSV color | | |
| TASK-048 | Support direction parameter: forward/reverse | | |
| TASK-049 | Test with 50 LEDs: verify smooth color transitions | | |
| TASK-050 | Test with different cycle speeds: 1s, 3s, 10s | | |

### Implementation Phase 2.8 — Core Effect: Chase

- **GOAL-008**: Chase effect with moving light pattern (theater marquee style)

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-051 | Create `clients/shared/leds/effects/ChaseEffect.h` | | |
| TASK-052 | Add parameters: `color`, `tailLength`, `speedPixelsPerSecond`, `direction` | | |
| TASK-053 | Implement moving pixel index calculation based on elapsed time | | |
| TASK-054 | Implement tail: fade trailing pixels with exponential decay | | |
| TASK-055 | Handle wraparound: chase loops from end to beginning | | |
| TASK-056 | Support reverse direction: chase moves backward | | |
| TASK-057 | Test with 3-pixel tail, 10-pixel tail, full-strip tail | | |
| TASK-058 | Verify smooth motion at 30, 60 FPS | | |

### Implementation Phase 2.9 — Core Effect: Sparkle

- **GOAL-009**: Sparkle effect with random LED twinkles (starfield style)

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-059 | Create `clients/shared/leds/effects/SparkleEffect.h` | | |
| TASK-060 | Add parameters: `baseColor`, `sparkleColor`, `density` (0.0-1.0), `fadeSpeed` | | |
| TASK-061 | Initialize all LEDs to `baseColor` | | |
| TASK-062 | Implement random sparkle: each frame, probability `density` of LED sparking | | |
| TASK-063 | Use `random()` seeded with `micros()` for ESP8266 compatibility | | |
| TASK-064 | Implement fade: reduce brightness of sparkling LEDs exponentially | | |
| TASK-065 | Test with low density (0.05), medium (0.2), high (0.5) | | |
| TASK-066 | Verify even distribution across strip (no clustering) | | |

### Implementation Phase 2.10 — Game Event Integration

- **GOAL-010**: Map game events to LED effects and trigger automatically

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-067 | Update `clients/esp8266-sensor/src/websocket.h`: extend `GameEvent` enum | | |
| TASK-068 | Add events: `SCORE_PLUS1`, `SCORE_PLUS2`, `SCORE_PLUS3`, `ZERO_ROLL` | | |
| TASK-069 | Create `clients/shared/leds/GameEventMapper.h` class | | |
| TASK-070 | Implement `onEvent(GameEvent event)` method with effect selection logic | | |
| TASK-071 | Map `COUNTDOWN_TICK` → BlinkEffect (blue, 600ms on, 400ms off, 1 count) | | |
| TASK-072 | Map `SCORE_PLUS1` → BlinkEffect (blue, 200ms, 1 count) | | |
| TASK-073 | Map `SCORE_PLUS2` → BlinkEffect (purple, 200ms on, 100ms off, 2 count) | | |
| TASK-074 | Map `SCORE_PLUS3` → SparkleEffect (gold, 300ms duration) | | |
| TASK-075 | Map `ZERO_ROLL` → PulseEffect (red, 500ms fade out) | | |
| TASK-076 | Map `WINNER_SELF` → RainbowEffect (7 seconds, fast cycle) | | |
| TASK-077 | Map `WINNER_OTHER` → PulseEffect (red, 2 seconds) | | |
| TASK-078 | Integrate GameEventMapper into main.cpp event handling | | |
| TASK-079 | Test each event trigger: verify correct effect plays | | |

### Implementation Phase 2.11 — StatusLed Coexistence

- **GOAL-011**: Connection state patterns work alongside game effects without conflicts

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-080 | Define effect priority levels: STATUS (highest), GAME_EVENT (medium), AMBIENT (lowest) | | |
| TASK-081 | Modify AnimationManager to track effect priority | | |
| TASK-082 | Implement priority-based effect interruption: high priority overrides low | | |
| TASK-083 | After high-priority effect completes, restore previous lower-priority effect | | |
| TASK-084 | Update StatusLed: register connection state as STATUS priority | | |
| TASK-085 | Test: connection state blink during game effect → connection takes precedence | | |
| TASK-086 | Test: game effect during connected state → effect plays, returns to solid green | | |

### Implementation Phase 2.12 — Performance Optimization

- **GOAL-012**: Achieve 30+ FPS with 50-LED strip on ESP8266 during WiFi activity

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-087 | Profile effect update times: log execution duration for each effect type | | |
| TASK-088 | Optimize hot paths: replace float math with integer arithmetic where possible | | |
| TASK-089 | Implement early exit in update() if no visual change needed | | |
| TASK-090 | Add `_dirty` flag to effects: skip show() if buffer unchanged | | |
| TASK-091 | Pre-compute sin/cos tables for Pulse effect (avoid runtime math) | | |
| TASK-092 | Test with 50 LEDs @ 30 FPS: verify <33ms frame time | | |
| TASK-093 | Test with 50 LEDs @ 60 FPS: measure dropped frame rate | | |
| TASK-094 | Verify WiFi stability: WebSocket connection maintained during 5-minute animation | | |

### Implementation Phase 2.13 — Documentation & Examples

- **GOAL-013**: Phase 2 fully documented with example code and troubleshooting guide

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-095 | Update `clients/shared/leds/README.md` with animation section | | |
| TASK-096 | Document effect base class interface and how to create custom effects | | |
| TASK-097 | Document each of 6 core effects with parameter descriptions | | |
| TASK-098 | Create `examples/led-game-events/main.cpp` demonstrating event-triggered effects | | |
| TASK-099 | Create `examples/led-all-effects/main.cpp` showcasing all 6 effects in sequence | | |
| TASK-100 | Add troubleshooting section: dropped frames, WiFi disconnects, jerky animation | | |
| TASK-101 | Add Doxygen comments to all LedEffect methods | | |
| TASK-102 | Document performance characteristics: frame time, memory usage per effect | | |

---

## Section 3 — Alternatives

- **ALT-001**: Use fixed timestep instead of delta time — Rejected because variable frame rates are unavoidable on ESP8266 under WiFi load. Delta time provides smoother animation during lag spikes.

- **ALT-002**: Implement effects as static functions instead of classes — Rejected because class-based approach enables effect composition, parameter encapsulation, and easier extension by users.

- **ALT-003**: Pre-render animation frames to buffer — Rejected because 50 LEDs × 60 FPS × 3 bytes = 9KB per second of animation, exceeds ESP8266 RAM for any reasonable buffer depth.

- **ALT-004**: Use Arduino Ticker library for precise frame timing — Rejected because Ticker uses hardware timers that conflict with WiFi stack on ESP8266.

- **ALT-005**: Implement all effects in single switch statement — Rejected because violates Open/Closed Principle, makes code harder to extend. Virtual method dispatch overhead is negligible.

- **ALT-006**: Support user-uploaded effect code via LittleFS — Rejected for Phase 2 scope. Security risk (code injection), complexity high. Deferred to future phase with sandboxing.

---

## Section 4 — Dependencies

- **DEP-001**: Phase 1 completion — `LedController` class with `setPixel()`, `show()`, `loop()` methods
- **DEP-002**: NeoPixelBus library (version 2.7.0+) — Already added in Phase 1
- **DEP-003**: Existing `GameEvent` enum in `websocket.h` — Will be extended with new event types
- **DEP-004**: Arduino `millis()` and `micros()` functions for timing
- **DEP-005**: C++ standard library `<algorithm>` for min/max functions
- **DEP-006**: ESP8266/ESP32 `random()` function for Sparkle effect
- **DEP-007**: Phase 1 hardware testing complete — Ensures LED controller works before adding complexity

---

## Section 5 — Files

### Created

- **FILE-001**: `clients/shared/leds/LedEffect.h` — Abstract base class for all LED effects
- **FILE-002**: `clients/shared/leds/AnimationManager.h` — Non-blocking animation loop manager
- **FILE-003**: `clients/shared/leds/AnimationManager.cpp` — Animation manager implementation
- **FILE-004**: `clients/shared/leds/GameEventMapper.h` — Maps game events to LED effects
- **FILE-005**: `clients/shared/leds/GameEventMapper.cpp` — Game event mapping logic
- **FILE-006**: `clients/shared/leds/effects/SolidEffect.h` — Solid color effect
- **FILE-007**: `clients/shared/leds/effects/BlinkEffect.h` — Blink/flash effect
- **FILE-008**: `clients/shared/leds/effects/PulseEffect.h` — Breathing/pulse effect
- **FILE-009**: `clients/shared/leds/effects/RainbowEffect.h` — Rainbow spectrum effect
- **FILE-010**: `clients/shared/leds/effects/ChaseEffect.h` — Moving chase/marquee effect
- **FILE-011**: `clients/shared/leds/effects/SparkleEffect.h` — Random sparkle/twinkle effect
- **FILE-012**: `examples/led-game-events/main.cpp` — Example: event-triggered effects
- **FILE-013**: `examples/led-all-effects/main.cpp` — Example: showcase all 6 effects

### Modified

- **FILE-014**: `clients/shared/leds/README.md` — Add animation section, effect documentation
- **FILE-015**: `clients/esp8266-sensor/src/websocket.h` — Extend GameEvent enum with scoring events
- **FILE-016**: `clients/esp8266-sensor/src/main.cpp` — Integrate AnimationManager and GameEventMapper
- **FILE-017**: `clients/esp8266-sensor/src/led.h` — Update StatusLed to use priority system
- **FILE-018**: `clients/esp8266-sensor/src/led.cpp` — Implement effect priority interruption

---

## Section 6 — Testing

### Compilation Tests

- **TEST-001**: ESP8266 builds without errors: `pio run -e d1_mini` exits with code 0
- **TEST-002**: ESP32 builds without errors: `pio run -e esp32dev` exits with code 0 (if Phase 1 ESP32 variant created)
- **TEST-003**: No compiler warnings in LED effect code (-Wall -Wextra enabled)
- **TEST-004**: All effect headers include guards and include only necessary dependencies

### Functional Tests — Individual Effects

- **TEST-005**: SolidEffect: All LEDs show red when configured with RGB(255,0,0)
- **TEST-006**: BlinkEffect: LEDs blink at exact 1 Hz frequency (measured with timer)
- **TEST-007**: BlinkEffect: Stops after configured blink count (test with 3 blinks)
- **TEST-008**: PulseEffect: Brightness varies smoothly from 0 to 255 in configured period
- **TEST-009**: RainbowEffect: All hues visible across 50-LED strip (red → violet gradient)
- **TEST-010**: ChaseEffect: Single pixel moves smoothly across strip, wraps to beginning
- **TEST-011**: SparkleEffect: Random LEDs twinkle, density matches configured parameter (0.2 = ~10 LEDs @ 50 total)

### Functional Tests — Animation Manager

- **TEST-012**: AnimationManager maintains 30 FPS with RainbowEffect on 50 LEDs
- **TEST-013**: AnimationManager yields to WiFi: WebSocket stays connected during 5-minute animation
- **TEST-014**: Frame timing accurate within ±5ms of target (measure with oscilloscope on GPIO pin toggle)
- **TEST-015**: Effect transitions smoothly with 500ms crossfade (no abrupt color changes)
- **TEST-016**: Effect priority system: STATUS interrupts GAME_EVENT, restores after completion

### Functional Tests — Game Event Integration

- **TEST-017**: COUNTDOWN_TICK event triggers blue blink (600ms on, 400ms off)
- **TEST-018**: SCORE_PLUS1 event triggers single blue flash (200ms)
- **TEST-019**: SCORE_PLUS2 event triggers double purple flash
- **TEST-020**: SCORE_PLUS3 event triggers gold sparkle effect (300ms)
- **TEST-021**: WINNER_SELF event triggers 7-second rainbow celebration
- **TEST-022**: Multiple events within 500ms queue properly (no dropped effects)

### Performance Tests

- **TEST-023**: RainbowEffect @ 30 FPS on 50 LEDs: average frame time <33ms
- **TEST-024**: All effects @ 60 FPS on 50 LEDs: dropped frame rate <5%
- **TEST-025**: Memory usage: animation engine uses <10KB RAM on ESP8266 (check Serial log)
- **TEST-026**: 1000 effect transitions: no memory leaks (check free heap over time)

### Stress Tests

- **TEST-027**: 10 rapid game events (scoring burst): all effects play without system crash
- **TEST-028**: WiFi reconnection during animation: animation pauses gracefully, resumes after reconnect
- **TEST-029**: Continuous animation for 24 hours: no watchdog resets, WiFi stable

---

## Section 7 — Risks & Assumptions

### Risks

- **RISK-001**: Effect update() methods exceed frame budget causing dropped frames — Mitigation: Profile all effects, optimize hot paths, document performance characteristics, allow users to reduce FPS.

- **RISK-002**: Complex effects (Rainbow, Sparkle) cause WiFi disconnects on ESP8266 — Mitigation: Aggressive yield points, frame time limiting, test WiFi stability for each effect.

- **RISK-003**: Memory fragmentation from effect object allocation/deallocation — Mitigation: Pre-allocate effect objects at startup, avoid `new`/`delete` in animation loop.

- **RISK-004**: Effect transition artifacts visible during crossfade — Mitigation: Use linear brightness blending, ensure fade duration long enough (min 200ms), test visually.

- **RISK-005**: Game event flood (sensor malfunction) overwhelms effect queue — Mitigation: Rate limit event processing (max 20/second), drop excess events with warning log.

- **RISK-006**: StatusLed priority conflicts cause visual glitches — Mitigation: Strict priority enforcement, test all priority combinations, document expected behavior.

- **RISK-007**: Custom effect implementation by users introduces bugs — Mitigation: Comprehensive documentation, example custom effect, validate parameters in base class.

### Assumptions

- **ASSUMPTION-001**: Phase 1 hardware testing completed successfully before starting Phase 2
- **ASSUMPTION-002**: ESP8266 running at 160MHz (not 80MHz) for adequate CPU headroom
- **ASSUMPTION-003**: Game events arrive via WebSocket at reasonable rate (<10/second typical)
- **ASSUMPTION-004**: LED strip is 50 LEDs or fewer for Phase 2 testing (larger strips in Phase 3)
- **ASSUMPTION-005**: Users accept occasional dropped frames under extreme WiFi load
- **ASSUMPTION-006**: Effect transitions allocated 200-1000ms duration (not instant)

---

## Section 8 — Related Specifications / Further Reading

### Project Documentation

- [`plan/feature-led-control-phase1-1.md`](./feature-led-control-phase1-1.md) — Phase 1 implementation plan and completion summary
- [`docs/PRD-LED-Control-Layer.md`](../docs/PRD-LED-Control-Layer.md) — Product requirements, Section 9.3 Phase 2
- [`spec/spec-led-control.md`](../spec/spec-led-control.md) — Technical specification, Section 6 (Animation Architecture)
- [`clients/shared/leds/README.md`](../clients/shared/leds/README.md) — Phase 1 LED controller documentation

### GitHub Issues (Phase 2 Related)

- [#5: LED-003 Basic Scoring Feedback](https://github.com/tamaygz/esp-rollaball-derby/issues/5) — Flash on score events
- [#8: LED-004 Countdown Synchronization](https://github.com/tamaygz/esp-rollaball-derby/issues/8) — Countdown tick animation
- [#12: LED-005 Winner Celebration](https://github.com/tamaygz/esp-rollaball-derby/issues/12) — Rainbow celebration effect

### External References

- [FastLED Animation Patterns](https://github.com/FastLED/FastLED/wiki/Pixel-reference) — Reference for effect ideas and algorithms
- [NeoPixelBus Performance Guide](https://github.com/Makuna/NeoPixelBus/wiki/Performance-Guide) — Optimization techniques for ESP8266/ESP32
- [ESP8266 Non-Blocking Patterns](https://arduino-esp8266.readthedocs.io/en/latest/faq/a02-my-esp-crashes.html) — Avoiding watchdog resets
- [HSV Color Space Explanation](https://en.wikipedia.org/wiki/HSL_and_HSV) — Understanding hue-based animations

---

## Execution Handoff

Upon approval of this plan:

1. **Phase 1 Validation**: Confirm Phase 1 hardware testing complete and LED controller stable
2. **Hardware Setup**: Use 50-LED WS2812B strip for Phase 2 testing (more LEDs than Phase 1)
3. **Estimation**: Allocate 1 week (40 hours) for Phase 2 completion
4. **Execution Priority**: 
   - Week 1 Days 1-2: Effect base class + animation manager (TASK-001 through TASK-024)
   - Week 1 Days 3-4: Core effects implementation (TASK-025 through TASK-066)
   - Week 1 Day 5: Game event integration + testing (TASK-067 through TASK-102)
5. **Review Checkpoints**: 
   - After Phase 2.3: Demo effect transitions to stakeholders
   - After Phase 2.9: Demo all 6 effects working
   - After Phase 2.10: Demo game event triggers
6. **Validation**: Complete all TEST-* items, especially performance tests (TEST-023 through TEST-029)
7. **Phase 3 Handoff**: After Phase 2 complete, proceed to server configuration & sync

**Next steps after Phase 2**:
- Phase 3: Server Configuration & Sync (WebSocket protocol extensions, REST API, configuration persistence)
- Phase 4: Web Admin Interface & Preview (LED simulator, effect library management)
- Phase 5: Matrix & Ring Support (XY/polar coordinate mapping, topology-specific effects)

**Success Criteria**:
- ✅ All 6 core effects working smoothly on physical hardware
- ✅ 30+ FPS maintained with 50-LED strip during WiFi activity
- ✅ Game events trigger correct LED effects with <50ms latency
- ✅ StatusLed connection patterns coexist with game effects
- ✅ No WiFi disconnects during 5-minute continuous animation
