# PRD: Roll-a-Ball Derby

> **Version**: 0.1 — Draft  
> **Date**: 2026-04-07  
> **Status**: Discovery Complete → Technical Drafting

---

## 1. Executive Summary

### Problem Statement

Physical "roll-a-ball" racing games (horse derby, camel race) at events lack a digital scoring backbone — points are tracked manually, spectators on a large screen can't follow progress, and there's no way to sync the game to a physical motorized display board automatically.

### Proposed Solution

A self-hosted, local-network multiplayer system with:

1. A **server web app** projected on a beamer/TV showing a live animated race track.
2. **ESP8266 sensor clients** detecting ball rolls into scoring holes via IR break-beam sensors.
3. A **web client** for testing/fallback input (buttons simulating +1 / +3 rolls).
4. An **ESP8266 motor controller** that drives stepper/servo motors to move physical player figures on a board in sync with the digital game state.

### Success Criteria

| KPI | Target |
|-----|--------|
| End-to-end latency (ball enters hole → display updates) | ≤ 300 ms |
| End-to-end latency (game state → motor position sync) | ≤ 500 ms |
| Concurrent players supported | 1–16 |
| Setup time (server + 1 client on local network) | ≤ 5 minutes |
| System uptime during a 4-hour event session | 100% (no crashes/restarts) |

---

## 2. User Experience & Functionality

### User Personas

| Persona | Description |
|---------|-------------|
| **Game Host** | Sets up the server, configures game parameters (player count, track length, theme), starts/resets games. Operates in front of the beamer/TV. |
| **Player** | Rolls a physical ball on the ramp. Doesn't interact with any screen — their input is purely physical. |
| **Spectator** | Watches the race on the big screen (beamer/TV). |
| **Developer/Tester** | Uses the web client to simulate rolls for testing without hardware. |

### User Stories

#### US-1: Game Host — Configure & Start Game
> As a **game host**, I want to configure player count (1–16), track length (default 15), and theme (horse, camel, etc.) so that I can tailor the game to the event.

**Acceptance Criteria:**
- Host can set player count (1–16) via a settings page before starting a game.
- Track length is configurable (min 5, max 50, default 15).
- Theme selector offers at least two themes (horse, camel) with distinct player figures and backgrounds.
- Game cannot start until at least 1 ESP8266 sensor client OR 1 web test client is connected.
- Host can assign player names/labels to lanes.

#### US-2: Game Host — Monitor & Control
> As a **game host**, I want to start, pause, and reset the game so that I can manage the event flow.

**Acceptance Criteria:**
- Start button begins the race; all lanes activate.
- Pause freezes all scoring; display shows "PAUSED".
- Reset returns all players to position 0 and clears scores.
- Game auto-detects a winner when a player reaches the finish line.
- Winner announcement displayed prominently with animation.

#### US-3: Spectator — Watch the Race
> As a **spectator**, I want to see all player lanes and their progress in real-time on the big screen so that I can follow the race excitement.

**Acceptance Criteria:**
- Display shows horizontal stacked lanes, one per player.
- Each player figure moves left-to-right proportionally to their score vs. track length.
- Scoring events trigger a visible animation (flash, bounce) on the player figure.
- Player name/label is visible on each lane.
- Finish line is clearly marked.

#### US-4: Player — Score via Ball Roll
> As a **player**, I want to roll a ball and have my score update automatically when it enters a scoring hole so that I can play without touching any device.

**Acceptance Criteria:**
- IR break-beam sensor detects ball entering +1 hole → score increments by 1.
- IR break-beam sensor detects ball entering +3 hole → score increments by 3.
- Ball returning without entering a hole produces no score change.
- Sensor debounce prevents double-counting (minimum 500 ms between triggers per sensor).
- Audio or visual feedback on the display confirms the score event.

#### US-5: Tester — Simulate Rolls via Web Client
> As a **developer/tester**, I want a web client with buttons to simulate +1 and +3 rolls so that I can test the full system without hardware.

