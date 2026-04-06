---
goal: "Implement the game server: Node.js + Express + ws — REST API, WebSocket hub, game logic, persistence"
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: "@tamaygz"
status: "Planned"
tags: [feature, architecture, server]
---

# Server Web — Implementation Plan

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

The game server is the central brain of the Roll-a-Ball Derby system. It runs as a single Node.js process serving REST endpoints for game administration, a WebSocket hub for real-time bidirectional communication with all clients (display, web test, ESP8266 sensors, ESP8266 motor controller), and a static file server for the frontend SPAs.

## 1. Requirements & Constraints

- **REQ-001**: Serve REST API for game config, start/pause/reset, player management (PRD §3.3)
- **REQ-002**: WebSocket hub accepting connections from sensor clients, web test clients, motor controller, and display clients (PRD §3.1)
- **REQ-003**: Game state machine: `idle` → `running` → `paused` → `finished` with transition enforcement
- **REQ-004**: Support 1–16 concurrent players, each with `id`, `name`, `position`, `connected` state
- **REQ-005**: Validate incoming score events (points must be 1 or 3, player must exist, game must be running)
- **REQ-006**: Rate limit score events: max 1 per 300ms per player
- **REQ-007**: Auto-assign random player name from `names.txt` if client doesn't provide one (unique per session)
- **REQ-008**: Sanitize player names: strip HTML/script, trim, max 20 chars
- **REQ-009**: Detect winner when a player reaches configurable track length (default 15)
- **REQ-010**: Broadcast full game state on every state change to all connected clients
- **REQ-011**: Send `positions` message specifically to motor controller clients
- **REQ-012**: Persist game history to SQLite (v1.0, not MVP)
- **SEC-001**: Validate and reject malformed WebSocket payloads (JSON parse errors, unknown event types)
- **SEC-002**: Player name XSS prevention — sanitize before broadcast
- **CON-001**: Must run on local network only (bind to LAN interface)
- **CON-002**: Single `npm install && npm start` setup — no build step required for server
- **CON-003**: Node.js LTS (v20+)
- **GUD-001**: Use raw WebSocket protocol (RFC 6455) — no Socket.IO
- **GUD-002**: JSON message format with `{ type: string, payload: object }` envelope
- **PAT-001**: State machine pattern for game lifecycle
- **PAT-002**: Observer pattern for WebSocket broadcast (server tracks connected clients by type)

## 2. Implementation Steps

### Phase 1: Project Bootstrap & Core Server

- GOAL-001: Scaffold the Node.js project, set up Express + `ws`, serve static files, health check

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-001 | Create `server/package.json` with dependencies: `express`, `ws`, `uuid` | | |
| TASK-002 | Create `server/src/index.js` — Express app, HTTP server, WebSocket server on same port (default 3000) | | |
| TASK-003 | Serve static files from `server/public/` (will hold display + test client SPAs) | | |
| TASK-004 | Add `GET /api/health` endpoint returning `{ status: "ok", uptime }` | | |
| TASK-005 | Add `.env` / config loading: `PORT`, `HOST` (default `0.0.0.0` for dev, warn if not LAN-bound in prod) | | |
| TASK-006 | Create `server/.gitignore` (node_modules, .env, *.db) | | |

### Phase 2: Game State Machine

- GOAL-002: Implement the core game state, config, and lifecycle transitions

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-007 | Create `server/src/game/GameState.js` — class managing state: `{ status, config, players[], startedAt }` | | |
| TASK-008 | Implement state transitions: `idle` → `running` → `paused` ↔ `running` → `finished` → `idle` (reset) | | |
| TASK-009 | Implement `config` object: `{ trackLength: 15, maxPlayers: 16, theme: "horse" }` with validation | | |
| TASK-010 | Implement `addPlayer(id, name?, type)` — assigns unique name from `names.txt` if none given | | |
| TASK-011 | Implement `removePlayer(id)` — marks disconnected, doesn't remove during active game | | |
| TASK-012 | Implement `score(playerId, points)` — validates state=running, points∈{1,3}, rate limit 300ms, updates position | | |
| TASK-013 | Implement winner detection: after each score, check if `player.position >= config.trackLength` | | |
| TASK-014 | Create `server/data/names.txt` with ≥50 horse/racing-themed fun names | | |
| TASK-015 | Implement name assignment logic: pick random unused name from file, ensure uniqueness in session | | |

### Phase 3: WebSocket Hub

- GOAL-003: Handle WebSocket connections, message routing, client registration, and broadcast

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-016 | Create `server/src/ws/ConnectionManager.js` — tracks connected clients Map<id, { ws, type, playerId }> | | |
| TASK-017 | Implement `register` message handler: validate `{ type, playerId?, playerName? }`, add to manager | | |
| TASK-018 | Implement `score` message handler: validate payload, call `GameState.score()`, broadcast result | | |
| TASK-019 | Implement broadcast logic: `broadcastAll(msg)` sends to all connected clients | | |
| TASK-020 | Implement `broadcastState()` — sends full `{ type: "state", payload: gameState.toJSON() }` to all | | |
| TASK-021 | Implement `broadcastPositions()` — sends `{ type: "positions", payload }` only to motor-type clients | | |
| TASK-022 | Implement `scored` event broadcast: `{ type: "scored", payload: { playerId, points, newPosition } }` | | |
| TASK-023 | Implement `winner` event broadcast: `{ type: "winner", payload: { playerId, name } }` | | |
| TASK-024 | Handle WebSocket `close` / `error` — mark player disconnected, broadcast updated state | | |
| TASK-025 | Implement JSON message envelope parsing with error handling (malformed JSON → close with 1003) | | |
| TASK-026 | Implement rate limiting: track last score timestamp per player, reject if <300ms gap | | |

