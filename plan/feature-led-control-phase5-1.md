---
goal: LED Control Layer Phase 5 - ESP8266 Sensor Firmware LED Integration
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: Development Team
status: 'Planned'
tags: [feature, firmware, embedded, led-control, phase5, esp8266]
---

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

# LED Control Layer Phase 5 — ESP8266 Sensor Firmware LED Integration

**Feature**: Wire the shared LED library (LedController + AnimationManager + GameEventMapper) fully into the ESP8266 sensor firmware, replacing the single-pixel `StatusLed` with an arcade-capable `LedManager`, handle server-pushed `led_config` and `test_effect` WebSocket messages, and include LED metadata in device registration.

**Epic**: LED Control Layer (Phases 1–5)  
**Dependencies**:
- Phase 1 (LED Abstraction Layer — `clients/shared/leds/`) ✅ Complete
- Phase 2 (Animation Engine — `AnimationManager`, `GameEventMapper`, all effects) ✅ Complete
- Phase 3 (Server Config & Sync — `LedConfigManager`, LED REST/WS API) ✅ Complete
- Phase 4 (Web Admin UI & Simulator) ✅ Complete

---

## Section 1 — Requirements & Constraints

### Functional Requirements

- **REQ-001**: ESP8266 sensor firmware initialises the full addressable LED strip at boot using default configuration from `config.h`
- **REQ-002**: On `led_config` WebSocket message, firmware reinitialises the LED strip with the server-supplied count, pin, topology, and brightness — without rebooting
- **REQ-003**: On `test_effect` WebSocket message, firmware plays the specified effect via `AnimationManager` for a duration-limited preview, then resumes the ambient pattern
- **REQ-004**: Device registration payload includes LED metadata (`ledCount`, `chipType`, `ledCapabilities`) so the server can validate configuration on connect
- **REQ-005**: All six game events trigger distinct visual feedback via `GameEventMapper`: SCORE_PLUS1, SCORE_PLUS2, SCORE_PLUS3, ZERO_ROLL, COUNTDOWN_TICK, WINNER_SELF, WINNER_OTHER
- **REQ-006**: Status states (NO_WIFI, WIFI_ONLY, WS_CONNECTED) drive full-strip ambient effects via `AnimationManager` (not just single-pixel blink)
- **REQ-007**: Animation loop runs at ≥30 FPS target while preserving WiFi/WebSocket responsiveness (yield every 50 ms)
- **REQ-008**: LED subsystem uses ≤40 KB RAM on ESP8266 (300 LEDs × 3 bytes = 900 bytes buffer; all effects pre-allocated)

### Non-Functional Requirements

- **REQ-009**: Both PlatformIO build targets (`d1_mini`, `nodemcuv2`) compile without error or warning
- **REQ-010**: Fallback to default built-in pattern if `led_config` message is malformed or pin is invalid
- **REQ-011**: `led_config` reinitialization is non-destructive — a failed `begin()` call leaves the strip in the previous valid state
- **REQ-012**: Serial output logs all LED state transitions for on-bench debugging

### Security Requirements

- **SEC-001**: `led_config` pin field validated against allowed GPIO list before `LedController::begin()` is called — reject GPIO0, GPIO2, GPIO15 (boot-strapping pins)
- **SEC-002**: `led_config` LED count clamped to ESP8266 platform maximum (300) even if server sends higher value
- **SEC-003**: `test_effect` effect name validated against known effects enum before dispatching — unknown names ignored

### Constraints

- **CON-001**: Existing `StatusLed` class in `led.cpp`/`led.h` must be replaced or subsumed — callers in `main.cpp` must be updated in same PR to avoid linker errors
- **CON-002**: `WebsocketsClient` in `websocket.h` is single-message-queue (`_pendingEvent` field stores only one `GameEvent`) — multi-event queuing is out of scope; only the most recent event is applied
- **CON-003**: `LedController::begin()` allocates heap memory for `NeoPixelBus<>` — avoid repeated reinit cycles; cache config and only reinit when values differ from current
- **CON-004**: GPIO3 (RX) is the DMA pin for ESP8266 — when DMA is active, `Serial` debugging is unavailable; this is already documented in `config.h` and acceptable
- **CON-005**: `AnimationManager` must be allocated before `GameEventMapper` (because `GameEventMapper` receives a pointer to `AnimationManager`)
- **CON-006**: No new libraries — `NeoPixelBus`, `ArduinoJson`, `ArduinoWebsockets`, `WiFiManager` are already in `platformio.ini`; do not add new `lib_deps`

### Guidelines