**Acceptance Criteria:**
- Web client shows a player selector dropdown and "+1" / "+3" buttons.
- Button press sends score event identical to an ESP8266 sensor event.
- Multiple web clients can connect simultaneously for multi-player testing.

#### US-6: Physical Board — Motor Sync
> As a **game host**, I want a physical board where motorized player figures move in sync with the digital game state so that the audience sees a tangible race.

**Acceptance Criteria:**
- Motor controller ESP8266 receives position updates from the server.
- Each player figure moves to the correct proportional position on the physical track.
- Movement is smooth (no jitter/overshoot beyond ±2mm).
- Motor controller gracefully handles disconnection and reconnects automatically.
- On game reset, all figures return to position 0.

#### US-7: Player Naming
> As a **client** (ESP8266 or web), I want to optionally send a player name on registration so that the display shows my chosen name — and if I don't, the server picks a fun random name for me.

**Acceptance Criteria:**
- `register` message accepts an optional `playerName` field.
- If `playerName` is provided and non-empty, the server uses it (trimmed, max 20 chars).
- If `playerName` is omitted or empty, the server assigns a random name from a bundled text file (`names.txt`, one name per line).
- Assigned names are unique within the current game session — no two players get the same name.
- Names are displayed on the race lane and in the admin panel.
- The host can override any player name via the admin panel (US-1).
- `names.txt` ships with ≥50 pre-generated fun names (themed: horse/racing puns encouraged).

### Non-Goals (v1)

- ❌ Online/internet multiplayer (local network only).
- ❌ Player accounts, authentication, or persistent leaderboards.
- ❌ Mobile native apps (web client is sufficient).
- ❌ Sound system integration (audio out of scope for MVP).
- ❌ Betting or currency systems.
- ❌ AI-controlled players.
- ❌ ESPHome / Home Assistant integration (see §3.6 for rationale).

---

## 3. Technical Specifications

### 3.1 Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                    Local Network (WiFi)                   │
│                                                          │
│  ┌──────────────┐     WebSocket      ┌───────────────┐  │
│  │ ESP8266       │ ──────────────────▶│               │  │
│  │ Sensor Client │  (score events)    │               │  │
│  │ (per player)  │                    │               │  │
│  └──────────────┘                     │    SERVER     │  │
│                                       │               │  │
│  ┌──────────────┐     WebSocket      │  - Game Logic  │  │
│  │ Web Test      │ ──────────────────▶│  - State Mgmt  │  │
│  │ Client        │  (score events)    │  - Web App     │  │
│  └──────────────┘                     │  - REST API    │  │
│                                       │               │  │
│  ┌──────────────┐     WebSocket      │               │  │
│  │ ESP8266       │ ◀──────────────────│               │  │
│  │ Motor Ctrl    │  (position updates)│               │  │
│  └──────────────┘                     └───────┬───────┘  │
│                                               │          │
│                                       WebSocket│          │
│                                               ▼          │
│                                       ┌───────────────┐  │
│                                       │ Display Client │  │
│                                       │ (Beamer/TV)   │  │
│                                       └───────────────┘  │
└──────────────────────────────────────────────────────────┘
```

**Data Flow:**
1. Ball enters hole → IR sensor triggers → ESP8266 sensor client sends `{ player: N, points: 1|3 }` via WebSocket.
2. Server validates event, updates game state, persists to storage.
3. Server broadcasts updated state to all connected clients (display, web clients, motor controller).
4. Display client animates the player figure to the new position.
5. Motor controller moves the physical figure to the corresponding position.

### 3.2 Tech Stack Comparison

| Aspect | **Node.js + Express + Socket.IO** | **Python + FastAPI + WebSockets** |
|--------|-----------------------------------|-----------------------------------|
| **WebSocket support** | Socket.IO: auto-reconnect, rooms, namespaces, fallback to polling. Battle-tested. | Native WebSockets via `websockets` lib or Starlette. Manual reconnect logic. |
| **ESP8266 client compat** | Arduino WebSocket libs support raw WS. Socket.IO protocol adds overhead; need dedicated Arduino Socket.IO lib OR use raw WS alongside. | Standard WebSocket — direct compatibility with Arduino WebSocket libs. |
| **Real-time perf** | Single-threaded event loop — excellent for I/O-bound real-time apps. | Async with `uvicorn` — comparable performance for this scale. |
| **Persistence** | SQLite via `better-sqlite3` (simple) or JSON file. | SQLite via `aiosqlite` or JSON file. |
| **Frontend serving** | Express serves static files natively. | FastAPI `StaticFiles` mount. |
| **Ecosystem for game UI** | npm ecosystem; can bundle React/Svelte for display. | Serves static frontend; same frontend options. |
| **ESP8266 firmware** | Same Arduino/C++ code regardless of server choice. | Same. |
| **Setup simplicity** | `npm install && npm start` — single runtime. | `pip install && uvicorn` — single runtime. Python often pre-installed. |
| **Self-hosted ease** | Single `node server.js` process. | Single `uvicorn main:app` process. |

**Recommendation**: Both are viable. **Node.js + Express + WS** (raw WebSockets, not Socket.IO) is recommended because:
- Raw WebSocket protocol is simplest for ESP8266 compatibility (no Socket.IO overhead).
- Single JS runtime for both server and frontend build tooling.
- `ws` library is lightweight and ESP8266 Arduino WebSocket libs speak the same protocol natively.

> **Decision: Node.js + Express + `ws`** — confirmed by user. Raw WebSocket protocol, single JS runtime, best ESP8266 compatibility.

### 3.3 Component Specifications

#### Server

| Property | Specification |
|----------|--------------|
| Runtime | Node.js (LTS) + Express + `ws` library |
| Protocol | WebSocket (RFC 6455) for real-time; HTTP REST for config/admin |
| Persistence | SQLite for game history; in-memory for active game state |
| Frontend | Single-page app served by the same process |
| Config | JSON config file or admin web UI |

**REST API Endpoints:**

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/game` | Get current game state |
| `POST` | `/api/game/start` | Start a new game |
| `POST` | `/api/game/pause` | Pause/resume game |
| `POST` | `/api/game/reset` | Reset game to initial state |
| `PUT` | `/api/game/config` | Update game configuration |
| `GET` | `/api/players` | List connected players |
| `PUT` | `/api/players/:id` | Update player name/label |

