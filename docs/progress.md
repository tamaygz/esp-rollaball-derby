# Progress Log — Roll-a-Ball Derby Architecture Planning

## Session: 2026-04-07

### Phase 1: Server Stack Research
- **Status:** complete
- **Started:** 2026-04-07
- Actions taken:
  - Created planning files (task_plan.md, findings.md, progress.md)
  - PRD reviewed — 6 open decisions identified
  - Researched Node.js + ws (22.7k stars, Autobahn-tested, raw WS)
  - Researched Python + FastAPI + WebSockets
  - Researched Links2004 vs gilmaimon Arduino WebSocket libs
  - Researched WiFiManager AP portal (tzapu)
  - Researched Pixi.js vs Phaser vs vanilla Canvas for display
  - Researched 28BYJ-48 vs NEMA 17 vs SG90 servo for motor control
  - Researched MCP23017 I²C GPIO expander compatibility
  - All findings documented in findings.md
- Files created/modified:
  - docs/task_plan.md (created)
  - docs/findings.md (created + updated)
  - docs/progress.md (created)

### Phase 5: User Decisions
- **Status:** complete
- Actions taken:
  - Presented research summary with recommendations
  - User confirmed: Node.js + ws, Pixi.js, WiFiManager, gilmaimon lib
  - User deferred motor type to Phase 3
  - Updated PRD §6 with all resolved decisions
  - Updated findings.md Technical Decisions table
- Files modified:
  - docs/PRD.md (decisions updated)
  - docs/findings.md (decisions table updated)
  - docs/task_plan.md (phases marked complete)

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|

### Phase 6: Implementation Plans
- **Status:** complete
- **Started:** 2026-04-07
- Actions taken:
  - 7 web searches across remaining domains (PlatformIO, IR debounce, AccelStepper, admin panels)
  - Created `plan/feature-server-web-1.md` (44 tasks, 6 phases)
  - Created `plan/feature-client-display-1.md` (28 tasks, 5 phases)
  - Created `plan/feature-client-web-1.md` (29 tasks, 5 phases)
  - Created `plan/feature-client-esp8266-sensor-1.md` (24 tasks, 4 phases)
  - Created `plan/feature-client-esp8266-motor-1.md` (33 tasks, 6 phases)
  - Updated findings.md status table to match resolved decisions
  - Updated task_plan.md Phase 6 to complete
- Files created:
  - plan/feature-server-web-1.md
  - plan/feature-client-display-1.md
  - plan/feature-client-web-1.md
  - plan/feature-client-esp8266-sensor-1.md
  - plan/feature-client-esp8266-motor-1.md
- Files modified:
  - docs/findings.md (Open Decisions table updated)
  - docs/task_plan.md (Phase 6 marked complete)
  - docs/progress.md (this entry)

---

## Session: 2026-04-07 (cont.) — Server Implementation

### Phase: Server Build
- **Status:** complete
- Actions taken:
  - Cloud agent (Software Engineer Agent) implemented full server from `plan/feature-server-web-1.md`
  - All 49 tests pass: `gameState.test.js` (27), `connectionManager.test.js` (13), `integration.test.js` (9)
  - Fixed: test glob `tests/*.test.js` required on Node 22+; `makeMockWs()` extended EventEmitter; WS listener race in integration tests
  - Bug discovered and fixed: `/assets/` static path had extra `../`
- Files created:
  - `server/src/index.js` — Express + WS, mounts `/assets/` and `/admin/`
  - `server/src/game/GameState.js` — state machine, scoring, 300ms rate-limit, name assignment from names.txt
  - `server/src/ws/ConnectionManager.js` — WS hub, broadcast helpers
  - `server/src/routes/{game,players,health}.js` — REST endpoints
  - `server/data/names.txt` — 60 horse/racing-themed fun names
  - `server/tests/` — 3 test files (49 tests total)
  - `server/package.json`, `.env.example`, `.gitignore`

---

## Session: 2026-04-07 (cont.) — Game Assets

### Phase: Asset Generation
- **Status:** complete
- Actions taken:
  - Designed and created SVG asset suite for horse and camel themes
  - All sprites white-fill tintable (Pixi.js `sprite.tint = 0xRRGGBB`)
  - Shared player-colors.json (16 colours, hex + pixi `0x` format) + preview.html
- Files created:
  - `clients/assets/themes/horse/` — sprite.svg, track-bg.svg, finish-flag.svg, theme.json
  - `clients/assets/themes/camel/` — sprite.svg, track-bg.svg, finish-flag.svg, theme.json
  - `clients/assets/themes/shared/player-colors.json`, `preview.html`

---

## Session: 2026-04-07 (cont.) — Web Admin SPA (PR #1)

