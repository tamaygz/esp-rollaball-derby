---
applyTo: "server/**"
---

# Server Conventions

Full reference: [server/README.md](../../server/README.md)

## Module Roles

| File | Responsibility |
|------|---------------|
| `src/game/GameState.js` | All game logic and state — the only place scores, players, and status change |
| `src/ws/ConnectionManager.js` | WebSocket hub: routing, broadcasts, reconnect tracking |
| `src/game/BotManager.js` | Autonomous bot lifecycle — hooks into GameState events |
| `src/routes/*.js` | Thin REST handlers — delegate to GameState/BotManager, never mutate state directly |
| `src/index.js` | Express + WS bootstrap, static mounts, wires modules together |

**Routes must not contain game logic.** Any new game behaviour belongs in `GameState.js`.

## GameState Machine

```
idle ──start()──► running ◄──resume()──► paused
  ▲                  │                      │
  └───reset()────────┼──────────────────────┘
                     └──winner──► finished ──reset()──► idle
```

### Transition guards

| Method | Valid from | Throws if not |
|--------|-----------|---------------|
| `start()` | `idle` | `'Cannot start: game is …'` |
| `start()` | requires ≥1 connected player | `'Cannot start: no players connected'` |
| `pause()` | `running` or `paused` (toggles) | `'Cannot pause: game is …'` |
| `score()` | `running` only | `'Cannot score: game is …'` |
| `updateConfig()` | `idle` only | `'Cannot update config while game is running'` |
| `reset()` | any state | — resets positions, clears name pool, keeps player map |
| `finish(winnerId)` | called internally by `score()` when position ≥ trackLength | — |

`reset()` does **not** remove players — it zeroes positions and streak counters. Players are only removed in `idle`; in other states `removePlayer` marks them `connected: false`.

### Theme resolution

`theme: 'auto'` is resolved to a random concrete theme (`'horse'` or `'camel'`) at `start()` time — it is not stored as `'auto'` after that point.

## Config Constraints

| Field | Type | Range |
|-------|------|-------|
| `trackLength` | integer | 5–50 |
| `maxPlayers` | integer | 1–16 |
| `theme` | string | `'horse'`, `'camel'`, or `'auto'` |

All three fields are optional in `PUT /api/game/config` — only present keys are validated and merged.

## Player Object Shape

```js
{
  id,                   // uuid string
  name,                 // display string
  position,             // integer ≥ 0
  type,                 // 'sensor' | 'bot' | 'motor' | 'web' | 'display'
  connected,            // boolean
  connectedAt,          // Date.now() timestamp
  lastScoredAt,         // Date.now() or null
  consecutiveZeros,     // integer — resets on any non-zero score
  consecutivePlusThrees // integer — resets on any non-three score
}
```

## Scoring Rules

- Valid `points` values: `0`, `1`, `2`, `3` — anything else throws
- Rate limit: **300 ms** per player (`RATE_LIMIT_MS`) — throws `'rate limited'`
- Events array is built inside `score()` — do not compute events in routes or ConnectionManager

### events[] logic

| Event | Condition |
|-------|-----------|
| `zero_roll` | `points === 0` |
| `score_1/2/3` | `points === 1/2/3` |
| `streak_zero_3x` | `consecutiveZeros >= 3` after update |
| `streak_three_2x` | `consecutivePlusThrees >= 2` after update |
| `took_lead` | moved to rank 0 and was not rank 0 before (requires ≥2 players, points > 0) |
| `became_last` | moved to last rank and was not last before (requires ≥2 players, points > 0) |

## REST Routes

| Method | Path | Handler | Notes |
|--------|------|---------|-------|
| GET | `/api/health` | `routes/health.js` | |
| GET | `/api/game` | `routes/game.js` | Returns full state + `connectedClients` counts |
| POST | `/api/game/start` | `routes/game.js` | Calls `gameState.start()`, `botManager.onGameStart()`, broadcasts |
| POST | `/api/game/pause` | `routes/game.js` | Toggles; calls `botManager.onGameStart/Stop()` based on new status |
| POST | `/api/game/reset` | `routes/game.js` | Calls `botManager.onGameReset()`, broadcasts state + positions |
| PUT | `/api/game/config` | `routes/game.js` | `idle` only; broadcasts state on success |
| GET | `/api/players` | `routes/players.js` | |
| PUT | `/api/players/:id` | `routes/players.js` | Rename — broadcasts state |
| DELETE | `/api/players/:id` | `routes/players.js` | Remove — broadcasts state |
| GET | `/api/bots` | `routes/bots.js` | |
| POST | `/api/bots` | `routes/bots.js` | Creates bot + player |
| DELETE | `/api/bots/:id` | `routes/bots.js` | Removes bot + player |
| GET | `/api/clients` | `routes/clients.js` | |
| DELETE | `/api/clients/:id` | `routes/clients.js` | Kicks WS client |

All error responses: `res.status(4xx).json({ error: message })` — never `res.send(string)`.

## BotManager Hooks

Call these after every state transition that affects bots:

| Event | Method |
|-------|--------|
| Game started / resumed | `botManager.onGameStart()` |
| Game paused / stopped | `botManager.onGameStop()` |
| Game reset | `botManager.onGameReset()` |

## Tests

```
server/tests/
├── gameState.test.js         37 tests — state machine, scoring, streaks, ranks
├── connectionManager.test.js 27 tests — WS hub, broadcasts, reconnect
├── botManager.test.js        19 tests — bot lifecycle, scoring hooks
└── integration.test.js       18 tests — HTTP + WS lifecycle end-to-end
```

Run: `npm test` (uses `node --test`, **not** Jest/Mocha). All 101 tests must pass before committing.

New test files go in `tests/` and must match `*.test.js`. Use Node's built-in `assert` — no external assertion libraries.
