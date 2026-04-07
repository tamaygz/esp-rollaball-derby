---
goal: 'LED Control Layer - Phase 3: Server Configuration & Synchronization'
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: Roll-a-Ball Derby Engineering Team
status: 'Planned'
tags: [feature, server, api, websocket, led, configuration, sync, phase3]
---

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

# Implementation Plan: LED Control Layer - Phase 3

This plan covers Phase 3 (Server Configuration & Synchronization) of the LED Control Layer feature, building upon Phase 1 (Core Abstraction) and Phase 2 (Animation Engine) to deliver server-side LED configuration management, WebSocket protocol extensions, and automatic configuration synchronization to ESP8266/ESP32 devices.

---

## Section 1 — Requirements & Constraints

### Requirements

- **REQ-001**: Extend WebSocket protocol with `led_config` message type for server-to-device configuration broadcast
- **REQ-002**: Add REST API endpoints: `GET /api/leds/config`, `PUT /api/leds/config`, `POST /api/leds/effects/test`
- **REQ-003**: Server stores LED configurations per device type (sensor, motor, display)
- **REQ-004**: Device sends detected LED count and chip type (ESP8266/ESP32) in `register` message payload
- **REQ-005**: Server validates device-reported LED count against admin-configured expected count (±5 LED tolerance)
- **REQ-006**: Auto-sync configuration to all connected devices of matching type on config update
- **REQ-007**: Configuration persists across server restarts (file-based storage: `server/data/led-config.json`)
- **REQ-008**: Support per-device-type configuration overrides (sensor vs motor may have different LED strip sizes)
- **REQ-009**: Effect test endpoint allows admin to preview effects on specific devices before saving
- **REQ-010**: Configuration includes: LED count, topology (strip/matrix/ring), GPIO pin, brightness limits, default effect

### Security Requirements

- **SEC-001**: Validate PUT `/api/leds/config` payload schema (LED count 1-1000, brightness 0-255, valid topology enum)
- **SEC-002**: Reject configuration updates with LED count exceeding platform limits (300 ESP8266, 1000 ESP32)
- **SEC-003**: Effect test endpoint rate-limited to 1 request per second per device to prevent abuse

### Constraints

- **CON-001**: Configuration file must be human-readable JSON for manual editing if needed
- **CON-002**: WebSocket broadcasts must not exceed 1KB payload size (network efficiency)
- **CON-003**: Configuration changes must propagate to devices within 500ms of API call
- **CON-004**: Server must handle devices connecting with mismatched LED counts gracefully (log warning, use device-reported count)
- **CON-005**: Backward compatibility: devices without LED support must continue to function (ignore `led_config` messages)

### Guidelines

- **GUD-001**: Use JSON Schema for configuration validation to ensure consistency
- **GUD-002**: Log all configuration changes to server console with timestamp and source (API/file load)
- **GUD-003**: Provide default configuration if `led-config.json` missing or corrupted (fallback: 10 LEDs, strip topology)
- **GUD-004**: Device-reported LED count takes precedence over admin-configured count unless mismatch tolerance exceeded

### Patterns

- **PAT-001**: Repository pattern for LED configuration persistence (isolate file I/O from business logic)
- **PAT-002**: Observer pattern for configuration change notifications (notify ConnectionManager on config update)
- **PAT-003**: DTO (Data Transfer Object) pattern for WebSocket message payloads

---

## Section 2 — Implementation Steps

### Implementation Phase 3.1 — Data Model & Persistence

- **GOAL-001**: LED configuration data model defined and persisted to `server/data/led-config.json`

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-001 | Create `server/src/config/LedConfigManager.js` with configuration schema definition | | |
| TASK-002 | Define JSON schema for LED config: `{ deviceType, ledCount, topology, gpioPin, brightness, defaultEffect }` | | |
| TASK-003 | Implement `loadConfig()` method to read from `server/data/led-config.json` | | |
| TASK-004 | Implement `saveConfig(config)` method with atomic write (write to temp file, rename) | | |
| TASK-005 | Add default configuration fallback if file missing: `{ sensor: { ledCount: 10, topology: "strip", gpioPin: 4, brightness: 128, defaultEffect: "rainbow" } }` | | |
| TASK-006 | Implement `getConfigForDeviceType(deviceType)` method returning device-specific config | | |
| TASK-007 | Add configuration validation: LED count 1-1000, brightness 0-255, topology in ["strip", "matrix", "ring"] | | |
| TASK-008 | Create `server/data/led-config.json` with initial default configuration | | |