**WebSocket Messages:**

| Direction | Event | Payload | Description |
|-----------|-------|---------|-------------|
| Client → Server | `score` | `{ playerId: string, points: 1\|3 }` | Ball entered a scoring hole |
| Server → All | `state` | `{ players: [{ id, name, position, connected }], status, config }` | Full game state broadcast |
| Server → All | `scored` | `{ playerId: string, points: number, newPosition: number }` | Score event notification (for animations) |
| Server → All | `winner` | `{ playerId: string, name: string }` | Game over — winner declared |
| Client → Server | `register` | `{ type: "sensor"\|"web"\|"motor", playerId?: string, playerName?: string }` | Client registration on connect; name optional (server assigns random if omitted) |
| Server → Motor | `positions` | `{ players: [{ id, position, maxPosition }] }` | Position update for motor controller |

#### ESP8266 Sensor Client (Input)

| Property | Specification |
|----------|--------------|
| MCU | ESP8266 (NodeMCU / Wemos D1 Mini) |
| Sensors | 2× IR break-beam pairs per player unit (one for +1 hole, one for +3 hole) |
| Protocol | WebSocket client |
| WiFi | Connects to local network (SSID/pass configurable via AP mode on first boot or hardcoded) |
| Debounce | 500 ms minimum between triggers per sensor |
| Reconnect | Auto-reconnect with exponential backoff (1s, 2s, 4s, max 30s) |
| Status LED | Onboard LED indicates connection status (solid = connected, blinking = disconnected) |
| Power | USB 5V or battery pack |

**Pin Mapping (per unit):**

| Pin | Function |
|-----|----------|
| D1 (GPIO5) | IR sensor — +1 hole |
| D2 (GPIO4) | IR sensor — +3 hole |
| Built-in LED | Connection status |

#### ESP8266 Motor Controller (Output)

