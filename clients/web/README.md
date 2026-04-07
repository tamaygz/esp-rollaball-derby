# Roll-a-Ball Derby — Web Admin Client

Vanilla JS SPA served by the game server at `/admin`. Dual purpose: game host admin panel and sensor testing tool.

## Features

- **Game controls** — Start, pause/resume, reset
- **Config** — track length, max players, theme (locked while game running)
- **Players** — live list with progress bars, inline rename, remove, connection status
- **Score simulator** — send +1 / +2 / +3 via WebSocket to test sensor flow
- **Bots** — add per-player bots that auto-score at human-like random intervals
- **Devices page** — debug view of all connected ESP/WS clients with kick support
- **Event log** — real-time feed of all game events
- **Auto-reconnect** — exponential backoff (1s → 30s) on WS disconnect

## Usage

No build step. The game server serves this directory statically at `/admin`.

1. Start the server: `cd server && npm start`
2. Open `http://localhost:3000/admin` in a browser
3. Open `http://localhost:3000/admin/devices.html` for the devices debug page

Player name is remembered in `localStorage` (key: `derby-player-name`).

## File Structure

```
clients/web/
├── index.html          — Admin SPA shell
├── devices.html        — ESP/WS devices debug page
├── css/
│   └── style.css       — dark theme, CSS custom properties, responsive grid
└── js/
    ├── connection.js   — WebSocket client (Derby.Connection)
    ├── state.js        — State tracker + DOM renderer (Derby.State)
    ├── admin.js        — REST game controls + player rename/remove (Derby.Admin)
    ├── test.js         — Score simulation panel (Derby.Test)
    ├── bots.js         — Bot management panel (Derby.Bots)
    ├── devices.js      — Devices debug page logic
    └── main.js         — Entry point, message router
```

Scripts load in order: `connection → state → admin → test → bots → main`. The devices page loads only `devices.js`.

## Module Namespace

All modules live on `window.Derby`:

| Module | Responsibilities |
|--------|-----------------|
| `Derby.Connection` | WS open/close/send/onMessage, reconnect backoff |
| `Derby.State` | Render full game state to DOM, log entries, winner banner |
| `Derby.Admin` | REST POST start/pause/reset, PUT config/rename, DELETE player |
| `Derby.Test` | Score button state management, send score via WS |
| `Derby.Bots` | Add/remove bots per player, auto-score at random intervals |

## Security

- User-supplied strings rendered via `textContent` or `_esc()` helper — no raw `innerHTML`
- Player name edit uses `document.createElement('input')`, not template strings
- Rename API calls use `encodeURIComponent` on player IDs