### Implementation Phase 3.2 — REST API Endpoints

- **GOAL-002**: REST API endpoints functional for reading, updating, and testing LED configurations

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-009 | Create `server/src/routes/leds.js` with Express router setup | | |
| TASK-010 | Implement `GET /api/leds/config` endpoint returning all device type configurations | | |
| TASK-011 | Implement `GET /api/leds/config/:deviceType` endpoint returning specific device type config | | |
| TASK-012 | Implement `PUT /api/leds/config/:deviceType` endpoint for updating device type config | | |
| TASK-013 | Add JSON schema validation middleware for PUT requests (validate ledCount, topology, brightness) | | |
| TASK-014 | Implement `POST /api/leds/effects/test` endpoint accepting `{ deviceId, effectName, params }` | | |
| TASK-015 | Add rate limiting to effect test endpoint: 1 request/second per device using `express-rate-limit` | | |
| TASK-016 | Register `/api/leds` routes in `server/src/index.js` | | |
| TASK-017 | Add error handling: 400 for invalid payloads, 404 for unknown device types, 500 for file I/O errors | | |

### Implementation Phase 3.3 — WebSocket Protocol Extension

- **GOAL-003**: WebSocket protocol extended with `led_config` and `test_effect` message types

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-018 | Update `server/README.md` WebSocket protocol documentation with `led_config` message spec | | |
| TASK-019 | Define `led_config` message format: `{ type: "led_config", payload: { ledCount, topology, gpioPin, brightness, defaultEffect } }` | | |
| TASK-020 | Define `test_effect` message format: `{ type: "test_effect", payload: { effectName, params: { color, speed, duration } } }` | | |
| TASK-021 | Modify `ConnectionManager.js`: add `broadcastLedConfig(deviceType, config)` method | | |
| TASK-022 | Implement broadcast filter: only send `led_config` to devices with matching `deviceType` | | |
| TASK-023 | Add `sendTestEffect(deviceId, effectName, params)` method to send effect test to specific device | | |
| TASK-024 | Handle backward compatibility: skip `led_config` broadcast if device protocol version < 2.0 | | |

### Implementation Phase 3.4 — Device Registration Enhancement

- **GOAL-004**: Device registration messages include LED detection data, server validates and responds

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-025 | Update `register` message schema in `server/README.md`: add `ledCount` and `chipType` fields | | |
| TASK-026 | Extend `ConnectionManager.handleRegister()` to extract `ledCount` and `chipType` from payload | | |
| TASK-027 | Store device LED metadata in connection registry: `{ deviceId, deviceType, chipType, reportedLedCount }` | | |
| TASK-028 | Implement validation logic: compare `reportedLedCount` vs `configuredLedCount` with ±5 tolerance | | |
| TASK-029 | If mismatch detected, include `warning` field in `registered` response: `{ type: "registered", payload: { playerId, warning: "LED count mismatch: expected 50, detected 48" } }` | | |
| TASK-030 | If validation passes, auto-send `led_config` message to newly registered device | | |
| TASK-031 | Log device registration with LED metadata to console: `"Device sensor-001 registered: ESP8266, 50 LEDs detected"` | | |

### Implementation Phase 3.5 — Configuration Change Propagation

- **GOAL-005**: Configuration updates via API instantly propagate to all connected devices

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-032 | Wire `LedConfigManager` change events to `ConnectionManager` using EventEmitter pattern | | |
| TASK-033 | On `PUT /api/leds/config/:deviceType`: save config, then trigger `broadcastLedConfig(deviceType, newConfig)` | | |
| TASK-034 | Add timestamp to `led_config` messages: `{ type: "led_config", timestamp: Date.now(), payload: {...} }` | | |
| TASK-035 | Verify broadcast completes within 500ms: add debug timing logs in `ConnectionManager` | | |
| TASK-036 | Handle edge case: if no devices of matching type connected, log info message (not error) | | |

### Implementation Phase 3.6 — Effect Test Integration

- **GOAL-006**: Effect test endpoint triggers visual preview on target devices

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-037 | Implement `POST /api/leds/effects/test` handler in `server/src/routes/leds.js` | | |
| TASK-038 | Validate payload schema: `{ deviceId: string, effectName: string, params: object }` | | |
| TASK-039 | Look up device connection by `deviceId` in `ConnectionManager` registry | | |
| TASK-040 | Send `test_effect` WebSocket message to target device only | | |
| TASK-041 | Return 404 if device not connected, 400 if effectName invalid, 200 on success | | |
| TASK-042 | Add allowed effect names validation: ["solid", "blink", "pulse", "rainbow", "chase", "sparkle"] | | |
| TASK-043 | Log effect test request: `"Effect test: rainbow sent to device sensor-001 by admin"` | | |