| Property | Specification |
|----------|--------------|
| MCU | ESP8266 (NodeMCU / Wemos D1 Mini) |
| Motor drivers | Stepper drivers (e.g., A4988 / DRV8825) or servo signal — one per player lane |
| Max motors | Up to 16 (limited by available GPIO; use I²C GPIO expander like MCP23017 for >4) |
| Protocol | WebSocket client (receives position updates from server) |
| Movement | Linear interpolation to target position; speed configurable |
| Homing | On boot / game reset, move all figures to position 0 (limit switch or timed return) |
| Reconnect | Auto-reconnect; on reconnect, request full state and sync all positions |
| Power | External 12V supply for motors; 5V regulated for ESP8266 |

**Expansion Strategy (>4 motors):**

| Motor Count | Approach |
|-------------|----------|
| 1–4 | Direct GPIO pins (D1–D4 for STEP signals, shared DIR) |
| 5–16 | I²C GPIO expander (MCP23017 = 16 extra pins per chip) |
| 16+ | Multiple ESP8266 motor controllers, each handling a subset |

#### Web Test Client

| Property | Specification |
|----------|--------------|
| Type | SPA served by the server |
| Features | Player selector dropdown, "+1" button, "+3" button, connection status indicator |
| Protocol | WebSocket to server |
| Purpose | Development testing and fallback input when ESP hardware is unavailable |

#### Display Client (Beamer/TV)

| Property | Specification |
|----------|--------------|
| Type | SPA served by the server, opened in fullscreen browser on the beamer/TV machine |
| Layout | Horizontal stacked lanes, one per active player |
| Animation | Player figures move smoothly along lanes; scoring flash/bounce effect |
| Theming | Configurable theme: horse race, camel race, custom (different figure sprites & backgrounds) |
| Responsive | Scales to any resolution (1080p, 4K, or projector native) |
| State | Receives full state via WebSocket; no local storage needed |

### 3.4 Integration Points

| Integration | Protocol | Notes |
|-------------|----------|-------|
| ESP8266 ↔ Server | WebSocket over WiFi | All on same local network / SSID |
| Browser ↔ Server | WebSocket + HTTP | Same-origin; server serves the SPA |
| Server ↔ SQLite | Local file I/O | Game history only; active state in memory |

### 3.5 Security & Privacy

- **No authentication required** for MVP (trusted local network).
- Server binds to local network interface only (not `0.0.0.0` in production).
- No personal data collected — player labels are transient.
- WebSocket messages validated server-side (reject malformed payloads, enforce point values of 1 or 3 only).
- Player names sanitized: stripped of HTML/script tags, trimmed, max 20 characters.
- Rate limiting on score events: max 1 event per 300 ms per player to prevent abuse/noise.

### 3.6 ESPHome / Home Assistant — Decision: Not Used

ESPHome + Home Assistant was evaluated and rejected for this project:

| Aspect | ESPHome + HA | Standalone Arduino |
|--------|-------------|-------------------|
| **Latency** | ESP → HA → MQTT/API → game server = 2–3 hops, ~100–500ms added | ESP → WebSocket → server = 1 hop, ~10–50ms |
| **Portability** | Requires HA instance at event venue | Laptop + WiFi AP — grab and go |
| **Motor control** | Custom stepper sequences awkward in YAML; needs custom components | Full C++ control, smooth interpolation |
| **Dependencies** | ESPHome + HA + MQTT broker = 3 extra systems | Zero external dependencies |
| **WiFi config** | ESPHome handles WiFi well (fallback AP) | WiFiManager gives same UX |
| **OTA updates** | ESPHome OTA is excellent | ArduinoOTA is one extra line of code |
| **Bidirectional protocol** | ESPHome native API is not designed for custom game protocols | Raw WebSocket — full control over message schema |

**Verdict**: The game requires direct, low-latency, bidirectional WebSocket communication with a custom message protocol. ESPHome would require custom components to achieve what raw Arduino + WebSocket does natively. Keeping the system standalone maximizes portability and minimizes setup complexity at events.

---

## 4. Hardware Bill of Materials (per player lane)

