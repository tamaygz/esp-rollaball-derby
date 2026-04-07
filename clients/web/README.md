# Roll-a-Ball Derby — Web Admin Client

Vanilla JS SPA served by the game server at `/admin`. Dual purpose: game host admin panel and sensor testing tool.

## Features

- **Game controls** — Start, pause/resume, reset
- **Config** — track length, max players, theme including auto-random (locked while game running)
- **Players** — live list with progress bars, inline rename, remove, connection status
- **Score simulator** — send +1 / +2 / +3 via WebSocket to test sensor flow
- **Server-side bots** — add/remove autonomous bot players via REST API
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
├── index.html          — SPA shell, 8 card sections
├── css/
│   └── style.css       — dark theme, CSS custom properties, responsive grid
└── js/
    ├── connection.js   — WebSocket client (Derby.Connection)
    ├── state.js        — State tracker + DOM renderer (Derby.State)
    ├── admin.js        — REST game controls + player rename/remove (Derby.Admin)
    ├── test.js         — Score simulation panel (Derby.Test)
    ├── bots.js         — Server-side bot management (Derby.Bots)
    └── main.js         — Entry point, message router
```

Scripts load in order: `connection → state → admin → test → bots → main`.

## Module Namespace

All modules live on `window.Derby`:

| Module | Responsibilities |
|--------|-----------------|
| `Derby.Connection` | WS open/close/send/onMessage, reconnect backoff |
| `Derby.State` | Render full game state to DOM, log entries, winner banner |
| `Derby.Admin` | REST POST start/pause/reset, PUT config/rename, DELETE player |
| `Derby.Test` | Score button state management, send score via WS |
| `Derby.Bots` | REST GET/POST/DELETE /api/bots, render bot list with status badges |

## Security

- User-supplied strings rendered via `textContent` or `_esc()` helper — no raw `innerHTML`
- Player name edit uses `document.createElement('input')`, not template strings
- Rename API calls use `encodeURIComponent` on player IDs