### Implementation Phase 3.7 — Documentation & Validation

- **GOAL-007**: API documentation complete, configuration schema documented, end-to-end flow validated

| Task     | Description                                                                 | Completed | Date |
|----------|-----------------------------------------------------------------------------|-----------|------|
| TASK-044 | Update `server/README.md`: add LED configuration API section with endpoint reference | | |
| TASK-045 | Document `led-config.json` schema with field descriptions and example | | |
| TASK-046 | Add example API requests for each endpoint (curl commands) | | |
| TASK-047 | Update WebSocket protocol section with `led_config` and `test_effect` message examples | | |
| TASK-048 | Create `docs/led-config-flow.md` documenting end-to-end configuration sync flow diagram | | |
| TASK-049 | Add JSDoc comments to all `LedConfigManager` and `leds.js` route methods | | |

---

## Section 3 — Alternatives

- **ALT-001**: Store LED config in `GameState.js` instead of separate `LedConfigManager` — Rejected because LED configuration is infrastructure concern, not game state (should persist even when game not running)
- **ALT-002**: Use MongoDB for configuration persistence — Rejected because adds external dependency for simple key-value storage; file-based JSON sufficient for v1
- **ALT-003**: Auto-detect LED count on server via ping/response protocol — Rejected because LED detection requires hardware access, must happen on device side
- **ALT-004**: WebSocket-only configuration (no REST API) — Rejected because admin UI needs RESTful CRUD for configuration management panel
- **ALT-005**: Device-specific config (per deviceId) instead of device-type config — Rejected because increases configuration complexity; device types (sensor/motor) sufficient for v1, can be extended later

---

## Section 4 — Dependencies

- **DEP-001**: Phase 1 (Core Abstraction Layer) completed — `LedController` class must exist on devices
- **DEP-002**: Phase 2 (Animation Engine) completed — Effect names in test endpoint reference Phase 2 effect library
- **DEP-003**: `express` package (already in `server/package.json`)
- **DEP-004**: `express-rate-limit` package for effect test rate limiting (add to `server/package.json`)
- **DEP-005**: `ajv` package for JSON schema validation (add to `server/package.json`)
- **DEP-006**: Node.js built-in `fs/promises` module for async file I/O
- **DEP-007**: `ConnectionManager.js` for WebSocket broadcast infrastructure
- **DEP-008**: `GameState.js` for access to current game state (needed for effect test permission checks)

---

## Section 5 — Files

### Created Files

- **FILE-001**: `server/src/config/LedConfigManager.js` — Created — LED configuration persistence and validation logic
- **FILE-002**: `server/src/routes/leds.js` — Created — REST API endpoints for LED configuration and effect testing
- **FILE-003**: `server/data/led-config.json` — Created — Default LED configuration data file
- **FILE-004**: `docs/led-config-flow.md` — Created — End-to-end configuration synchronization flow diagram

### Modified Files

- **FILE-005**: `server/src/index.js` — Modified — Register `/api/leds` routes
- **FILE-006**: `server/src/ws/ConnectionManager.js` — Modified — Add `broadcastLedConfig()` and `sendTestEffect()` methods, enhance `handleRegister()` for LED metadata
- **FILE-007**: `server/README.md` — Modified — Add LED configuration API documentation and WebSocket protocol updates
- **FILE-008**: `server/package.json` — Modified — Add `express-rate-limit` and `ajv` dependencies
- **FILE-009**: `clients/esp8266-sensor/src/websocket.cpp` — Modified — Add `led_config` and `test_effect` message handlers (device-side implementation)
- **FILE-010**: `clients/esp8266-sensor/src/websocket.h` — Modified — Add message handler function declarations

---

## Section 6 — Testing

### Unit Tests

