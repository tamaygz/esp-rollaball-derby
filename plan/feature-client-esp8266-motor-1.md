---
goal: "Implement ESP8266 motor controller firmware — receive positions via WebSocket, drive stepper motors"
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: "@tamaygz"
status: "Planned"
tags: [feature, firmware, esp8266, arduino, platformio, motor, stepper]
---

# Client ESP8266 Motor Controller — Implementation Plan

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

PlatformIO project for ESP8266 (Wemos D1 Mini) motor controller firmware. Receives player position updates from the game server via WebSocket and drives stepper motors to move physical player figures on the board. Targeted for Phase 3 / v1.5 of the roadmap.

## 1. Requirements & Constraints

### Functional
- **REQ-001**: Connect to WiFi using WiFiManager captive portal (AP mode on first boot / failed connection)
- **REQ-002**: Connect to game server via WebSocket, register as `{ type: "register", payload: { type: "motor" } }`
- **REQ-003**: Receive `positions` messages: `{ type: "positions", payload: { players: [{ id, position, maxPosition }] } }` from server
- **REQ-004**: Map each player to a stepper motor; calculate target step position proportionally
- **REQ-005**: Drive stepper motors non-blocking using AccelStepper library (smooth acceleration/deceleration)
- **REQ-006**: On game reset (all positions = 0), home all motors (return to start)
- **REQ-007**: Homing sequence on boot using limit switches (one per lane)
- **REQ-008**: Motor type to be determined (deferred) — design firmware to abstract motor interface
- **REQ-009**: Status LED: solid = WebSocket connected, blinking 1Hz = WiFi connected but WS disconnected, fast blink = no WiFi
- **REQ-010**: Auto-reconnect WebSocket with exponential backoff (1s, 2s, 4s, 8s, max 30s)

### Security & Constraints
- **SEC-001**: WiFi credentials stored in ESP flash (WiFiManager default) — not in source code
- **SEC-002**: Validate incoming WebSocket messages (JSON parse, expected fields)
- **CON-001**: PlatformIO project structure with `platformio.ini` for dependency management
- **CON-002**: ESP8266 Arduino framework (Wemos D1 Mini board: `d1_mini`)
- **CON-003**: gilmaimon/ArduinoWebsockets library for WebSocket client
- **CON-004**: WiFiManager (tzapu) for WiFi config with custom parameters (server IP, port)
- **CON-005**: AccelStepper library for non-blocking stepper control
- **CON-006**: Adafruit MCP23017 library for I²C GPIO expansion (>4 motors)

### Guidelines
- **PAT-001**: Non-blocking state machine in `loop()`: poll WebSocket → run steppers → update LED
- **PAT-002**: Abstract motor interface to allow swapping motor types without rewriting position logic

## 2. Implementation Steps

### Phase 1: Project Setup & WiFi

- GOAL-001: PlatformIO project, WiFiManager with custom server IP/port parameter, connect to WiFi

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Create `clients/esp8266-motor/platformio.ini` — board: `d1_mini`, framework: `arduino`, lib_deps: `gilmaimon/ArduinoWebsockets@^0.5.4`, `tzapu/WiFiManager@^2.0.17`, `bblanchon/ArduinoJson@^7.x`, `waspinator/AccelStepper@^1.64` | | |
| TASK-002 | Create `clients/esp8266-motor/src/main.cpp` — `setup()` and `loop()` entry point | | |
| TASK-003 | Implement WiFiManager setup: auto-connect with AP name `"Derby-Motor-XX"` (XX = chip ID last 4 hex digits) | | |
| TASK-004 | Add WiFiManager custom parameters: `server_ip` (default `192.168.4.1`), `server_port` (default `3000`) | | |
| TASK-005 | Save custom parameters to LittleFS on WiFiManager save callback; load on boot | | |
| TASK-006 | Implement WiFi connection status checking in `loop()` with reconnect | | |

### Phase 2: WebSocket Connection

- GOAL-002: Connect to game server WebSocket, register as motor, receive position updates

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | Create `clients/esp8266-motor/src/websocket.h/.cpp` — WebSocket client wrapper class | | |
| TASK-008 | Implement `connect(serverIp, serverPort)` — connect to `ws://<ip>:<port>/` | | |
| TASK-009 | Implement `onConnect` callback — send register message: `{ type: "register", payload: { type: "motor" } }` | | |
| TASK-010 | Implement `onMessage` callback — parse JSON, handle `positions` message, extract player positions array | | |
| TASK-011 | Implement `onDisconnect` callback — set connected flag false, trigger reconnect backoff | | |
| TASK-012 | Implement auto-reconnect: non-blocking exponential backoff timer (1s, 2s, 4s, 8s, 16s, max 30s), reset on success | | |
| TASK-013 | Call `ws.poll()` in main `loop()` for non-blocking WebSocket operation | | |

