# Findings & Decisions — Roll-a-Ball Derby

## Requirements (from PRD)
- Self-hosted, local-network, 1–16 players
- Server web app: game logic + display + admin
- ESP8266 sensor clients: IR break-beam, WebSocket, debounce
- ESP8266 motor controller: stepper/servo, WebSocket, I²C expander
- Web test client: buttons for simulated rolls
- Display: horizontal stacked lanes, themeable (horse/camel), beamer/TV
- ≤300ms sensor→display latency, ≤500ms state→motor latency
- Player naming: optional from client, random from names.txt

## Open Decisions (from PRD §6)
| # | Decision | Options | Status |
|---|----------|---------|--------|
| 1 | Server stack | Node.js + WS vs Python + FastAPI | Researching |
| 2 | Frontend framework | Canvas vs React/Svelte vs Pixi.js/Phaser | Researching |
| 3 | Motor type | 28BYJ-48 vs NEMA 17 vs Servos | Researching |
| 4 | WiFi config | Hardcoded vs WiFiManager AP portal | Researching |

## Research Findings

### Server Stack
<!-- To be filled during Phase 1 -->

### ESP8266 Libraries
<!-- To be filled during Phase 2 -->

### Frontend Display
<!-- To be filled during Phase 3 -->

### Motor Control
<!-- To be filled during Phase 4 -->

## Technical Decisions
| Decision | Rationale |
|----------|-----------|

## Issues Encountered
| Issue | Resolution |
|-------|------------|

## Resources
- PRD: docs/PRD.md
