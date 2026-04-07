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