### Phase 3: Motor Interface & Stepper Control

- GOAL-003: Abstract motor control, map positions to steps, drive steppers non-blocking

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-014 | Create `clients/esp8266-motor/src/motor_controller.h` — abstract base class with `moveTo(targetSteps)`, `home()`, `update()`, `isHomed()` | | |
| TASK-015 | Create `clients/esp8266-motor/src/stepper_controller.h/.cpp` — AccelStepper implementation of motor_controller | | |
| TASK-016 | Configure AccelStepper: motor type (FULL4WIRE for 28BYJ-48 or DRIVER for A4988/DRV8825), speed, acceleration | | |
| TASK-017 | Implement position mapping: `targetSteps = (position / maxPosition) * totalTrackSteps` | | |
| TASK-018 | Call `stepper.run()` in `loop()` for non-blocking motor movement (AccelStepper pattern) | | |
| TASK-019 | Create `clients/esp8266-motor/src/config.h` — pin map, totalTrackSteps, speed, acceleration, number of players/motors | | |
| TASK-020 | Handle `positions` WebSocket message: iterate player array → update target for each motor | | |

### Phase 4: Homing & Reset

- GOAL-004: Homing sequence on boot and on game reset

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-021 | Configure limit switch pins (one per lane) as INPUT_PULLUP | | |
| TASK-022 | Implement homing sequence: move motor slowly in reverse until limit switch triggers, set position to 0 | | |
| TASK-023 | Run homing for all motors sequentially on boot (before accepting WS positions) | | |
| TASK-024 | Detect game reset: if all positions = 0 in `positions` message → trigger home sequence | | |
| TASK-025 | Homing state machine: IDLE → HOMING → HOMED → RUNNING (non-blocking) | | |

### Phase 5: Status LED & Diagnostics

- GOAL-005: Visual status indication via built-in LED, serial debug output

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-026 | Implement LED state machine: NO_WIFI (fast blink 5Hz) → WIFI_OK (slow blink 1Hz) → WS_CONNECTED (solid on) | | |
| TASK-027 | Non-blocking LED blink using `millis()` timer (no `delay()`) | | |
| TASK-028 | Serial debug output: WiFi status, WS messages, motor positions, homing progress | | |
| TASK-029 | Compile, upload, and test with serial monitor | | |

### Phase 6: GPIO Expansion (multi-motor)

- GOAL-006: Support more than 4 motors via I²C GPIO expander

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-030 | Add `adafruit/Adafruit MCP23017 Arduino Library@^2.3` to lib_deps | | |
| TASK-031 | Implement MCP23017 initialization and pin configuration over I²C (SDA=D2, SCL=D1) | | |
| TASK-032 | Extend `StepperController` to accept MCP23017 pin references for motor STEP/DIR signals | | |
| TASK-033 | Test with 1 motor on direct GPIO, then 1 motor on MCP23017, then mixed setup | | |

## 3. Alternatives

- **ALT-001**: ESPHome YAML firmware — rejected; custom WebSocket protocol not supported natively, latency from HA hop (PRD §3.6)
- **ALT-002**: Links2004/arduinoWebSockets — rejected in favor of gilmaimon/ArduinoWebsockets for modern API and cleaner callback pattern
- **ALT-003**: Arduino IDE instead of PlatformIO — rejected; PlatformIO provides dependency management (`lib_deps`), multi-env support, VSCode integration, reproducible builds
- **ALT-004**: Single firmware for both sensor and motor — rejected; different hardware, different concerns, separate projects are cleaner
- **ALT-005**: Servo motors instead of steppers — deferred; servos simpler but less precise for linear track movement. Decision pending hardware prototyping.

## 4. Dependencies

