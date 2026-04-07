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
| GET | `/api/game` | Current game state + client counts |
| POST | `/api/game/start` | Start the race |
| POST | `/api/game/pause` | Pause / resume |
| POST | `/api/game/reset` | Reset to idle |
| PUT | `/api/game/config` | Update config (idle only) |
| GET | `/api/players` | List all players |
| PUT | `/api/players/:id` | Rename a player |
| DELETE | `/api/players/:id` | Remove / disconnect a player |
| GET | `/api/bots` | List active server-side bots |
| POST | `/api/bots` | Create a new autonomous bot player |
| DELETE | `/api/bots/:id` | Remove a bot and its player |
| GET | `/api/clients` | List connected WebSocket clients |
| DELETE | `/api/clients/:id` | Kick a WebSocket client |

Static mounts:
- `/admin` → `../clients/web/` — admin SPA
- `/display` → `../clients/display/` — display client (beamer/TV)
- `/assets` → `../clients/assets/` — shared game assets

## WebSocket Protocol

Connect to `ws://localhost:3000`. All messages are JSON `{ type, payload }`.

| `type` | Direction | Description |
|--------|-----------|-------------|
| `register` | client→server | Register with `{ type: "web"\|"sensor"\|"display"\|"motor", playerName?, playerId? }` |
| `registered` | server→client | Confirms registration with `{ id, name, playerType }` |
| `score` | client→server | `{ playerId, points }` — points: 0, 1, 2, or 3 |
| `state` | server→broadcast | Full game state snapshot |
| `scored` | server→broadcast | `{ playerId, playerName, points, newPosition, events }` |
| `positions` | server→motor | `{ players: [{ id, position, maxPosition }] }` |
| `winner` | server→broadcast | `{ playerId, name }` |
| `error` | server→client | `{ message }` |

### Events array

Every `scored` message includes an `events[]` array with zero or more of:

| Event | Meaning |
|-------|---------|
| `zero_roll` | Rolled 0 points |
| `score_1` | Scored +1 |
| `score_2` | Scored +2 |
| `score_3` | Scored +3 |
| `streak_zero_3x` | 3+ consecutive zeros |
| `streak_three_2x` | 2+ consecutive +3 rolls |
| `took_lead` | Player just overtook all others |
| `became_last` | Player just dropped to last place |

## Server-Side Bots

The `BotManager` creates autonomous bot players that score at random 2–8 s intervals while the game is running. Bots:

- Are created via `POST /api/bots` (no WebSocket connection needed)
- Get auto-assigned player names and type `'bot'`
- Roll with human-like probability: ~10.9% +3, ~14.9% +2, ~29.7% +1, ~44.6% +0
- Auto-start/stop with game state transitions (start, pause, reset)
- Generate the full `events[]` pipeline (streaks, rank changes)

## Game State Machine

```
idle ──start──► running ──pause──► paused
  ▲               │ └────resume────┘
  └───reset───────┘ └──winner──► finished ──reset──► idle
```

Config fields (`trackLength`, `maxPlayers`, `theme`) can only be changed in `idle` state. Theme `'auto'` resolves to a random concrete theme (horse/camel) at game start.

## Tests

```
server/tests/
├── botManager.test.js        (19 tests — bot lifecycle, scoring, game hooks)
├── gameState.test.js         (37 tests — state machine, scoring, rate-limiting, names, streaks, ranks)
├── connectionManager.test.js (27 tests — WS hub, routing, broadcasts, reconnect, disconnects)
└── integration.test.js       (18 tests — HTTP+WS lifecycle, bot REST API)
```

Run: `npm test` — all 101 tests should pass.
