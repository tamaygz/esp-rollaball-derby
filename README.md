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
| `server/` | ✅ Complete | Tested Node.js server |
| `clients/assets/` | ✅ Complete | Horse + camel SVG themes |
| `clients/web/` | ✅ Complete | Admin SPA at `/admin` |
| `clients/display/` | 🔲 Not started | Pixi.js race visualization — next |
| `clients/esp8266-sensor/` | 🔲 Not started | IR sensor firmware |
| `clients/esp8266-motor/` | ⏳ Deferred | Phase 3 |

## Directory Structure

```
esp-rollaball-derby/
├── server/                   — Node.js game server
│   ├── src/
│   │   ├── index.js          — Express + WS, static mounts
│   │   ├── game/GameState.js — Game logic + state machine
│   │   ├── ws/ConnectionManager.js
│   │   └── routes/           — REST: game, players, health
│   ├── tests/                — 49 tests (Node test runner)
│   └── data/names.txt        — Fun player name pool
│
├── clients/
│   ├── assets/               — Shared SVG sprites + theme manifests
│   │   └── themes/{horse,camel}/
│   ├── web/                  — Vanilla JS admin + test SPA
│   └── display/              — Pixi.js display client (TODO)
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

- Admin panel: `http://localhost:3000/admin`
- Health check: `http://localhost:3000/api/health`
- Asset preview: open `clients/assets/themes/shared/preview.html` in a browser

## Documentation

- [PRD](docs/PRD.md) — Product Requirements Document
- [Progress Log](docs/progress.md) — Session-by-session build history
- [Findings](docs/findings.md) — Research decisions

## Implementation Plans

| Plan | Status |
|------|--------|
| [feature-server-web-1.md](plan/feature-server-web-1.md) | ✅ Completed |
| [feature-client-web-1.md](plan/feature-client-web-1.md) | ✅ Completed |
| [feature-client-display-1.md](plan/feature-client-display-1.md) | 🔲 Planned |
| [feature-client-esp8266-sensor-1.md](plan/feature-client-esp8266-sensor-1.md) | 🔲 Planned |
| [feature-client-esp8266-motor-1.md](plan/feature-client-esp8266-motor-1.md) | ⏳ Deferred |

## Bill of Materials

See [PRD §5](docs/PRD.md) for full hardware list. Core: ESP8266 NodeMCU, IR break-beam sensors, ball chutes, display PC/RPi.

