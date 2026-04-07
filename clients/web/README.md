# Roll-a-Ball Derby — Web Admin Client

Vanilla JS SPA served by the game server at `/admin`. Dual purpose: game host admin panel and sensor testing tool.

## Features

- **Game controls** — Start, pause/resume, reset
- **Config** — track length, max players, theme (locked while game running)
- **Players** — live list with progress bars, inline rename, connection status
- **Score simulator** — send +1 / +3 via WebSocket to test sensor flow
- **Event log** — real-time feed of all game events
- **Auto-reconnect** — exponential backoff (1s → 30s) on WS disconnect

## Usage

No build step. The game server serves this directory statically at `/admin`.

1. Start the server: `cd server && npm start`
2. Open `http://localhost:3000/admin` in a browser

Player name is remembered in `localStorage` (key: `derby-player-name`).

## File Structure

```
clients/web/
├── index.html          — SPA shell, 7 card sections
├── css/
│   └── style.css       — dark theme, CSS custom properties, responsive grid
└── js/
    ├── connection.js   — WebSocket client (Derby.Connection)
    ├── state.js        — State tracker + DOM renderer (Derby.State)
    ├── admin.js        — REST game controls + player rename (Derby.Admin)
    ├── test.js         — Score simulation panel (Derby.Test)
    └── main.js         — Entry point, message router
```

Scripts load in order: `connection → state → admin → test → main`.

## Module Namespace

All modules live on `window.Derby`:

| Module | Responsibilities |
|--------|-----------------|
| `Derby.Connection` | WS open/close/send/onMessage, reconnect backoff |
| `Derby.State` | Render full game state to DOM, log entries, winner banner |
| `Derby.Admin` | REST POST start/pause/reset, PUT config/rename |
| `Derby.Test` | Score button state management, send score via WS |

## Security

- User-supplied strings rendered via `textContent` or `_esc()` helper — no raw `innerHTML`
- Player name edit uses `document.createElement('input')`, not template strings
- Rename API calls use `encodeURIComponent` on player IDs