- **GUD-001**: New `LedManager` class goes in `clients/esp8266-sensor/src/led.h` and `led.cpp` (replace `StatusLed` — same files, same includes)
- **GUD-002**: `LedConfig` struct for runtime configuration lives in `config.h` alongside the compile-time defaults
- **GUD-003**: WebSocket LED message handling is added to `websocket.cpp`/`websocket.h` — keep LED config retrieval via a callback/getter (avoid direct coupling)
- **GUD-004**: Effect parameter constants (colors, durations) for game events belong in `led.cpp`, NOT scattered across `main.cpp`
- **GUD-005**: All Serial log lines follow the existing `[COMPONENT]` prefix convention (e.g., `[LED]`, `[WS]`)

### Patterns

- **PAT-001**: `LedManager` wraps `LedController` + `AnimationManager` + `GameEventMapper` as private members — composition over inheritance
- **PAT-002**: Config-driven reinitialization: `LedManager::applyConfig(const LedConfig&)` checks if config differs before calling `LedController::begin()`
- **PAT-003**: Ambient effects (status states) are lower-priority animations; game event effects interrupt and restore ambient via `AnimationManager` transition API
- **PAT-004**: WebSocket message decoding stays in `websocket.cpp`; the decoded data is surfaced via getter methods or a new `LedMessage` struct polled in `main.cpp`

---

## Section 2 — Implementation Steps

### Implementation Phase 5.1 — LedConfig Struct & Defaults

**GOAL-001**: Define `LedConfig` runtime struct and compile-time defaults in `config.h` so all Phase 5 code has a shared configuration object

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-001 | Add `LED_DEFAULT_COUNT`, `LED_DEFAULT_TOPOLOGY_STRIP`, `LED_DEFAULT_PIN`, `LED_DEFAULT_BRIGHTNESS` compile-time constants to `clients/esp8266-sensor/src/config.h` | | |
| TASK-002 | Add `enum class LedTopology { STRIP, RING, MATRIX_ZIGZAG, MATRIX_PROGRESSIVE }` to `config.h`                                  | | |
| TASK-003 | Add `struct LedConfig { uint16_t ledCount; uint8_t pin; LedTopology topology; uint8_t brightness; uint8_t matrixRows; uint8_t matrixCols; }` to `config.h` | | |
| TASK-004 | Add `LedConfig ledConfigDefaults()` inline factory function to `config.h` that returns defaults filled from the `LED_DEFAULT_*` constants | | |

### Implementation Phase 5.2 — WebSocket LED Message Handling

**GOAL-002**: Parse `led_config` and `test_effect` server messages in WSClient, expose decoded data via polling API

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-005 | Add `LedConfigMessage` and `LedTestEffectMessage` structs to `websocket.h` (include `config.h` and `LedEffect.h`)               | | |
| TASK-006 | Add pending-message fields `_pendingLedConfig`, `_hasPendingLedConfig`, `_pendingTestEffect`, `_hasPendingTestEffect` to `WSClient` private section | | |
| TASK-007 | In `_onMessage()`, handle `"led_config"` type: validate and parse `payload.ledCount`, `pin`, `topology`, `brightness` into `_pendingLedConfig` | | |
| TASK-008 | In `_onMessage()`, handle `"test_effect"` type: parse `payload.effectName` and `params` (`color`, `speed`) into `_pendingTestEffect` | | |
| TASK-009 | Add `bool pollLedConfig(LedConfig& out)` public method to `WSClient` — returns and clears `_pendingLedConfig` (returns false if none) | | |
| TASK-010 | Add `bool pollTestEffect(LedTestEffectMessage& out)` public method to `WSClient`                                                | | |
| TASK-011 | Extend `GameEvent` enum in `websocket.h` with `SCORE_PLUS1`, `SCORE_PLUS2`, `SCORE_PLUS3`, `ZERO_ROLL` values                  | | |
| TASK-012 | In `_onMessage()`, parse `"scored"` message `payload.events[]` array and map the first relevant event type to the new `GameEvent` values | | |

### Implementation Phase 5.3 — Extend Registration Payload with LED Metadata

**GOAL-003**: Include `ledCount`, `chipType`, and `ledCapabilities` in the `register` WebSocket message so the server can validate LED count on connection

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-013 | Add `void setLedMetadata(uint16_t ledCount)` method to `WSClient` — stores LED count to include in next registration            | | |
| TASK-014 | In `_sendRegister()`, extend the JSON payload with `ledCount`, `chipType: "ESP8266"`, and `ledCapabilities: { maxLeds: 300, method: "DMA", pin: X }` | | |
| TASK-015 | In `main.cpp` `setup()`, call `wsClient.setLedMetadata(LED_DEFAULT_COUNT)` before `wsClient.begin()`; update again in `loop()` if config changes | | |

