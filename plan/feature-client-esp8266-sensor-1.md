---
goal: "Implement ESP8266 sensor client firmware — IR break-beam input, score events via WebSocket"
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: "@tamaygz"
status: "Planned"
tags: [feature, firmware, esp8266, arduino, platformio, sensor]
---

# Client ESP8266 Sensor — Implementation Plan

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

PlatformIO project for ESP8266 (Wemos D1 Mini) sensor client firmware. Reads IR break-beam sensors (+1 and +3 holes) and sends score events to the game server via WebSocket.

## 1. Requirements & Constraints

### Functional
- **REQ-001**: Connect to WiFi using WiFiManager captive portal (AP mode on first boot / failed connection)
- **REQ-002**: Connect to game server via WebSocket (server IP/port configurable as WiFiManager custom parameter)
- **REQ-003**: Register on connect: `{ type: "register", payload: { type: "sensor", playerName?: string } }`
- **REQ-004**: Read 2x IR break-beam sensor pins (D1 for +1 hole, D2 for +3 hole) using hardware interrupts
- **REQ-005**: Debounce each sensor independently — minimum 500ms between triggers per sensor using `millis()`
- **REQ-006**: On valid trigger, send score: `{ type: "score", payload: { playerId: "<assigned>", points: 1|3 } }`
- **REQ-007**: Status LED: solid = WebSocket connected, blinking 1Hz = WiFi connected but WS disconnected, fast blink = no WiFi
- **REQ-008**: Auto-reconnect WebSocket with exponential backoff (1s, 2s, 4s, 8s, max 30s)
- **REQ-009**: Non-blocking main loop — WebSocket polling + sensor reads must not starve each other

### Security & Constraints
- **SEC-001**: WiFi credentials stored in ESP flash (WiFiManager default) — not in source code
- **SEC-002**: Validate incoming WebSocket messages (JSON parse, expected fields)
- **CON-001**: PlatformIO project structure with `platformio.ini` for dependency management
- **CON-002**: ESP8266 Arduino framework (Wemos D1 Mini board: `d1_mini`)
- **CON-003**: gilmaimon/ArduinoWebsockets library for WebSocket client
- **CON-004**: WiFiManager (tzapu) for WiFi config with custom parameters (server IP, port, player name)

### Guidelines
- **GUD-001**: `ICACHE_RAM_ATTR` on all ISR functions (ESP8266 requirement for interrupt handlers in RAM)
- **GUD-002**: Keep ISR minimal — set flag only, process in `loop()`
- **PAT-001**: Non-blocking state machine in `loop()`: poll WebSocket → check sensor flags → update LED

## 2. Implementation Steps

### Phase 1: Project Setup & WiFi

- GOAL-001: PlatformIO project, WiFiManager with custom server IP/port parameter, connect to WiFi

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Create `clients/esp8266-sensor/platformio.ini` — board: `d1_mini`, framework: `arduino`, lib_deps: `gilmaimon/ArduinoWebsockets@^0.5.4`, `tzapu/WiFiManager@^2.0.17`, `bblanchon/ArduinoJson@^7.x` | | |
| TASK-002 | Create `clients/esp8266-sensor/src/main.cpp` — `setup()` and `loop()` entry point | | |
| TASK-003 | Implement WiFiManager setup: auto-connect with AP name `"Derby-Sensor-XX"` (XX = chip ID last 4 hex digits) | | |
| TASK-004 | Add WiFiManager custom parameters: `server_ip` (default `192.168.4.1`), `server_port` (default `3000`), `player_name` (default empty) | | |
| TASK-005 | Save custom parameters to LittleFS on WiFiManager save callback; load on boot | | |
| TASK-006 | Implement WiFi connection status checking in `loop()` with reconnect | | |

### Phase 2: WebSocket Connection

- GOAL-002: Connect to game server WebSocket, register, handle messages, auto-reconnect

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | Create `clients/esp8266-sensor/src/websocket.h/.cpp` — WebSocket client wrapper class | | |
| TASK-008 | Implement `connect(serverIp, serverPort)` — connect to `ws://<ip>:<port>/` | | |
| TASK-009 | Implement `onConnect` callback — send register message: `{ type: "register", payload: { type: "sensor", playerName } }` | | |
| TASK-010 | Implement `onMessage` callback — parse JSON, handle `state` message (extract assigned playerId for future score messages) | | |
| TASK-011 | Implement `onDisconnect` callback — set connected flag false, trigger reconnect backoff | | |
| TASK-012 | Implement auto-reconnect: non-blocking exponential backoff timer (1s, 2s, 4s, 8s, 16s, max 30s), reset on success | | |
| TASK-013 | Call `ws.poll()` in main `loop()` for non-blocking WebSocket operation | | |

### Phase 3: IR Sensor Reading & Debounce

- GOAL-003: Read IR break-beam sensors with hardware interrupts, debounce, send score events

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-014 | Create `clients/esp8266-sensor/src/sensors.h/.cpp` — sensor reading and debounce logic | | |
| TASK-015 | Configure D1 (GPIO5) as INPUT_PULLUP for +1 hole sensor; D2 (GPIO4) for +3 hole sensor | | |
| TASK-016 | Attach `FALLING` edge interrupts on both pins with `ICACHE_RAM_ATTR` ISR functions | | |
| TASK-017 | ISR implementation: `millis()` debounce check (500ms), set volatile `triggered` flag if valid | | |
| TASK-018 | In `loop()`: check triggered flags, if set → build JSON score message → send via WebSocket → clear flag | | |
| TASK-019 | Score message format: `{ type: "score", payload: { playerId: "<id>", points: 1 } }` (or 3 for D2 sensor) | | |
| TASK-020 | Use assigned `playerId` from server's state response (received after register) | | |