| Component | Qty | Purpose |
|-----------|-----|---------|
| ESP8266 (Wemos D1 Mini) | 1 | Sensor client per player |
| IR break-beam sensor pair | 2 | Detect ball in +1 and +3 holes |
| USB cable + 5V power source | 1 | Power sensor client |
| **Shared — Motor Controller** | | |
| ESP8266 (Wemos D1 Mini) | 1 | Motor controller (shared for all lanes) |
| Stepper motor (28BYJ-48 or NEMA 17) | 1 per lane | Move player figure |
| Stepper driver (ULN2003 or A4988) | 1 per lane | Drive motor |
| MCP23017 I²C expander | 0–1 | If >4 lanes |
| Limit switch | 1 per lane | Home position detection |
| 12V power supply | 1 | Motor power |
| **Shared — Server** | | |
| Laptop / Raspberry Pi | 1 | Run server |
| WiFi router / hotspot | 1 | Local network |
| Beamer / TV + HDMI | 1 | Display |

---

## 5. Risks & Roadmap

### Phased Rollout

#### MVP (Phase 1)
- Server with in-memory game state, REST API, WebSocket.
- Display client with basic lane animation (single theme: horse).
- Web test client for simulated input.
- Single ESP8266 sensor client (1 player, 2 holes).
- No persistence, no motor controller.

#### v1.0 (Phase 2)
- Multi-player support (up to 8 players).
- Theme selector (horse, camel).
- SQLite persistence (game history).
- ESP8266 sensor client firmware finalized with AP-mode WiFi config.
- Winner detection and announcement animation.
- Host admin panel (configure, start, pause, reset).

#### v1.5 (Phase 3)
- ESP8266 motor controller integration.
- Physical board sync with homing and smooth movement.
- Up to 16 players (I²C expander support).

#### v2.0 (Phase 4)
- Custom theming engine (upload sprites/backgrounds).
- Sound effects (scoring, winner fanfare).
- Tournament mode (multiple rounds, bracket).
- Game replay / highlight reel.

### Technical Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| WiFi latency spikes on crowded event network | Delayed score display | Medium | Use dedicated WiFi AP; keep all devices on isolated SSID |
| IR sensor false triggers (ambient light, reflections) | Phantom scores | Medium | Use modulated IR (38 kHz); add physical light shields around sensors |
| ESP8266 WebSocket disconnect under load | Missed score events | Low | Auto-reconnect with backoff; server-side event acknowledgment |
| Motor drift / position inaccuracy over time | Physical board out of sync | Medium | Limit switches for homing; re-home between games on reset |
| ESP8266 GPIO shortage for >4 motors | Can't support 16 players physically | Low | I²C MCP23017 expander; or multiple ESP8266 motor controllers |
| Power supply noise from motors affecting ESP8266 | Random resets/hangs | Medium | Separate power rails; decoupling capacitors; optoisolation |

---

## 6. Open Decisions

| # | Decision | Options | Status |
|---|----------|---------|--------|
| 1 | Server tech stack | Node.js + Express + WS **vs** Python + FastAPI + WebSockets | **Decided: Node.js + Express + `ws`** |
| 2 | Frontend framework for display | Vanilla JS + Canvas **vs** React/Svelte + CSS animations **vs** Pixi.js/Phaser for game rendering | **Decided: Pixi.js** |
| 3 | Motor type for physical board | 28BYJ-48 steppers (cheap, slow) **vs** NEMA 17 (precise, faster) **vs** Servos (positional, simple) | **Deferred to Phase 3** (physical board build) |
| 4 | ESP8266 WiFi config method | Hardcoded SSID/pass **vs** AP mode captive portal (WiFiManager) | **Decided: WiFiManager AP portal** |
| 5 | ESPHome / Home Assistant integration | Use ESPHome **vs** standalone Arduino firmware | **Decided: Standalone** (see §3.6) |
| 6 | ESP8266 WebSocket client library | Links2004/arduinoWebSockets **vs** gilmaimon/ArduinoWebsockets | **Decided: gilmaimon/ArduinoWebsockets** |

---

*End of PRD — please review and flag any sections that need adjustment.*
