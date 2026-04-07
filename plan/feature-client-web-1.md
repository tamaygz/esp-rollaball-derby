---
goal: "Implement the web test client + admin panel: vanilla JS SPA for testing and game control"
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: "@tamaygz"
status: "Completed"
tags: [feature, frontend, client, admin, testing]
---

# Client Web — Implementation Plan

![Status: Completed](https://img.shields.io/badge/status-Completed-brightgreen)

The web test client serves two purposes: (1) a development/testing tool that simulates ESP8266 sensor input by sending score events via buttons, and (2) a game host admin panel for configuring, starting, pausing, and resetting the game. It's a vanilla JavaScript SPA served by the game server — no build step, no framework.

## 1. Requirements & Constraints

- **REQ-001**: SPA served by the game server at `/admin` or `/admin/index.html`
- **REQ-002**: Connect to server via WebSocket, register as type `"web"` with optional `playerName`
- **REQ-003**: Test mode: player selector dropdown + "+1" and "+3" score buttons
- **REQ-004**: Admin mode: game configuration form (track length, max players, theme selector)
- **REQ-005**: Admin mode: Start, Pause/Resume, and Reset game buttons
- **REQ-006**: Admin mode: player list with name editing (host override), connection status
- **REQ-007**: Live game state display — show current status, player positions, connected clients
- **REQ-008**: WebSocket connection status indicator
- **REQ-009**: Auto-reconnect to WebSocket on disconnect
- **REQ-010**: Multiple web clients can connect simultaneously for multi-player testing
- **REQ-011**: Admin actions use REST API (POST/PUT); real-time state via WebSocket
- **SEC-001**: Sanitize player name input before sending (max 20 chars, strip HTML)
- **CON-001**: Vanilla JavaScript — no frameworks, no build step
- **CON-002**: Must work on mobile browsers (responsive layout) for game host using phone/tablet
- **CON-003**: Lightweight — loads instantly on local network
- **GUD-001**: Use native `fetch()` for REST calls, native `WebSocket` for real-time
- **GUD-002**: Simple responsive CSS — use CSS Grid/Flexbox, minimal styling
- **PAT-001**: State-driven UI: WebSocket `state` messages update DOM elements directly

## 2. Implementation Steps

### Phase 1: Project Setup & WebSocket Connection

- GOAL-001: Bootstrap the web test client SPA, establish WebSocket connection

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Create `clients/web/index.html` — basic responsive HTML layout with sections: connection, test, admin, state | | |
| TASK-002 | Create `clients/web/css/style.css` — responsive layout (CSS Grid), mobile-friendly, dark/light neutral theme | | |
| TASK-003 | Create `clients/web/js/main.js` — entry point, initialize modules | | |
| TASK-004 | Create `clients/web/js/connection.js` — WebSocket client, auto-reconnect (exponential backoff 1s–30s), connection status indicator | | |
| TASK-005 | Implement message dispatcher: parse `{ type, payload }`, route to UI update handlers | | |
| TASK-006 | Send `register` message on connect: `{ type: "register", payload: { type: "web", playerName } }` | | |
| TASK-007 | Player name input field — sent on register, stored in `localStorage` for persistence across reloads | | |

### Phase 2: Test Mode — Score Simulation

- GOAL-002: Buttons and player selector for simulating score events

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-008 | Player selector dropdown — populated from server state (list of connected players) | | |
| TASK-009 | "+1" button — sends `{ type: "score", payload: { playerId, points: 1 } }` via WebSocket | | |
| TASK-010 | "+3" button — sends `{ type: "score", payload: { playerId, points: 3 } }` via WebSocket | | |
| TASK-011 | Visual feedback on button press (brief flash/animate) to confirm send | | |
| TASK-012 | Disable score buttons when game is not running (idle/paused/finished) | | |

### Phase 3: Admin Mode — Game Control

- GOAL-003: Game configuration, lifecycle controls, player management

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-013 | Config form: track length input (5–50, default 15), max players input (1–16), theme selector (horse/camel) | | |
| TASK-014 | "Save Config" button — `PUT /api/game/config` via `fetch()`, disabled when game is running | | |
| TASK-015 | "Start Game" button — `POST /api/game/start`, disabled when no clients connected or game already running | | |
| TASK-016 | "Pause / Resume" button — `POST /api/game/pause`, toggles label based on current state | | |
| TASK-017 | "Reset Game" button — `POST /api/game/reset`, with confirmation dialog to prevent accidental reset | | |
| TASK-018 | Player list table: name, type (sensor/web), position, connected status | | |
| TASK-019 | Inline player name edit — click to edit, `PUT /api/players/:id` on blur/enter | | |
| TASK-020 | Error display: show API error responses as toast/banner messages | | |

### Phase 4: Live State Display

- GOAL-004: Real-time visualization of game state from WebSocket

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-021 | Game status badge: shows "Idle" / "Running" / "Paused" / "Finished" with color coding | | |
| TASK-022 | Player progress bars: simple horizontal bars showing position/trackLength per player | | |
| TASK-023 | Connected clients counter: total and by type (sensor, web, motor, display) | | |
| TASK-024 | Event log: scrollable list of recent score events with timestamp, player, points | | |
| TASK-025 | Winner announcement: prominent banner when game finishes with winner name | | |

### Phase 5: Polish & Responsiveness

- GOAL-005: Mobile-friendly layout, edge cases, UX

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-026 | Responsive layout: test on phone/tablet viewport sizes (game host may use phone) | | |
| TASK-027 | Touch-friendly buttons: minimum 44x44px tap targets for score buttons | | |
| TASK-028 | Handle edge cases: no players connected, rapid button presses, WebSocket disconnect during action | | |
| TASK-029 | Keyboard shortcuts: Enter to start game, Space to pause (optional) | | |

## 3. Alternatives

- **ALT-001**: React/Vue SPA — rejected; overhead of build step and framework for a simple admin tool
- **ALT-002**: Server-rendered templates (EJS/Handlebars) — rejected; need real-time WebSocket updates, SPA is simpler
- **ALT-003**: Combined display + admin in one SPA — rejected; display is fullscreen passive, admin is interactive — different concerns

## 4. Dependencies

- **DEP-001**: Game server WebSocket endpoint (from server-web plan)
- **DEP-002**: Game server REST API endpoints (from server-web plan)
- **DEP-003**: No JS libraries — vanilla browser APIs only (`WebSocket`, `fetch`, DOM)

## 5. Files

- **FILE-001**: `clients/web/index.html` — main HTML page
- **FILE-002**: `clients/web/css/style.css` — responsive styles
- **FILE-003**: `clients/web/js/main.js` — entry point and initialization
- **FILE-004**: `clients/web/js/connection.js` — WebSocket client with auto-reconnect
- **FILE-005**: `clients/web/js/test.js` — score simulation buttons and player selector
- **FILE-006**: `clients/web/js/admin.js` — game config form, lifecycle buttons, player management
- **FILE-007**: `clients/web/js/state.js` — live state display and event log

## 6. Testing

- **TEST-001**: WebSocket connect — client connects, registers as "web", receives state
- **TEST-002**: Auto-reconnect — disconnect server, verify client retries and reconnects
- **TEST-003**: Score buttons — press +1 and +3, verify server receives correct score events
- **TEST-004**: Admin config — change track length, verify saved via REST API
- **TEST-005**: Game lifecycle — start, pause, resume, reset via buttons, verify state changes
- **TEST-006**: Player name edit — change player name inline, verify server updates and broadcasts
- **TEST-007**: Multiple clients — open 2+ tabs, verify both receive state updates
- **TEST-008**: Mobile layout — test on 375px viewport, verify all buttons accessible

## 7. Risks & Assumptions

- **RISK-001**: No framework means more manual DOM manipulation — mitigated by simple UI (few dynamic elements)
- **RISK-002**: REST and WebSocket may report conflicting state briefly — mitigated by always using WebSocket state as source of truth for display
- **ASSUMPTION-001**: Admin actions are authenticated by network access (LAN only, no auth system)
- **ASSUMPTION-002**: Game host has a modern browser on phone/tablet/laptop

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Server Web Plan](feature-server-web-1.md)
- [MDN: WebSocket client](https://developer.mozilla.org/en-US/docs/Web/API/WebSockets_API/Writing_WebSocket_client_applications)
- [MDN: Fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API)