- **DEP-001**: `gilmaimon/ArduinoWebsockets` ^0.5.4 — WebSocket client (RFC-6455, ESP8266 support)
- **DEP-002**: `tzapu/WiFiManager` ^2.0.17 — WiFi configuration with captive portal
- **DEP-003**: `bblanchon/ArduinoJson` ^7.x — JSON serialization/deserialization
- **DEP-004**: `waspinator/AccelStepper` ^1.64 — non-blocking stepper motor control
- **DEP-005**: `adafruit/Adafruit MCP23017 Arduino Library` ^2.3 — I²C GPIO expander (Phase 6)
- **DEP-006**: PlatformIO Core CLI or VSCode PlatformIO extension
- **DEP-007**: ESP8266 Arduino framework (platform: `espressif8266`)
- **DEP-008**: Game server WebSocket endpoint (from server-web plan)
- **DEP-009**: Stepper motor + driver hardware (type TBD — likely 28BYJ-48 + ULN2003 or NEMA 17 + A4988)

## 5. Files

- **FILE-001**: `clients/esp8266-motor/platformio.ini` — PlatformIO config, board, dependencies
- **FILE-002**: `clients/esp8266-motor/src/main.cpp` — setup/loop, orchestrates WiFi, WS, motors, LED
- **FILE-003**: `clients/esp8266-motor/src/websocket.h` / `websocket.cpp` — WebSocket client wrapper
- **FILE-004**: `clients/esp8266-motor/src/motor_controller.h` — abstract motor interface
- **FILE-005**: `clients/esp8266-motor/src/stepper_controller.h` / `stepper_controller.cpp` — AccelStepper implementation
- **FILE-006**: `clients/esp8266-motor/src/led.h` / `led.cpp` — status LED state machine
- **FILE-007**: `clients/esp8266-motor/src/config.h` — pin map, motor steps/rev, track length in steps, speed, acceleration

## 6. Testing

- **TEST-001**: WiFiManager — first boot → AP mode appears → configure WiFi + server IP → connects
- **TEST-002**: WiFiManager — subsequent boot → auto-connects to saved WiFi
- **TEST-003**: WebSocket connect — connects, registers as "motor", receives positions messages
- **TEST-004**: WebSocket reconnect — kill server, verify backoff retries, restart server, verify reconnects
- **TEST-005**: Motor movement — send position update via server, motor moves to correct proportional position
- **TEST-006**: Smooth motion — AccelStepper acceleration/deceleration visible (no sudden stops)
- **TEST-007**: Multiple motors — positions message with multiple players, each motor moves independently
- **TEST-008**: Game reset — all positions=0 received, all motors return to home position
- **TEST-009**: Homing on boot — motors find limit switch and calibrate zero position
- **TEST-010**: LED states — verify no WiFi=fast blink, WiFi only=slow blink, WS connected=solid
- **TEST-011**: MCP23017 — motor on expander GPIO responds same as motor on direct GPIO

## 7. Risks & Assumptions

- **RISK-001**: WiFiManager custom parameters may not persist correctly on all ESP8266 flash sizes — mitigated by using LittleFS (verified for 4MB flash on Wemos D1 Mini)
- **RISK-002**: AccelStepper `run()` call frequency may be impacted by WebSocket polling — mitigated by keeping `loop()` fast, measuring max loop time in dev
- **RISK-003**: MCP23017 I²C latency may limit stepper step rate — acceptable for 28BYJ-48 (~15 RPM); not suitable for high-speed NEMA 17
- **RISK-004**: Motor type decision deferred — firmware abstracts interface, but physical mounting and wiring will depend on motor choice
- **RISK-005**: Limit switch wiring for many lanes may exhaust GPIO — mitigated by MCP23017 expansion or multiplexing
- **ASSUMPTION-001**: Wemos D1 Mini with ESP8266 (4MB flash, 80MHz) as target board
- **ASSUMPTION-002**: Stepper motor + driver TBD (likely 28BYJ-48 + ULN2003)
- **ASSUMPTION-003**: Server and ESP8266 on same WiFi network with <50ms latency
- **ASSUMPTION-004**: PlatformIO installed on development machine (VSCode extension or CLI)
- **ASSUMPTION-005**: One ESP8266 motor controller handles all player motors (up to 16 with GPIO expansion)

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Server Web Plan](feature-server-web-1.md)
- [Sensor Client Plan](feature-client-esp8266-sensor-1.md)
- [Findings & Decisions](../docs/findings.md)
- [gilmaimon/ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/)
- [AccelStepper docs](https://www.airspayce.com/mikem/arduino/AccelStepper/)
- [Adafruit MCP23017](https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library)
- [PlatformIO ESP8266 D1 Mini](https://docs.platformio.org/en/stable/boards/espressif8266/d1_mini.html)
- [RandomNerdTutorials: ESP8266 stepper + WebSocket](https://randomnerdtutorials.com/stepper-motor-esp8266-websocket/)
