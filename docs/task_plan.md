# Task Plan: Roll-a-Ball Derby — Architecture & Tech Stack Research

## Goal
Resolve all open decisions in the PRD (server stack, frontend framework, ESP8266 libraries, motor control, WiFi config) through systematic research, then produce a finalized architecture document ready for implementation planning.

## Current Phase
Phase 1

## Phases

### Phase 1: Server Stack Research
- [ ] Research Node.js + Express + `ws` for WebSocket game servers
- [ ] Research Python + FastAPI + WebSockets for game servers
- [ ] Compare: latency, simplicity, ESP8266 compat, community examples
- [ ] Document findings
- **Status:** in_progress

### Phase 2: ESP8266 Client Research
- [ ] Research Arduino WebSocket client libraries for ESP8266
- [ ] Research WiFiManager vs hardcoded WiFi config
- [ ] Research IR break-beam sensor best practices / debounce patterns
- [ ] Research OTA update options (ArduinoOTA)
- [ ] Document findings
- **Status:** pending

### Phase 3: Frontend Display Research
- [ ] Research Pixi.js for 2D game rendering (race track)
- [ ] Research Phaser.js for 2D game rendering
- [ ] Research vanilla Canvas + CSS animations approach
- [ ] Evaluate theming/sprite support for each option
- [ ] Document findings
- **Status:** pending

### Phase 4: Motor Controller Research
- [ ] Research stepper motor control on ESP8266 (AccelStepper, etc.)
- [ ] Research I²C GPIO expanders (MCP23017) with ESP8266
- [ ] Research servo vs stepper tradeoffs for linear positioning
- [ ] Document findings
- **Status:** pending

### Phase 5: User Decisions
- [ ] Present research summary with recommendations
- [ ] Ask user for final decisions on each open question
- [ ] Document decisions
- **Status:** pending

### Phase 6: Finalize Architecture
- [ ] Update PRD with all resolved decisions
- [ ] Create final architecture summary
- **Status:** pending

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
