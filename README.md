# Roll-a-Ball Derby

A local-network physical race game. Players roll balls past IR sensors; an ESP8266 detects each pass and sends a score event to a central Node.js server. A TV/beamer display shows the live race. A web panel lets the game host control the session.

## System Architecture

```
ESP8266 Sensors ─────┐
                      ├──► Node.js Server ──► Display Client (Pixi.js, TV/beamer)
ESP8266 Motors ──────┘         │
                               └──► Web Admin Client (browser)
```

All communication over WebSocket (`ws://`). The server is the single source of truth.

## Current Status

| Component | Status | Notes |
|-----------|--------|-------|
| `server/` | ✅ Complete | 101 tests passing (GameState, ConnectionManager, BotManager, integration) |
| `clients/assets/` | ✅ Complete | Horse + camel SVG themes |
| `clients/web/` | ✅ Complete | Admin SPA at `/admin` — game controls, config, bots, score testing |
| `clients/display/` | ✅ Complete | Pixi.js race visualization at `/display` with action effects |
| `clients/esp8266-sensor/` | 🚧 In progress | PlatformIO, WiFi, WebSocket, IR sensors via interrupts, status LED, web flashing |
| `clients/esp8266-motor/` | ⏳ Deferred | Phase 3 |

## Key Features

- **Scoring**: 0 / +1 / +2 / +3 point rolls with streak tracking and rank-change events
- **Server-side bots**: Autonomous bot players via REST API — roll at 2–8 s human-like intervals
- **Action effects**: 8 visual event types (zero_roll, score_1/2/3, streak_zero_3x, streak_three_2x, took_lead, became_last)
- **Theming**: Horse 🐎 and camel 🐪 themes with auto-random selection
- **Reconnect**: Both admin and display clients auto-reconnect with exponential backoff
- **mDNS autodiscovery**: Server publishes `_derby._tcp` via DNS-SD; ESP8266 sensors find the server automatically on the LAN
- **Rate limiting**: 300 ms per player to prevent spam

## Directory Structure

```
esp-rollaball-derby/
├── server/                   — Node.js game server
│   ├── src/
│   │   ├── index.js          — Express + WS, static mounts
│   │   ├── game/
│   │   │   ├── GameState.js  — Game logic + state machine
│   │   │   └── BotManager.js — Server-side autonomous bot players
│   │   ├── ws/ConnectionManager.js — WebSocket hub + broadcasts
│   │   └── routes/           — REST: game, players, bots, clients, health
│   ├── tests/                — 101 tests (Node test runner)
│   └── data/names.txt        — Fun player name pool
│
├── clients/
│   ├── assets/               — Shared SVG sprites + theme manifests
│   │   └── themes/{horse,camel}/
│   ├── web/                  — Vanilla JS admin + test SPA
│   └── display/              — Pixi.js display client (beamer/TV)
│       └── js/effects/       — ActionEffect, ScoringEffect, WinnerOverlay
│
├── docs/                     — PRD, findings, progress log
└── plan/                     — Implementation plan files
```

## Quick Start

```bash
cd server
npm install
npm start
```

- Admin panel: `http://localhost:3000/admin` (or `http://derby-server.local:3000/admin`)
- Display (TV): `http://localhost:3000/display/` (add `?fullscreen=1` for auto-fullscreen)
- Health check: `http://localhost:3000/api/health`
- Asset preview: open `clients/assets/themes/shared/preview.html` in a browser

The server advertises itself via mDNS as `_derby._tcp`. ESP8266 sensors auto-discover the server IP and port on the same LAN — no manual IP entry required.

## Documentation

- [PRD](docs/PRD.md) — Product Requirements Document
- [Progress Log](docs/progress.md) — Session-by-session build history
- [Findings](docs/findings.md) — Research decisions

## Implementation Plans

| Plan | Status |
|------|--------|
| [feature-server-web-1.md](plan/feature-server-web-1.md) | ✅ Completed |
| [feature-client-web-1.md](plan/feature-client-web-1.md) | ✅ Completed |
| [feature-client-display-1.md](plan/feature-client-display-1.md) | ✅ Completed |
| [feature-client-esp8266-sensor-1.md](plan/feature-client-esp8266-sensor-1.md) | 🔲 Planned |
| [feature-client-esp8266-motor-1.md](plan/feature-client-esp8266-motor-1.md) | ⏳ Deferred |

## Bill of Materials

See [PRD §5](docs/PRD.md) for full hardware list. Core: ESP8266 NodeMCU, IR break-beam sensors, ball chutes, display PC/RPi.

