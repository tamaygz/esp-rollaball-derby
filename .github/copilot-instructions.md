# Roll-a-Ball Derby — Copilot Instructions

Local-network physical race game: IR sensors on ESP8266 → Node.js server (single source of truth) → Pixi.js display + vanilla JS admin client, all over WebSocket.

## Architecture

```
ESP8266 Sensors ──┐
                   ├──► Node.js Server ──► Display Client (Pixi.js beamer/TV)
ESP8266 Motors ───┘         │
                             └──► Web Admin Client (vanilla JS)
```

| Component | Status | Path |
|-----------|--------|------|
| Server | ✅ Complete | `server/` |
| Shared assets | ✅ Complete | `clients/assets/` |
| Web admin | ✅ Complete | `clients/web/` |
| Display client | ✅ Complete | `clients/display/` |
| ESP8266 sensor firmware | 🔲 Not started | `clients/esp8266-sensor/` |
| ESP8266 motor firmware | ⏳ Deferred (Phase 3) | `clients/esp8266-motor/` |

## Build and Test

```bash
cd server
npm install
npm start          # port 3000 (override with PORT env var)
npm run dev        # watch mode
npm test           # Node built-in test runner — no jest/mocha
```

- Admin: `http://localhost:3000/admin`
- Display: `http://localhost:3000/display/` (`?fullscreen=1` for auto-fullscreen)
- Assets preview: open `clients/assets/themes/shared/preview.html` in a browser

**No build step** for any client — all JS is served as static files.

## Code Conventions

### Server (`server/`)
- Plain Node.js — no TypeScript, no build tooling
- Test files: `tests/*.test.js`, run with `node --test` (Node built-in runner)
- All game logic lives in `GameState.js`; WS fanout lives in `ConnectionManager.js`
- REST routes are thin — they delegate to `GameState` and `BotManager`
- No raw `res.send(string)` — always `res.json({...})`

### Web client (`clients/web/`)
- **No framework, no bundler** — vanilla JS with a `window.Derby` namespace
- Script load order matters: `connection → state → admin → test → bots → main`
- XSS guard: user-supplied strings must use `textContent` or the `_esc()` helper — never raw `innerHTML`
- Player ID in URLs: always wrap with `encodeURIComponent`

### Display client (`clients/display/`)
- Pixi.js + GSAP bundled in `vendor/` for offline/LAN use — do **not** add CDN imports
- Sprite tinting relies on pure-white SVG fills — new sprites must follow the same convention (see `clients/assets/README.md`)
- Action effects live in `js/effects/`; add new event types there, not in `main.js`

### Assets (`clients/assets/`)
- Sprites use pure-white fills (`#ffffff`) so Pixi.js can tint them with any player colour
- Theme manifest schema is in `clients/assets/README.md`; both `horse/` and `camel/` must stay in sync

## WebSocket Protocol

All messages are JSON `{ type, payload }`. Key types:

| Direction | `type` | Notes |
|-----------|--------|-------|
| client→server | `register` | `{ type: "web"\|"sensor"\|"display"\|"motor", playerName?, playerId? }` |
| server→broadcast | `state` | Full game state snapshot |
| server→broadcast | `scored` | Includes `events[]` array (see [server README](../server/README.md)) |
| server→broadcast | `winner` | `{ playerId, name }` |

Full protocol reference: [server/README.md](../server/README.md#websocket-protocol)

## Docs

- [PRD](../docs/PRD.md) — Product requirements and open decisions
- [Progress log](../docs/progress.md) — Session history
- [Findings](../docs/findings.md) — Research and technology decisions
- [Plans](../plan/) — Per-feature implementation plans