### Implementation Phase 5.4 — Replace StatusLed with LedManager

**GOAL-004**: Replace the single-pixel `StatusLed` class with `LedManager` that owns the full LED strip and drives ambient effects, status states, and game event feedback through `AnimationManager`

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-016 | Delete `StatusLed` class from `led.h` and `led.cpp`; replace with `LedManager` class declaration                               | | |
| TASK-017 | Add private members to `LedManager`: `LedController _controller`, `AnimationManager _animator`, `GameEventMapper _mapper`, `LedConfig _config`, `LedState _state` | | |
| TASK-018 | Implement `LedManager::begin(const LedConfig& cfg)` — calls `_controller.begin()`, `_animator.begin()`, `_mapper.begin()`, plays idle rainbow | | |
| TASK-019 | Implement `LedManager::applyConfig(const LedConfig& cfg)` — diffs against `_config`, reinitialises controller only if pin or ledCount changed, always updates brightness | | |
| TASK-020 | Implement `LedManager::setState(LedState state)` using `AnimationManager` ambient effects: `NO_WIFI` → fast red blink, `WIFI_ONLY` → slow orange blink, `WS_CONNECTED` → slow green pulse | | |
| TASK-021 | Implement `LedManager::onGameEvent(GameEventType event)` — delegates to `_mapper.onEvent()` which interrupts ambient with event effect, then restores ambient | | |
| TASK-022 | Implement `LedManager::playTestEffect(const LedTestEffectMessage& msg)` — parses effect name string to `EffectType`, builds `EffectParams`, calls `_animator.playEffect()` | | |
| TASK-023 | Implement `LedManager::loop()` — calls `_controller.loop()` and `_animator.loop()`; no blocking, no `delay()` | | |

### Implementation Phase 5.5 — Wire LedManager into main.cpp

**GOAL-005**: Replace all `led.*` calls in `main.cpp` with `LedManager` equivalents and route all new event types through the manager

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-024 | Replace `static StatusLed led` with `static LedManager ledManager` in `main.cpp`                                               | | |
| TASK-025 | In `setup()`, replace `led.begin(PIN_LED)` with `ledManager.begin(ledConfigDefaults())`                                        | | |
| TASK-026 | In `loop()`, replace `led.loop()` / `led.setState()` calls with `ledManager.loop()` / `ledManager.setState()`                  | | |
| TASK-027 | In `loop()`, add `LedConfig cfg; if (wsClient.pollLedConfig(cfg)) { ledManager.applyConfig(cfg); wsClient.setLedMetadata(cfg.ledCount); }` | | |
| TASK-028 | In `loop()`, add `LedTestEffectMessage tMsg; if (wsClient.pollTestEffect(tMsg)) { ledManager.playTestEffect(tMsg); }`           | | |
| TASK-029 | Extend `GameEvent` → `GameEventType` mapping in `loop()` for `SCORE_PLUS1`, `SCORE_PLUS2`, `SCORE_PLUS3`, `ZERO_ROLL`          | | |
| TASK-030 | Remove now-dead includes/code: `led.triggerCountdownTick()`, `led.triggerWinner()`, `led.triggerLoser()` (replaced by TASK-029) | | |

### Implementation Phase 5.6 — Build Verification & Diagnostics

**GOAL-006**: Confirm both PlatformIO targets build clean, serial log output is informative, and RAM usage stays within budget

| Task     | Description                                                                                                                      | Completed | Date |
|----------|----------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| TASK-031 | Run `pio run -e d1_mini` and `pio run -e nodemcuv2` — resolve all errors and warnings                                          | | |
| TASK-032 | Add `[LED] Initialised: pin=%u count=%u topology=%u brightness=%u` log line in `LedManager::begin()`                           | | |
| TASK-033 | Add `[LED] Config updated: ...diffs...` log line in `LedManager::applyConfig()` showing only changed fields                    | | |
| TASK-034 | Add `[LED] Test effect: name=%s color=#%06X speed=%u` log line in `LedManager::playTestEffect()`                               | | |
| TASK-035 | Verify RAM usage via `pio run --verbose` build output — confirm `LedManager` heap allocation for 300 LEDs ≤ 40 KB               | | |

---

## Section 3 — Alternatives

