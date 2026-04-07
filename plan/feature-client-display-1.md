---
goal: "Implement the display client: Pixi.js race track visualization for beamer/TV"
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: "@tamaygz"
status: "Completed"
tags: [feature, frontend, display, pixi]
---

# Client Display — Implementation Plan

![Status: Completed](https://img.shields.io/badge/status-Completed-brightgreen)

The display client is a fullscreen SPA rendered on a beamer/TV connected to the game server. It shows horizontal stacked lanes (one per player), animated player figures that move from start to finish as scores come in, and themed visuals (horse race, camel race). It receives all state updates via WebSocket and has no user input — purely a spectator view.

## 1. Requirements & Constraints

- **REQ-001**: Fullscreen SPA served from the game server at `/display` or `/display/index.html`
- **REQ-002**: Connect to server via WebSocket, receive `state`, `scored`, `winner` messages
- **REQ-003**: Render horizontal stacked lanes — one per active player, dynamically adjusting to player count (1–16)
- **REQ-004**: Each lane shows: player name label, player figure sprite, track background, finish line marker
- **REQ-005**: Player figures tween smoothly to new position on score events (proportional: position/trackLength)
- **REQ-006**: Scoring animation: flash/bounce on the player figure when a `scored` event arrives
- **REQ-007**: Winner animation: prominent announcement overlay when `winner` event arrives
- **REQ-008**: Game status overlay: show "WAITING" (idle), "PAUSED" (paused) states
- **REQ-009**: Theming system: load different sprite sheets and backgrounds per theme (horse, camel)
- **REQ-010**: Scale to any resolution (1080p, 4K, projector native) — responsive canvas sizing
- **REQ-011**: Auto-reconnect to WebSocket on disconnect (exponential backoff)
- **REQ-012**: No user interaction required — display-only (no buttons, forms, or input)
- **CON-001**: Pixi.js v8 as rendering engine (~450KB)
- **CON-002**: No build step required for MVP — can use ESM imports from CDN or bundled in `public/`
- **CON-003**: Must work in modern Chromium-based browsers (Chrome, Edge — typical beamer browser)
- **GUD-001**: Use `gsap` (GreenSock) for tweening — industry standard, lightweight, Pixi.js compatible
- **GUD-002**: Sprites organized as texture atlases per theme for efficient GPU batching
- **PAT-001**: State-driven rendering: WebSocket state → update Pixi scene graph → Pixi ticker renders

## 2. Implementation Steps

### Phase 1: Project Setup & WebSocket Connection

- GOAL-001: Bootstrap the display SPA, connect to game server WebSocket, handle messages

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Create `clients/display/index.html` — fullscreen HTML page, load Pixi.js + gsap via ESM/CDN | | |
| TASK-002 | Create `clients/display/js/main.js` — entry point, initialize Pixi Application (fullscreen, resize listener) | | |
| TASK-003 | Create `clients/display/js/connection.js` — WebSocket client, auto-reconnect with exponential backoff | | |
| TASK-004 | Implement message dispatcher: parse `{ type, payload }`, route to handlers (state, scored, winner) | | |
| TASK-005 | Send `register` message on connect: `{ type: "register", payload: { type: "display" } }` | | |
| TASK-006 | Add connection status indicator (small dot: green=connected, red=disconnected) | | |

### Phase 2: Race Track Layout & Lane Rendering

- GOAL-002: Render dynamic lanes that adapt to the number of players

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | Create `clients/display/js/scene/RaceTrack.js` — Pixi Container managing all lanes | | |
| TASK-008 | Create `clients/display/js/scene/Lane.js` — single lane: background strip, track line, player sprite, name label, finish line | | |
| TASK-009 | Dynamic lane layout: divide canvas height by player count, reflow on player add/remove | | |
| TASK-010 | Draw finish line marker at right edge of each lane (or configurable position) | | |
| TASK-011 | Draw start position marker at left edge of each lane | | |
| TASK-012 | Player name as Pixi Text, left-aligned on the lane | | |
| TASK-013 | Handle canvas resize (window resize event) — recalculate all lane positions and sizes | | |

### Phase 3: Sprite System & Theming

- GOAL-003: Load themed sprites and support theme switching

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-014 | Create `clients/display/assets/themes/horse/` — sprite atlas (horse figures, track bg, finish flag) | | |
| TASK-015 | Create `clients/display/assets/themes/camel/` — sprite atlas (camel figures, track bg, finish flag) | | |
| TASK-016 | Create `clients/display/js/ThemeManager.js` — load texture atlas for active theme, provide texture references | | |
| TASK-017 | Assign distinct sprite variant (color/number) per player within a theme | | |
| TASK-018 | Support theme switch on config change (reload textures, update all lanes) | | |

### Phase 4: Animation & Effects

- GOAL-004: Smooth movement, scoring effects, winner celebration

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-019 | Implement player figure tween: on `state` update, gsap.to() sprite x-position to `(position / trackLength) * laneWidth` | | |
| TASK-020 | Implement scoring flash: on `scored` event, scale-bounce + brightness flash on the scored player's sprite | | |
| TASK-021 | Implement winner overlay: full-screen celebration animation (confetti particles, winner name, "WINNER!" text) | | |
| TASK-022 | Implement idle state overlay: "WAITING FOR PLAYERS" centered text when status=idle | | |
| TASK-023 | Implement pause overlay: "PAUSED" centered text with dimmed background when status=paused | | |
| TASK-024 | Implement player connect/disconnect visual: dim/grey-out sprite for disconnected players | | |

### Phase 5: Responsive Scaling & Fullscreen

- GOAL-005: Ensure display looks perfect on any beamer/TV resolution

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-025 | Auto-detect screen resolution, set Pixi renderer to fill viewport | | |
| TASK-026 | Handle browser fullscreen API (F11 / auto-fullscreen via URL param `?fullscreen=1`) | | |
| TASK-027 | Test at 1920x1080, 3840x2160 (4K), and 1024x768 (older projectors) | | |
| TASK-028 | Ensure text remains readable at all resolutions (dynamic font sizing) | | |

## 3. Alternatives

- **ALT-001**: Phaser 3 — rejected; 1.2MB bundle, built-in physics/audio/input we don't need for a passive display
- **ALT-002**: Vanilla Canvas + CSS — rejected; too much manual work for sprite animation, tweening, and theming
- **ALT-003**: React + CSS transitions — rejected; DOM-based rendering less performant for 16 concurrent sprite animations, no GPU batching

## 4. Dependencies

- **DEP-001**: Pixi.js v8 — 2D WebGL rendering engine (~450KB)
- **DEP-002**: gsap (GreenSock) v3 — animation/tweening library (~30KB core)
- **DEP-003**: Game server WebSocket (from server-web plan)
- **DEP-004**: Sprite assets per theme (horse, camel) — to be created or sourced

## 5. Files

- **FILE-001**: `clients/display/index.html` — entry HTML, loads JS/CSS, fullscreen layout
- **FILE-002**: `clients/display/js/main.js` — Pixi app init, scene setup, message routing
- **FILE-003**: `clients/display/js/connection.js` — WebSocket client with auto-reconnect
- **FILE-004**: `clients/display/js/scene/RaceTrack.js` — lane container and layout manager
- **FILE-005**: `clients/display/js/scene/Lane.js` — individual lane rendering
- **FILE-006**: `clients/display/js/ThemeManager.js` — theme asset loading and texture references
- **FILE-007**: `clients/display/js/effects/ScoringEffect.js` — bounce/flash on score
- **FILE-008**: `clients/display/js/effects/WinnerOverlay.js` — winner celebration animation
- **FILE-009**: `clients/display/assets/themes/horse/` — horse theme sprites
- **FILE-010**: `clients/display/assets/themes/camel/` — camel theme sprites
- **FILE-011**: `clients/display/css/style.css` — base styles (fullscreen, no scrollbar, black bg)

## 6. Testing

- **TEST-001**: WebSocket connection — connects, registers as "display" type, receives state
- **TEST-002**: Auto-reconnect — disconnect server, verify client retries and reconnects
- **TEST-003**: Lane rendering — verify correct number of lanes for 1, 4, 8, 16 players
- **TEST-004**: Position tween — send score events, verify sprites move to correct proportional position
- **TEST-005**: Scoring animation — trigger scored event, verify visual flash/bounce
- **TEST-006**: Winner overlay — trigger winner event, verify celebration displays
- **TEST-007**: Theme switch — change theme in config, verify sprites update
- **TEST-008**: Resolution scaling — test at 1080p, 4K, 1024x768

## 7. Risks & Assumptions

- **RISK-001**: Sprite assets may need custom creation — placeholder colored rectangles for MVP, proper sprites for v1.0
- **RISK-002**: Pixi.js v8 API changes from v7 — verify API compatibility with current docs
- **RISK-003**: Older projectors may not support WebGL — Pixi.js has Canvas fallback but it's slower
- **ASSUMPTION-001**: Browser on beamer machine is modern Chromium (Chrome/Edge)
- **ASSUMPTION-002**: Server and display machine on same local network (<50ms latency)
- **ASSUMPTION-003**: gsap free (no-charge) license covers non-commercial/personal use

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Server Web Plan](feature-server-web-1.md)
- [Pixi.js v8 docs](https://pixijs.com/8.x/guides)
- [gsap docs](https://gsap.com/docs/v3/)

## 9. Implementation Notes (Post-Completion)

Completed with the following additions beyond the original plan:

- **ActionEffect.js** — full event-driven effects pipeline replacing the simple scoring flash:
  - 8 event types (zero_roll, score_1/2/3, streak_zero_3x, streak_three_2x, took_lead, became_last)
  - Per-event visual treatments: scale-bounces, tint flashes, lane flashes, emoji popups
  - Streak effects delayed slightly to layer on top of base score effects
  - Rank effects fire last for visual prominence
- **Vendor bundles** — Pixi.js + gsap shipped in `vendor/` for offline LAN use (not CDN)
- **SVG sizing** — all SVGs use explicit width/height at 3× viewBox for crisp Pixi texture rendering
- **Events array** — display consumes full `events[]` from server `scored` messages; falls back to deriving events from `points` for backwards compatibility