### Phase: Client Web Build
- **Status:** complete (PR #1 open on branch `copilot/implement-server-frontend`)
- Actions taken:
  - Cloud agent implemented `plan/feature-client-web-1.md`
  - PR #1: "feat: server frontend — vanilla JS admin + test SPA at /admin"
  - Also fixed `/assets/` static path bug in `server/src/index.js`
- Files created:
  - `clients/web/index.html` — SPA shell, 7 sections
  - `clients/web/css/style.css` — dark theme, CSS custom properties, responsive grid
  - `clients/web/js/connection.js` — WS client, exponential-backoff reconnect
  - `clients/web/js/state.js` — game state tracker + DOM renderer (276 lines)
  - `clients/web/js/admin.js` — REST calls for game control + player rename
  - `clients/web/js/test.js` — score simulation panel
  - `clients/web/js/main.js` — entry point, message router, localStorage name persistence
- Key decisions:
  - Vanilla JS IIFE modules under `window.Derby` namespace (no bundler)
  - `_esc()` + DOM element creation (no `innerHTML` for user content), XSS-safe

## Test Results
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| gameState.test.js (27 tests)     | — | pass | pass | ✅ |
| connectionManager.test.js (13 tests) | — | pass | pass | ✅ |
| integration.test.js (9 tests)    | — | pass | pass | ✅ |

---

## Session: 2026-04-07 (cont.) — Display Client Build

### Phase: Display Client
- **Status:** complete
- Actions taken:
  - Implemented `plan/feature-client-display-1.md` — fullscreen Pixi.js SPA at `/display`
  - Theme-aware rendering: horse and camel themes with auto-random selection
  - Lane rendering with player sprites, track backgrounds, finish flags
  - Smooth position tweening (gsap) and scoring animations
  - Status overlays (idle / paused) and connection status dot
  - Winner overlay with confetti celebration
  - Auto-reconnect with exponential backoff
  - Pixi.js v8 + gsap v3 bundled locally in `vendor/` for offline LAN use
- Files created:
  - `clients/display/index.html` — entry point
  - `clients/display/css/style.css` — fullscreen reset, status dot
  - `clients/display/js/main.js` — Pixi app init, message routing
  - `clients/display/js/connection.js` — WebSocket client (display type)
  - `clients/display/js/ThemeManager.js` — theme asset loading
  - `clients/display/js/scene/RaceTrack.js` — all-lanes container
  - `clients/display/js/scene/Lane.js` — single player lane
  - `clients/display/js/effects/ScoringEffect.js` — scale-bounce + tint flash
  - `clients/display/js/effects/WinnerOverlay.js` — celebration + confetti
  - `clients/display/vendor/` — pixi.js + gsap (offline bundles)

---

## Session: 2026-04-07 (cont.) — Enhanced Scoring + Action Effects

### Phase: +2 Scoring, Events Pipeline, Action Effects
- **Status:** complete
- Actions taken:
  - Added +2 point scoring support (0/1/2/3 all valid)
  - Added auto-theme resolution: `'auto'` theme resolves to random horse/camel at game start
  - Added WebSocket reconnect with `playerId` — players keep their identity on reconnect
  - Added player removal (`DELETE /api/players/:id`)
  - Added `/api/clients` route (list/kick WebSocket clients)
  - Built streak tracking in GameState: consecutive zeros (3x threshold) and +3s (2x threshold)
  - Built rank-change detection: `took_lead` and `became_last` events
  - Server `scored` message now includes `events[]` array with up to 8 event types
  - Built `ActionEffect.js` — visual effect dispatcher for all 8 event types:
    - `zero_roll` → shrink-bounce + red flash + 😢 popup
    - `score_1` → scale-bounce + white tint flash
    - `score_2` → bigger bounce + blue lane flash
    - `score_3` → large bounce + gold flash + ⭐ "+3!" popup
    - `streak_zero_3x` → dark pulse + 😭 "3× ZERO" popup
    - `streak_three_2x` → orange glow + 🔥 "HOT!" popup
    - `took_lead` → gold aura + 👑 "LEAD!" popup
    - `became_last` → dark-red flash + 👎 "LAST!" popup
  - Lane.js extended with `_flashOverlay`, `_popupContainer` for effects
  - Tests expanded from 49 → 78 (streaks, rank events, reconnect, +2 scoring, auto-theme)
- Files created/modified:
  - `clients/display/js/effects/ActionEffect.js` — event-driven effect dispatcher (222 lines)
  - Modified: `server/src/game/GameState.js` — streaks, events, +2 scoring, auto-theme
  - Modified: `server/src/ws/ConnectionManager.js` — events in scored, reconnect, positions broadcast
  - Modified: `server/src/routes/{players,clients}.js` — remove player, list/kick clients
  - Modified: `clients/display/js/scene/Lane.js` — flash overlay, popup container
  - Modified: `clients/display/js/main.js` — events routing to ActionEffect

---

## Session: 2026-04-07 (cont.) — Server-Side Bot Players

### Phase: BotManager
- **Status:** complete
- Actions taken:
  - Created `BotManager.js` — server-side autonomous bot players (no WS connection needed)
  - Bots create players with type `'bot'`, get auto-assigned names
  - Bots roll at random 2–8 s intervals with human-like probability distribution
  - Full events pipeline works for bots (streaks, rank changes, zero rolls)
  - Created REST API: `GET/POST/DELETE /api/bots`
  - Wired BotManager into game routes (start/pause/reset lifecycle hooks)
  - Rewrote admin client `bots.js` — simple REST-based UI (no more client-side timers/player-overwriting bug)
  - Admin HTML simplified: single "🤖 Add Bot" button instead of player dropdown
  - Tests expanded from 78 → 101 (19 BotManager unit tests + 4 bot REST integration tests)
- Files created:
  - `server/src/game/BotManager.js` — autonomous bot player manager (188 lines)
  - `server/src/routes/bots.js` — REST router (39 lines)
  - `server/tests/botManager.test.js` — 19 tests
- Files modified:
  - `server/src/index.js` — BotManager wiring + /api/bots route
  - `server/src/routes/game.js` — botManager lifecycle hooks (onGameStart/Stop/Reset)
  - `server/tests/integration.test.js` — 4 bot REST API integration tests
  - `clients/web/js/bots.js` — rewritten to use REST API
  - `clients/web/index.html` — simplified bot section

## Test Results (Final)
| Test | Input | Expected | Actual | Status |
|------|-------|----------|--------|--------|
| botManager.test.js (19 tests)        | — | pass | pass | ✅ |
| gameState.test.js (37 tests)         | — | pass | pass | ✅ |
| connectionManager.test.js (27 tests) | — | pass | pass | ✅ |
| integration.test.js (18 tests)       | — | pass | pass | ✅ |
| **Total: 101 tests**                 | — | pass | pass | ✅ |

---

## Session: 2026-04-07 (cont.) — ESP8266 Sensor Client Firmware

### Phase: Sensor Firmware Implementation (Phases 1–3)
- **Status:** In progress (Phase 1–3 complete, Phase 4 testing backlog)
- **Branch:** `copilot/implement-esp-sensor-client` (PR #3)
- Actions taken:
  - Implemented PlatformIO project structure with dual board support (Wemos D1 Mini, NodeMCU v3)
  - Built WiFiManager integration with captive portal AP fallback (`Derby-Sensor-XXXX`)
  - Custom WiFiManager parameters: server IP, port, optional player name
  - Implemented `WSClient` — non-blocking WebSocket client with exponential backoff reconnect
  - Built `Sensors` class — hardware interrupt-based IR sensor reading (3 sensors = +1, +2, +3 points)
  - Implemented `ICACHE_RAM_ATTR` ISR functions for ESP8266 RAM-resident interrupt handlers
  - Debounce logic: 500 ms minimum between triggers per sensor using `millis()` timer
  - Built `StatusLed` state machine: NO_WIFI (5 Hz blink), WIFI_ONLY (1 Hz blink), WS_CONNECTED (solid on)
  - One-shot LED sequences for game events: countdown ticks, winner celebration, loser notification
  - LittleFS-based config persistence: server IP, port, player name survive device reboot
  - HTTP config endpoint on port 80 for alternative WiFi credential management
  - Serial debug output at 115200 baud for development/troubleshooting
- Key implementation details:
  - Pin mapping: D1=GPIO5 (+1), D5=GPIO14 (+2), D2=GPIO4 (+3), LED_BUILTIN=GPIO2
  - Non-blocking event loop: WS poll → sensor check → LED update, no `delay()` calls
  - Finite state machine for WiFi/WS/sensor flow
  - ArduinoJson v7.x for JSON message parsing/construction
  - gilmaimon/ArduinoWebsockets for raw RFC 6455 WebSocket protocol compatibility
- Web flashing support:
  - ESP Web Tools integration: browser-based flashing without PlatformIO toolchain
  - Pre-built firmware binaries published to GitHub Releases
  - Manifest files for d1_mini and nodemcuv2 board variants
  - Secure context support (https://) for flashing from remote machines
- Files created:
  - `clients/esp8266-sensor/platformio.ini` — PlatformIO configuration (dual board targets)
  - `clients/esp8266-sensor/src/main.cpp` — firmware entry point, setup/loop, WiFi setup
  - `clients/esp8266-sensor/src/config.h` — pin definitions, constants, WS backoff timings
  - `clients/esp8266-sensor/src/websocket.h/.cpp` — WebSocket client wrapper class
  - `clients/esp8266-sensor/src/sensors.h/.cpp` — IR sensor ISR handlers + debounce
  - `clients/esp8266-sensor/src/led.h/.cpp` — Status LED state machine + sequences
  - `clients/esp8266-sensor/web-install/index.html` — Web flashing UI
  - `clients/esp8266-sensor/web-install/manifest.json` — Firmware manifest (d1_mini + nodemcuv2)
  - `clients/esp8266-sensor/README.md` — Hardware setup, flashing instructions, WiFi setup guide
- Remaining tasks (Phase 4 — Testing Backlog):
  - Field testing with actual IR sensors and break-beam hardware
  - Stress testing with rapid-fire sensor triggers (ball flow simulation)
  - WiFi reconnect scenario testing (network dropout + recovery)
  - Server reconnect with player ID persistence verification
  - Web flashing end-to-end testing (multiple browsers, boards)
  - Serial output validation (debug logging completeness)
  - Edge cases: WiFi timeout, WS message queue overflow, sensor debounce edge cases