- **ALT-001**: Keep `StatusLed` and add a separate `LedStrip` class for game effects — Rejected because it would require two `NeoPixelBus` instances on the same GPIO, which is not supported; unified manager is cleaner.
- **ALT-002**: Store pending `led_config` in EEPROM/LittleFS so config survives power cycles — Deferred to Phase 6; Phase 5 focuses on runtime application.
- **ALT-003**: Use a FIFO queue for `GameEvent` instead of single-event overwrite — Rejected because ESP8266 loop runs at ~10ms; only one game event can physically occur per loop iteration from physical sensors.
- **ALT-004**: Handle `led_config` / `test_effect` callback directly to `LedManager` from `WSClient` — Rejected because it creates circular dependency (`wsClient` would need a pointer to `ledManager`); poll pattern avoids coupling.
- **ALT-005**: Implement `WS_CONNECTED` status as solid green on LED 0 only (backward compat) — Rejected because Phase 5 goal is full-strip ambient effects to match game atmosphere.

---

## Section 4 — Dependencies

- **DEP-001**: `clients/shared/leds/LedController.h` / `.cpp` — Core LED hardware abstraction (Phase 1)
- **DEP-002**: `clients/shared/leds/AnimationManager.h` / `.cpp` — Frame-based animation engine (Phase 2)
- **DEP-003**: `clients/shared/leds/GameEventMapper.h` — Game event → effect routing (Phase 2)
- **DEP-004**: `clients/shared/leds/effects/` — All 6 effect headers: Solid, Blink, Pulse, Rainbow, Chase, Sparkle (Phase 2)
- **DEP-005**: `clients/esp8266-sensor/src/websocket.h` / `.cpp` — WSClient with existing reconnect/registration logic
- **DEP-006**: `clients/esp8266-sensor/src/config.h` — Compile-time pin and WiFi constants
- **DEP-007**: `makuna/NeoPixelBus@^2.7.0` — Already in `platformio.ini`; no new lib_deps needed
- **DEP-008**: Phase 3 server — `led_config` and `test_effect` WebSocket messages must be emitted by running server for manual testing

---

## Section 5 — Files

- **FILE-001**: `clients/esp8266-sensor/src/config.h` — **modified** — Add `LedConfig` struct, `LedTopology` enum, `LED_DEFAULT_*` constants, `ledConfigDefaults()` factory function
- **FILE-002**: `clients/esp8266-sensor/src/led.h` — **modified** — Replace `StatusLed` with `LedManager` class; include `AnimationManager.h`, `GameEventMapper.h`, `LedEffect.h`
- **FILE-003**: `clients/esp8266-sensor/src/led.cpp` — **modified** — Full rewrite: implement `LedManager::begin()`, `applyConfig()`, `setState()`, `onGameEvent()`, `playTestEffect()`, `loop()`
- **FILE-004**: `clients/esp8266-sensor/src/websocket.h` — **modified** — Add `LedConfigMessage`, `LedTestEffectMessage` structs; extend `GameEvent` enum; add `pollLedConfig()`, `pollTestEffect()`, `setLedMetadata()` declarations
- **FILE-005**: `clients/esp8266-sensor/src/websocket.cpp` — **modified** — Add `led_config` and `test_effect` message handling in `_onMessage()`; extend `scored` decoding; extend `_sendRegister()` with LED metadata
- **FILE-006**: `clients/esp8266-sensor/src/main.cpp` — **modified** — Replace `StatusLed led` with `LedManager ledManager`; wire LED config/test-effect polling; extend game event mapping

---

## Section 6 — Testing

### Build Tests
- **TEST-001**: `pio run -e d1_mini` exits with code 0, no errors
- **TEST-002**: `pio run -e nodemcuv2` exits with code 0, no errors
- **TEST-003**: RAM usage for LED subsystem in verbose build output ≤ 40 KB

### Unit / Compile-Time Verification
- **TEST-004**: `LedConfig` struct fields match the `led_config` WebSocket payload schema defined in `docs/led-config-flow.md`
- **TEST-005**: `LedTopology` enum values match topology strings expected by `LedController` (STRIP, RING, MATRIX)
- **TEST-006**: `GameEvent` enum in `websocket.h` contains `SCORE_PLUS1`, `SCORE_PLUS2`, `SCORE_PLUS3`, `ZERO_ROLL`

