# Roll-a-Ball Derby — Copilot Instructions

Local-network physical race game: IR sensors on ESP8266/ESP32 → Node.js server (single source of truth) → Pixi.js display + vanilla JS admin client, all over WebSocket.

## Architecture

```
ESP8266/ESP32 Sensors ──┐
                       ├──► Node.js Server ──► Display Client (Pixi.js beamer/TV)
ESP32 Peripheral ─────┘         │
                             └──► Web Admin Client (vanilla JS)
```

| Component | Status | Path |
|-----------|--------|------|
| Server | ✅ Complete | `server/` |
| Shared assets | ✅ Complete | `clients/assets/` |
| Web admin | ✅ Complete | `clients/web/` |
| Display client | ✅ Complete | `clients/display/` |
| ESP8266 sensor firmware | 🚧 In progress | `clients/esp8266-sensor/` (ESP8266 + ESP32 DevKit targets) |
| ESP32 peripheral firmware | 🚧 In progress | `clients/esp32-motor/` |
| ESP8266 motor firmware | ⏳ Superseded by ESP32 peripheral | `clients/esp8266-motor/` |

## Build and Test

```bash
cd server
npm install
npm start          # port 3000 (override with PORT env var)
npm run dev        # watch mode
npm test           # Node built-in test runner — no jest/mocha
```

- Admin: `http://localhost:3000/admin` (or `http://derby-server.local:3000/admin`)
- Display: `http://localhost:3000/display/` (`?fullscreen=1` for auto-fullscreen)
- Assets preview: open `clients/assets/themes/shared/preview.html` in a browser

**No build step** for any client — all JS is served as static files.

## Network Discovery (mDNS)

The server publishes `_derby._tcp` via DNS-SD (`bonjour-service`). ESP8266 sensors query `_derby._tcp.local` on boot to auto-discover the server IP and port — no manual IP configuration needed when both are on the same LAN. The `/api/health` endpoint exposes mDNS status. Manually configured IP/port in WiFiManager serves as fallback.

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

### Shared IO library (`clients/shared/io/`)
- Header-only utilities: include with angle brackets (`<device_info.h>`, `<color_utils.h>`)
- Platform guards use `#if defined(ESP8266)` / `#elif defined(ESP32)` — never check `ESP32` first
- Buffer size for chip ID strings: always use `DERBY_CHIP_ID_HEX_MAX_LEN` (17) to cover both platforms

### Shared LED library (`clients/shared/leds/`)
- Valid data pins: ESP8266 GPIO2 (UART1, default) or GPIO3 (DMA); ESP32 any GPIO 0–39 (RMT)
- Use `ledPinIsValid(pin)` from `LedPlatform.h` to validate GPIO before use
- `LED_GPIO_MAX` (39) is defined for ESP32 only; ESP8266 has no equivalent constant

### Assets (`clients/assets/`)
- Sprites use pure-white fills (`#ffffff`) so Pixi.js can tint them with any player colour
- Theme manifest schema is in `clients/assets/README.md`; both `horse/` and `camel/` must stay in sync

## WebSocket Protocol

All messages are JSON `{ type, payload }`. Key types:

| Direction | `type` | Notes |
|-----------|--------|-------|
| client→server | `register` | `{ type: "web"\|"sensor"\|"display"\|"motor", playerName?, playerId?, chipId?, chipType?, ledCount?, ledCapabilities?, deviceCapabilities? }` |
| server→broadcast | `state` | Full game state snapshot |
| server→broadcast | `scored` | Includes `events[]` array (see [server README](../server/README.md)) |
| server→broadcast | `winner` | `{ playerId, name }` |

Full protocol reference: [server/README.md](../server/README.md#websocket-protocol)

## Docs

- [PRD](../docs/PRD.md) — Product requirements and open decisions
- [Progress log](../docs/progress.md) — Session history
- [Findings](../docs/findings.md) — Research and technology decisions
- [Plans](../plan/) — Per-feature implementation plans
