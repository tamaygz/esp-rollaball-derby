# Roll-a-Ball Derby — Game Server

Node.js + Express + `ws` game server. Central hub for all clients.

## Stack

- **Node.js** 20+ (LTS)
- **Express** 4 — REST API + static file serving
- **ws** 8 — raw WebSocket server (RFC 6455)

## Quick Start

```bash
cd server
npm install
npm start          # production
npm run dev        # watch mode (node --watch)
npm test           # run all tests
```

Server starts on **http://localhost:3000** (override with `PORT` env var). Copy `.env.example` to `.env` to configure.

## Routes

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/health` | Health check |
| GET | `/api/game` | Current game state |
| POST | `/api/game/start` | Start the race |
| POST | `/api/game/pause` | Pause / resume |
| POST | `/api/game/reset` | Reset to idle |
| PUT | `/api/game/config` | Update config (idle only) |
| GET | `/api/players` | List all players |
| PUT | `/api/players/:id` | Rename a player |
| DELETE | `/api/players/:id` | Remove a player |
| GET | `/api/clients` | List connected clients |
| DELETE | `/api/clients/:id` | Disconnect a client |

Static mounts:
- `/admin` → `../clients/web/` — admin SPA
- `/assets` → `../clients/assets/` — shared game assets
- `/` → `public/` — display SPA (future)

## WebSocket Protocol

Connect to `ws://localhost:3000`. All messages are JSON `{ type, payload }`.

| `type` | Direction | Description |
|--------|-----------|-------------|
| `register` | client→server | Register with `{ type: "web"\|"sensor"\|"display"\|"motor", playerName?, playerId? }`. Include `playerId` to reconnect to an existing player session. |
| `registered` | server→client | Confirms registration with `{ id, name, playerType }` |
| `score` | client→server | `{ playerId, points }` — points must be 1, 2, or 3 |
| `state` | server→broadcast | Full game state snapshot |
| `scored` | server→broadcast | `{ playerName, points, newPosition }` |
| `winner` | server→broadcast | `{ name, id }` |
| `error` | server→client | `{ message }` |

## Game State Machine

```
idle ──start──► running ──pause──► paused
  ▲               │ └────resume────┘
  └───reset───────┘ └──winner──► finished ──reset──► idle
```

Config fields (`trackLength`, `maxPlayers`, `theme`) can only be changed in `idle` state.

## Tests

```
server/tests/
├── gameState.test.js         — state machine, scoring, rate-limiting, config, names, auto-theme
├── connectionManager.test.js — WS hub, routing, broadcasts, disconnects, reconnect
└── integration.test.js       — full HTTP+WS lifecycle, REST endpoints, clients API
```

Run: `npm test` — all tests should pass.
