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
| GET | `/api/leds/config` | Get all LED configurations |
| GET | `/api/leds/config/:deviceType` | Get LED config for device type (sensor/motor/display) |
| PUT | `/api/leds/config/:deviceType` | Update LED config (triggers broadcast) |
| POST | `/api/leds/effects/test` | Send test effect to a specific device (rate-limited) |

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
| `led_config` | server→device | LED configuration update with `{ timestamp, payload: { ledCount, topology, gpioPin, brightness, defaultEffect } }` |
| `test_effect` | server→device | Test effect command with `{ payload: { effectName, params } }` |
| `error` | server→client | `{ message }` |

### LED Configuration

Devices (sensor, motor) register with optional `ledCount` and `chipType` fields:

```json
{
  "type": "register",
  "payload": {
    "type": "sensor",
    "playerName": "ESP-001",
    "ledCount": 10,
    "chipType": "ESP8266"
  }
}
```

On registration, the server:
1. Validates reported LED count against configured count (±5 tolerance)
2. Includes `warning` field in `registered` response if mismatch detected
3. Auto-sends `led_config` message with current configuration

Configuration updates via REST API trigger broadcast to all devices of that type.

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

The `BotManager` creates autonomous bot players that score at random 345 ms–6.3 s intervals while the game is running. Bots:

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

## LED Configuration API

Server stores LED configurations for each device type in `server/data/led-config.json`. Configuration schema:

```json
{
  "sensor": {
    "ledCount": 10,
    "topology": "strip",
    "gpioPin": 4,
    "brightness": 255,
    "defaultEffect": "rainbow"
  },
  "motor": {
    "ledCount": 10,
    "topology": "strip",
    "gpioPin": 4,
    "brightness": 255,
    "defaultEffect": "chase"
  },
  "display": {
    "ledCount": 0,
    "topology": "strip",
    "gpioPin": 4,
    "brightness": 0,
    "defaultEffect": "solid"
  }
}
```

### Field Descriptions

- `ledCount` (0–1000): Number of LEDs in the strip, subject to platform limits (ESP8266: 300, ESP32: 1000)
- `topology` (`"strip"` | `"ring"` | `"matrix"`): Physical arrangement of LEDs
- `gpioPin` (0–32): GPIO pin number for LED data line
- `brightness` (0–255): Global brightness level (255 = 100%)
- `defaultEffect` (`"solid"` | `"blink"` | `"pulse"` | `"rainbow"` | `"chase"` | `"sparkle"`): Effect shown when idle

### Example API Requests

**Get all configurations:**
```bash
curl http://localhost:3000/api/leds/config
```

**Get sensor configuration:**
```bash
curl http://localhost:3000/api/leds/config/sensor
```

**Update motor LED count and effect:**
```bash
curl -X PUT http://localhost:3000/api/leds/config/motor \
  -H "Content-Type: application/json" \
  -d '{"ledCount": 20, "defaultEffect": "pulse"}'
```

**Send test effect to device:**
```bash
curl -X POST http://localhost:3000/api/leds/effects/test \
  -H "Content-Type: application/json" \
  -d '{"deviceId": "abc123", "effectName": "rainbow", "params": {"duration": 3000}}'
```

Rate limiting: Test effect endpoint limited to 1 request/second per device ID.

## Tests

```
server/tests/
├── botManager.test.js        (19 tests — bot lifecycle, scoring, game hooks)
├── gameState.test.js         (37 tests — state machine, scoring, rate-limiting, names, streaks, ranks)
├── connectionManager.test.js (27 tests — WS hub, routing, broadcasts, reconnect, disconnects)
└── integration.test.js       (18 tests — HTTP+WS lifecycle, bot REST API)
```

Run: `npm test` — all 101 tests should pass.