### Phase 4: REST API

- GOAL-004: Admin endpoints for game configuration and control

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-027 | `GET /api/game` — return current game state as JSON | | |
| TASK-028 | `POST /api/game/start` — transition to running (validate at least 1 client connected) | | |
| TASK-029 | `POST /api/game/pause` — toggle pause/resume | | |
| TASK-030 | `POST /api/game/reset` — reset all positions to 0, status to idle, broadcast state | | |
| TASK-031 | `PUT /api/game/config` — update trackLength, maxPlayers, theme (only when idle) | | |
| TASK-032 | `GET /api/players` — list connected players with status | | |
| TASK-033 | `PUT /api/players/:id` — update player name (host override), sanitize, broadcast | | |
| TASK-034 | Add input validation middleware for all endpoints (express-validator or manual) | | |

### Phase 5: Testing & Polish

- GOAL-005: Unit tests, integration tests, startup logging

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-035 | Unit tests for `GameState` — state transitions, scoring, winner detection, name assignment | | |
| TASK-036 | Unit tests for `ConnectionManager` — register, disconnect, broadcast | | |
| TASK-037 | Integration test: WebSocket client connects, registers, scores, receives state broadcast | | |
| TASK-038 | Add startup console logging: port, host, connected clients count, game status | | |
| TASK-039 | Add `npm run dev` script with `--watch` for auto-reload during development | | |

### Phase 6: Persistence (v1.0)

- GOAL-006: SQLite game history for completed games

| Task | Description | Completed | Date |
|------|-------------|-----------|------|
| TASK-040 | Add `better-sqlite3` dependency | | |
| TASK-041 | Create `server/src/db/schema.sql` — `games` table (id, config, started_at, finished_at, winner_id) + `game_players` table | | |
| TASK-042 | Create `server/src/db/Database.js` — init, saveGame, getHistory | | |
| TASK-043 | On game finish, persist game result to SQLite | | |
| TASK-044 | `GET /api/history` — return past game results | | |

## 3. Alternatives

- **ALT-001**: Socket.IO instead of raw `ws` — rejected due to protocol overhead on ESP8266 and unnecessary abstraction for our simple message format
- **ALT-002**: Python + FastAPI — rejected; Node.js provides single-runtime simplicity and better community examples for ESP8266 WebSocket integration
- **ALT-003**: In-memory persistence only — chosen for MVP; SQLite deferred to Phase 6 (v1.0)

## 4. Dependencies

- **DEP-001**: `express` ^4.x — HTTP server and static file serving
- **DEP-002**: `ws` ^8.x — WebSocket server (RFC 6455)
- **DEP-003**: `uuid` ^9.x — unique client/player ID generation
- **DEP-004**: `better-sqlite3` ^11.x — SQLite persistence (Phase 6 only)
- **DEP-005**: Node.js v20 LTS

## 5. Files

- **FILE-001**: `server/package.json` — project manifest and scripts
- **FILE-002**: `server/src/index.js` — entry point: Express + WS server setup
- **FILE-003**: `server/src/game/GameState.js` — game state machine and logic
- **FILE-004**: `server/src/ws/ConnectionManager.js` — WebSocket client tracking and broadcast
- **FILE-005**: `server/src/routes/game.js` — REST API routes for game control
- **FILE-006**: `server/src/routes/players.js` — REST API routes for player management
- **FILE-007**: `server/data/names.txt` — pre-generated player names (≥50)
- **FILE-008**: `server/public/` — directory for serving display + test client SPAs
- **FILE-009**: `server/src/db/Database.js` — SQLite wrapper (Phase 6)
- **FILE-010**: `server/src/db/schema.sql` — database schema (Phase 6)
- **FILE-011**: `server/tests/` — test directory

## 6. Testing

- **TEST-001**: GameState transitions — idle→running only when players connected, running→paused→running, running→finished on winner
- **TEST-002**: Scoring — valid points (1, 3) accepted; invalid rejected; rate limit enforced; position increments correctly
- **TEST-003**: Winner detection — player reaching trackLength triggers finished state and winner broadcast
- **TEST-004**: Name assignment — random unique names from file; custom names preserved; sanitization works
- **TEST-005**: WebSocket register — clients tracked by type; duplicate IDs handled; disconnection cleanup
- **TEST-006**: Broadcast — all clients receive state updates; motor clients receive positions; scored events fire
- **TEST-007**: REST API — config only changeable when idle; start requires connected client; pause toggles
- **TEST-008**: Malformed messages — invalid JSON rejected; unknown types ignored; XSS in names stripped

## 7. Risks & Assumptions

- **RISK-001**: WebSocket message ordering under high load — mitigated by Node.js single-threaded event loop (natural serialization)
- **RISK-002**: `better-sqlite3` native addon may fail to compile on some platforms — mitigated by making persistence optional for MVP
- **ASSUMPTION-001**: All clients on same local network with <50ms latency
- **ASSUMPTION-002**: Maximum 16 simultaneous WebSocket connections (well within Node.js/ws capabilities)
- **ASSUMPTION-003**: Game runs on a single machine (laptop/RPi) — no clustering needed

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Findings & Decisions](../docs/findings.md)
- [ws library docs](https://github.com/websockets/ws/blob/master/doc/ws.md)
- [Express.js API](https://expressjs.com/en/4x/api.html)