### Manual Hardware Tests (Serial Monitor)
- **TEST-007**: On boot with no server, device shows fast red blink (NO_WIFI state) on full strip
- **TEST-008**: After WiFi connects (no WS), device shows slow orange blink on full strip (WIFI_ONLY)
- **TEST-009**: After WebSocket connects, device shows slow green pulse on full strip (WS_CONNECTED)
- **TEST-010**: `[WS] Registered: playerId=...` line also shows `ledCount=X chipType=ESP8266` in JSON payload visible in Wireshark/server log
- **TEST-011**: Server admin `PUT /api/leds/config/sensor` → device serial shows `[LED] Config updated: count=60 pin=3 brightness=128`; strip reinitialises
- **TEST-012**: Server admin test effect (solid red) → device serial shows `[LED] Test effect: name=solid color=#FF0000 speed=0`; strip turns red for ~3 seconds then resumes ambient
- **TEST-013**: Physical sensor trigger on +1 hole → strip flashes single white blink (SCORE_PLUS1 GameEventMapper effect)
- **TEST-014**: Physical sensor trigger on +2 hole → strip shows double white flash
- **TEST-015**: Physical sensor trigger on +3 hole → strip shows sparkle burst
- **TEST-016**: Server sends countdown → strip shows blue tick pulse each second
- **TEST-017**: Own player wins → strip shows golden rainbow victory animation
- **TEST-018**: Another player wins → strip dims to dark red (loser pattern)

### Regression Tests
- **TEST-019**: Existing server test suite (`cd server && npm test`) passes with 111/111 — no server-side changes expected
- **TEST-020**: `LedController::begin()` with invalid pin (GPIO0) returns `false`; serial shows `[LED] ERROR: Invalid GPIO pin 0`; strip remains in last valid state

---

## Section 7 — Risks & Assumptions

- **RISK-001**: `LedController::begin()` heap allocation may fail on memory-constrained ESP8266 if called multiple times (config reload) — Mitigation: Skip reinit if `ledCount` and `pin` are unchanged in `applyConfig()`; log heap free before/after with `ESP.getFreeHeap()`
- **RISK-002**: GPIO3 (RX/DMA) conflict with `Serial` breaks debugging during LED operation — Mitigation: Acceptable known tradeoff documented in `config.h`; can switch to `GPIO2` (UART method, slightly lower quality) for debug sessions if needed
- **RISK-003**: `AnimationManager` frame timing interacts poorly with `WiFi.status()` check in `loop()` — Mitigation: `AnimationManager::loop()` already yields every 50ms; verify no blocking calls in new effect dispatch paths
- **RISK-004**: `test_effect` message arrives while `wsClient.pollEvent()` has an unread `GameEvent` — Mitigation: Test effects and game events use separate queues (introduced in Phase 5.2 TASK-006); no collision
- **RISK-005**: Server sends `led_config` with the same values on every state broadcast — Mitigation: `applyConfig()` diffs against current config; no-op if nothing changed (TASK-019)
- **RISK-006**: Phase 5 replaces `StatusLed` but `led.h` is included by `main.cpp` only — low coupling risk, one file change — Mitigation: Verify grep for any other `StatusLed` references before completing TASK-016

- **ASSUMPTION-001**: `GameEventMapper` effect parameters defined in `clients/shared/leds/GameEventMapper.h` already match the PRD §5.2 event → colour mapping (blue pulse for countdown, white blink for score, etc.) and do not need modification
- **ASSUMPTION-002**: `AnimationManager::playEffect()` correctly restores the previous ambient effect after a finite-duration game event effect ends (interrupt-and-restore pattern is implemented)
- **ASSUMPTION-003**: During Phase 5, only the sensor firmware is modified; the motor client is handled in Phase 6 or a separate plan
- **ASSUMPTION-004**: Physical hardware (Wemos D1 Mini + WS2812B strip) is available for manual testing of TESTx tests 007–018

---

## Section 8 — Related Specifications / Further Reading

- [`docs/PRD-LED-Control-Layer.md`](../docs/PRD-LED-Control-Layer.md) — Product requirements: animation engine spec (§4.3), event-driven feedback (§4.4), fallback/diagnostics (§4.6)
- [`docs/led-config-flow.md`](../docs/led-config-flow.md) — WebSocket protocol: `led_config` / `test_effect` message schemas, device registration extension
- [`plan/feature-led-control-phase4-1.md`](feature-led-control-phase4-1.md) — Phase 4 plan (Web Admin + Simulator) — upstream dependency
- [`clients/shared/leds/README.md`](../clients/shared/leds/README.md) — Shared LED library API reference and usage examples
- [`clients/esp8266-sensor/README.md`](../clients/esp8266-sensor/README.md) — Sensor firmware architecture and build instructions
- [`spec/spec-led-control.md`](../spec/spec-led-control.md) — Technical spec: C++ API contracts, WS protocol extensions (§4.4), platform constraints (§3)
- [NeoPixelBus ESP8266 DMA documentation](https://github.com/Makuna/NeoPixelBus/wiki/ESP8266-NeoMethods) — DMA/UART method selection on ESP8266
- [ArduinoWebsockets library](https://github.com/gilmaimon/ArduinoWebsockets) — WS client API used in `websocket.cpp`