- **TEST-001**: `LedConfigManager.loadConfig()` successfully loads valid `led-config.json` and returns parsed object
- **TEST-002**: `LedConfigManager.loadConfig()` returns default config when file missing or corrupted
- **TEST-003**: `LedConfigManager.saveConfig()` atomically writes config to file (verify temp file technique)
- **TEST-004**: `LedConfigManager.getConfigForDeviceType("sensor")` returns sensor-specific configuration
- **TEST-005**: Configuration validation rejects LED count > 1000 with clear error message
- **TEST-006**: Configuration validation rejects invalid topology values (accepts only "strip", "matrix", "ring")
- **TEST-007**: `GET /api/leds/config` returns 200 with all device type configurations
- **TEST-008**: `PUT /api/leds/config/sensor` with valid payload returns 200 and updates config file
- **TEST-009**: `PUT /api/leds/config/sensor` with invalid payload returns 400 with validation errors
- **TEST-010**: `POST /api/leds/effects/test` with valid payload sends WebSocket message to target device
- **TEST-011**: `POST /api/leds/effects/test` returns 404 when device not connected
- **TEST-012**: Rate limiting on effect test endpoint: 2nd request within 1 second returns 429
- **TEST-013**: `ConnectionManager.broadcastLedConfig()` sends message only to devices matching deviceType filter

### Integration Tests

- **TEST-014**: Device registers with LED metadata → server validates → sends `led_config` automatically
- **TEST-015**: Device registers with mismatched LED count (expected 50, detected 45) → receives warning in `registered` response
- **TEST-016**: Admin updates sensor LED config via API → all connected sensor devices receive `led_config` message within 500ms
- **TEST-017**: Effect test request → target device receives `test_effect` message with correct parameters
- **TEST-018**: Server restart → LED config persists and is reloaded from `led-config.json`
- **TEST-019**: Invalid config file (malformed JSON) → server logs error, uses default config, continues running

### Manual Tests

- **TEST-020**: Connect ESP8266 sensor with 50 LEDs → verify `led_config` message received via serial console
- **TEST-021**: Update LED config via admin UI (Phase 4 prerequisite) → verify device LEDs reconfigure instantly
- **TEST-022**: Trigger effect test from admin panel → verify target device shows effect animation
- **TEST-023**: Server handles 5 simultaneous devices (3 sensors, 2 motors) with different LED configs correctly

---

## Section 7 — Risks & Assumptions

### Risks

- **RISK-001**: File I/O errors during config save (disk full, permissions) could corrupt configuration
  - *Mitigation*: Use atomic write (temp file + rename), add try-catch with fallback to previous config, log errors prominently
  
- **RISK-002**: Network latency causes devices to receive config updates out of order if rapid changes occur
  - *Mitigation*: Add timestamp to `led_config` messages, devices ignore messages older than current config timestamp

- **RISK-003**: Device might disconnect immediately after registration before receiving `led_config` message
  - *Mitigation*: Check connection status before sending config, log warning if send fails, device will request config on reconnect

- **RISK-004**: Large LED counts (500+ LEDs) may cause WebSocket message size to approach 1KB limit
  - *Mitigation*: Monitor message sizes, compress config payload if needed, verify 1KB limit not exceeded in tests

- **RISK-005**: Effect test flooding: malicious admin spamming test endpoint could disrupt device operation
  - *Mitigation*: Rate limiting (1 req/sec/device), add admin authentication in future phase

### Assumptions

- **ASSUMPTION-001**: Device firmware implements `led_config` and `test_effect` message handlers (coordinated ESP8266 firmware update)
- **ASSUMPTION-002**: Admin UI (Phase 4) will consume these APIs correctly (this phase only provides backend)
- **ASSUMPTION-003**: Device types are known at registration time (sensor/motor/display)
- **ASSUMPTION-004**: LED count auto-detection on device side (Phase 1 feature LED-001) is functional before Phase 3
- **ASSUMPTION-005**: Maximum 16 devices connected simultaneously (server can handle broadcast to all within 500ms)
- **ASSUMPTION-006**: File system is writable (server has permission to create/modify `server/data/led-config.json`)

---

## Section 8 — Related Specifications / Further Reading

- [`plan/feature-led-control-phase1-1.md`](./feature-led-control-phase1-1.md) — Phase 1: Core abstraction layer and hardware setup
- [`plan/feature-led-control-phase2-1.md`](./feature-led-control-phase2-1.md) — Phase 2: Animation engine and effects library
- [`docs/PRD-LED-Control-Layer.md`](../docs/PRD-LED-Control-Layer.md) — Product requirements for full LED control feature
- [`server/README.md`](../server/README.md) — Server architecture and WebSocket protocol documentation
- [Express.js Rate Limiting](https://www.npmjs.com/package/express-rate-limit) — Rate limiting middleware documentation
- [AJV JSON Schema Validator](https://ajv.js.org/) — JSON schema validation library
- [Atomic File Writes](https://github.com/npm/write-file-atomic) — Pattern for safe file persistence