### Phase 4: Status LED & Diagnostics

- GOAL-004: Visual status indication via built-in LED, serial debug output

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-021 | Implement LED state machine: NO_WIFI (fast blink 5Hz) → WIFI_OK (slow blink 1Hz) → WS_CONNECTED (solid on) | | |
| TASK-022 | Non-blocking LED blink using `millis()` timer (no `delay()`) | | |
| TASK-023 | Serial debug output: WiFi status, WS connection status, sensor triggers, sent messages | | |
| TASK-024 | Compile, upload, and test with serial monitor | | |

## 3. Alternatives

- **ALT-001**: ESPHome YAML firmware — rejected; custom WebSocket protocol not supported natively, latency from HA hop (PRD §3.6)
- **ALT-002**: Links2004/arduinoWebSockets — rejected in favor of gilmaimon/ArduinoWebsockets for modern API and cleaner callback pattern
- **ALT-003**: Arduino IDE instead of PlatformIO — rejected; PlatformIO provides dependency management (`lib_deps`), multi-env support, VSCode integration, reproducible builds
- **ALT-004**: Polling sensors instead of interrupts — rejected; interrupts are more responsive and don't waste CPU cycles in tight loop polling

## 4. Dependencies

- **DEP-001**: `gilmaimon/ArduinoWebsockets` ^0.5.4 — WebSocket client (RFC-6455, ESP8266 support)
- **DEP-002**: `tzapu/WiFiManager` ^2.0.17 — WiFi configuration with captive portal
- **DEP-003**: `bblanchon/ArduinoJson` ^7.x — JSON serialization/deserialization
- **DEP-004**: PlatformIO Core CLI or VSCode PlatformIO extension
- **DEP-005**: ESP8266 Arduino framework (platform: `espressif8266`)
- **DEP-006**: Game server WebSocket endpoint (from server-web plan)

## 5. Files

- **FILE-001**: `clients/esp8266-sensor/platformio.ini` — PlatformIO config, board, dependencies
- **FILE-002**: `clients/esp8266-sensor/src/main.cpp` — setup/loop, orchestrates WiFi, WS, sensors, LED
- **FILE-003**: `clients/esp8266-sensor/src/websocket.h` / `websocket.cpp` — WebSocket client wrapper
- **FILE-004**: `clients/esp8266-sensor/src/sensors.h` / `sensors.cpp` — IR sensor reading + debounce
- **FILE-005**: `clients/esp8266-sensor/src/led.h` / `led.cpp` — status LED state machine
- **FILE-006**: `clients/esp8266-sensor/src/config.h` — pin definitions, default server IP/port, debounce interval

## 6. Testing

- **TEST-001**: WiFiManager — first boot → AP mode appears → configure WiFi + server IP → connects
- **TEST-002**: WiFiManager — subsequent boot → auto-connects to saved WiFi
- **TEST-003**: WebSocket connect — connects to server, sends register, receives state with assigned playerId
- **TEST-004**: WebSocket reconnect — kill server, verify backoff retries, restart server, verify reconnects
- **TEST-005**: Sensor trigger — break IR beam on D1 → serial shows trigger → WS score message sent with points=1
- **TEST-006**: Sensor trigger — break IR beam on D2 → serial shows trigger → WS score message sent with points=3
- **TEST-007**: Debounce — rapid beam breaks within 500ms → only first trigger counted
- **TEST-008**: LED states — verify no WiFi=fast blink, WiFi only=slow blink, WS connected=solid
- **TEST-009**: Non-blocking — WebSocket messages received while sensor debounce timer active

## 7. Risks & Assumptions

- **RISK-001**: WiFiManager custom parameters may not persist correctly on all ESP8266 flash sizes — mitigated by using LittleFS (verified for 4MB flash on Wemos D1 Mini)
- **RISK-002**: `millis()` overflow after ~49 days — mitigated; events use `millis()-lastTrigger > debounceMs` arithmetic which handles overflow correctly for unsigned long
- **RISK-003**: IR sensor false triggers from ambient light — mitigated by using modulated IR break-beam sensors (38kHz) and physical light shielding around holes
- **ASSUMPTION-001**: Wemos D1 Mini with ESP8266 (4MB flash, 80MHz) as target board
- **ASSUMPTION-002**: IR break-beam sensors output digital LOW when beam broken (active LOW)
- **ASSUMPTION-003**: Server and ESP8266 on same WiFi network with <50ms latency
- **ASSUMPTION-004**: PlatformIO installed on development machine (VSCode extension or CLI)

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Server Web Plan](feature-server-web-1.md)
- [Motor Controller Plan](feature-client-esp8266-motor-1.md)
- [Findings & Decisions](../docs/findings.md)
- [gilmaimon/ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/)
- [ESP8266 GPIO interrupts](https://lastminuteengineers.com/handling-esp8266-gpio-interrupts-tutorial/)
- [Adafruit IR break-beam tutorial](https://learn.adafruit.com/ir-breakbeam-sensors/arduino)
- [PlatformIO ESP8266 D1 Mini](https://docs.platformio.org/en/stable/boards/espressif8266/d1_mini.html)
