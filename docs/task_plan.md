# Task Plan: Roll-a-Ball Derby — Architecture & Tech Stack Research

## Goal
Resolve all open decisions in the PRD (server stack, frontend framework, ESP8266 libraries, motor control, WiFi config) through systematic research, then produce a finalized architecture document ready for implementation planning.

## Current Phase
Phase 7: ESP8266 Sensor Client Implementation (Phases 1–3 complete)

## Phases

### Phase 1: Server Stack Research
- [x] Research Node.js + Express + `ws` for WebSocket game servers
- [x] Research Python + FastAPI + WebSockets for game servers
- [x] Compare: latency, simplicity, ESP8266 compat, community examples
- [x] Document findings
- **Status:** complete

### Phase 2: ESP8266 Client Research
- [x] Research Arduino WebSocket client libraries for ESP8266
- [x] Research WiFiManager vs hardcoded WiFi config
- [x] Research IR break-beam sensor best practices / debounce patterns
- [x] Research OTA update options (ArduinoOTA)
- [x] Document findings
- **Status:** complete

### Phase 3: Frontend Display Research
- [x] Research Pixi.js for 2D game rendering (race track)
- [x] Research Phaser.js for 2D game rendering
- [x] Research vanilla Canvas + CSS animations approach
- [x] Evaluate theming/sprite support for each option
- [x] Document findings
- **Status:** complete

### Phase 4: Motor Controller Research
- [x] Research stepper motor control on ESP8266 (AccelStepper, etc.)
- [x] Research I²C GPIO expanders (MCP23017) with ESP8266
- [x] Research servo vs stepper tradeoffs for linear positioning
- [x] Document findings
- **Status:** complete

### Phase 5: User Decisions
- [x] Present research summary with recommendations
- [x] Ask user for final decisions on each open question
- [x] Document decisions
- **Status:** complete

### Phase 6: Finalize Architecture
- [x] Update PRD with all resolved decisions
- [x] Create implementation plans: server-web, client-display, client-web, esp8266-sensor, esp8266-motor
- **Status:** complete

### Phase 7: ESP8266 Sensor Client Implementation
- [x] Phase 1: Project setup, WiFi config, WiFiManager integration
- [x] Phase 2: WebSocket client, reconnection backoff, message routing
- [x] Phase 3: IR sensor ISR handlers, debounce, score event transmission
- [ ] Phase 4: Testing & field validation (stress tests, WiFi dropout, edge cases)
- **Status:** in progress (phases 1–3 complete)

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
