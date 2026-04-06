# Findings & Decisions — Roll-a-Ball Derby

## Requirements (from PRD)
- Self-hosted, local-network, 1–16 players
- Server web app: game logic + display + admin
- ESP8266 sensor clients: IR break-beam, WebSocket, debounce
- ESP8266 motor controller: stepper/servo, WebSocket, I²C expander
- Web test client: buttons for simulated rolls
- Display: horizontal stacked lanes, themeable (horse/camel), beamer/TV
- ≤300ms sensor→display latency, ≤500ms state→motor latency
- Player naming: optional from client, random from names.txt

## Open Decisions (from PRD §6)
| # | Decision | Options | Status |
|---|----------|---------|--------|
| 1 | Server stack | Node.js + WS vs Python + FastAPI | Researching |
| 2 | Frontend framework | Canvas vs React/Svelte vs Pixi.js/Phaser | Researching |
| 3 | Motor type | 28BYJ-48 vs NEMA 17 vs Servos | Researching |
| 4 | WiFi config | Hardcoded vs WiFiManager AP portal | Researching |

## Research Findings

### Server Stack

#### Node.js + Express + `ws`
- **`ws` library**: 22.7k GitHub stars, blazing fast, RFC 6455 compliant, thoroughly tested (Autobahn test suite). Minimal API surface.
- Native WebSocket protocol — no custom framing overhead (unlike Socket.IO).
- ESP8266 Arduino WebSocket libs (Links2004, gilmaimon) speak raw WS natively → zero protocol mismatch.
- Node.js 22+ has experimental built-in WebSocket client (not server yet) — `ws` remains the standard server lib.
- Express serves static files (display SPA, test client) out of the box.
- Single JS runtime for server + frontend tooling (if using build step).
- `better-sqlite3` for persistence is synchronous and simple.
- Setup: `npm install && node server.js` — single command.

#### Python + FastAPI + WebSockets
- FastAPI has native WebSocket support via Starlette (built on `websockets` lib).
- Async with `uvicorn` — comparable perf for this scale (1-16 clients).
- More boilerplate for connection management (must manually track connected clients, handle reconnect).
- Python often pre-installed, but needs virtual env management for deps.
- `aiosqlite` for async SQLite access.
- Setup: `pip install -r requirements.txt && uvicorn main:app` — single command.
- Fewer ESP8266 + Python WebSocket "game server" examples in the wild vs Node.js.

#### Verdict
Both work. **Node.js + `ws`** has the edge because:
1. Raw WS protocol = simplest ESP8266 compat (no adapter layer)
2. More community examples for WebSocket game servers + ESP8266 clients
3. Single language (JS) for server + display client if desired
4. `ws` is battle-tested at scale (22.7k stars, Autobahn-certified)

### ESP8266 Libraries

#### WebSocket Client Libraries
| Library | Stars | ESP8266 | ESP32 | Client | Server | Reconnect | SSL |
|---------|-------|---------|-------|--------|--------|-----------|-----|
| **Links2004/arduinoWebSockets** | ~2.5k | ✓ | ✓ | ✓ | ✓ | Manual | ✓ |
| **gilmaimon/ArduinoWebsockets** | ~1.5k | ✓ | ✓ | ✓ | ✓ | Manual | ✓ |

- **Links2004**: Older, more battle-tested, used in most ESP8266 tutorials. Has some reported stability issues at high message rates (>2/sec per one SO report — but that's 2017-era).
- **gilmaimon**: Modern C++ API with lambda callbacks, cleaner event handling, RFC-6455 complete (fragmentation, ping/pong). Smaller footprint.
- **Recommendation**: **gilmaimon/ArduinoWebsockets** — cleaner API, modern C++, good ESP8266 support, easier callback patterns. Both work fine for our low message rate (~1 msg/500ms max per client).

#### WiFi Configuration
- **WiFiManager (tzapu/tablatronix)**: De-facto standard. Falls back to AP mode with captive portal if WiFi creds not configured. One library call: `wifiManager.autoConnect("Derby-SensorX")`. Custom parameters supported (e.g., server IP).
- **Hardcoded**: Simpler code but requires re-flash for every network change. Not practical at events.
- **Recommendation**: **WiFiManager** — essential for event portability. Game host connects to AP, enters WiFi creds once per device.

#### OTA Updates
- **ArduinoOTA**: Built into ESP8266 Arduino core. ~5 lines of setup code. Allows re-flashing over WiFi without USB cable.
- Recommended for v1.0+ (not MVP).

#### IR Sensor Debounce
- Hardware: IR break-beam sensors (e.g., Adafruit #2167) are digital — already clean signal.
- Software: Simple `millis()` debounce with 500ms window per pin. No library needed.
- Best practice: Use `FALLING` interrupt with `detachInterrupt` + timer re-enable for robust debounce.

### Frontend Display

#### Pixi.js
- 43k GitHub stars, 4M+ npm downloads/month
- Pure rendering library — ~450KB minified (3x smaller than Phaser)
- 2x faster pure rendering performance
- No built-in physics, audio, scene management, or input handling
- WebGL with Canvas fallback
- Maximum control over rendering pipeline
- Would need to build scene management, tweening, asset loading manually

#### Phaser 3
- 36k GitHub stars, 2M+ npm downloads/month
- Full game framework — ~1.2MB minified
- Built-in: physics (Arcade/Matter), scene management, tweens, audio, sprite sheets, input handling, camera
- Uses PixiJS v3 internally for rendering (Phaser 3 has own renderer based on it)
- Huge tutorial ecosystem, game-specific examples
- Overkill for non-interactive display? We only need: load sprites, move them on lanes, animate scoring

#### Vanilla Canvas + CSS
- Zero dependencies, smallest bundle
- Full control, but must implement everything: sprite loading, animation loop, scaling, tweening
- Good for simple progress bars, but sprite animation code adds up
- No particle effects / fancy transitions without effort

#### Assessment for our use case
Our display is **not an interactive game** — it's a passive visualization:
- Load themed sprites (horse/camel per lane)
- Move sprites horizontally on scoring events (smooth tween)
- Flash/bounce animation on score
- Winner celebration animation
- Responsive scaling to beamer resolution

**Pixi.js** is the sweet spot:
- Small bundle (450KB) — loads fast on beamer machine
- WebGL sprite rendering is buttery smooth
- We only need: `Sprite`, `Container`, `Ticker`, and basic tweening (use `gsap` or write our own)
- No wasted overhead from Phaser's physics/audio/input we don't need
- Theming = swapping texture atlases (built-in Pixi concept)

**Alternative**: For absolute simplicity, pure **CSS animations + DOM elements** could work for MVP (div per lane, CSS transform for position). Then upgrade to Pixi if we need fancier effects.

### Motor Control

#### 28BYJ-48 Stepper + ULN2003
- **Cheap**: ~$2-3 per motor+driver set
- **Precision**: 2048 steps/revolution, very precise positioning
- **Speed**: Slow (~15 RPM max). With belt/pulley, ~50mm/s linear speed
- **Torque**: Higher than SG90 servo, good for pulling figures on a track
- **Full rotation**: Can rotate continuously in steps
- **Driver**: ULN2003 — 4 GPIO pins per motor (IN1-IN4)
- **Power**: 5V — can share ESP power supply for 1-2 motors, separate for more
- **Library**: `AccelStepper` — acceleration/deceleration, position tracking

#### NEMA 17 + A4988/DRV8825
- **Expensive**: ~$10-15 per motor + driver
- **Precision**: 200 steps/rev (1.8°), up to 3200 with microstepping
- **Speed**: Much faster (~100+ RPM), good linear speed
- **Torque**: High (up to 0.5 Nm)
- **Driver**: A4988/DRV8825 — only 2 GPIO pins per motor (STEP + DIR)
- **Power**: 8-35V — requires separate power supply
- **Overkill** for moving lightweight figures on a ~1m track

#### SG90 Servo
- **Cheap**: ~$2
- **Limitation**: 0-180° range only — NOT suitable for linear track positioning (would need rack and pinion or limited travel)
- **No full rotation** in standard config
- **Not recommended** for this use case unless continuous rotation servo variant

#### Recommendation: Start with 28BYJ-48 + ULN2003
- Cheapest option for prototype
- 5V power simplifies wiring
- AccelStepper library handles smooth movement
- **Problem**: Takes 4 GPIO pins per motor → only 2-3 motors per bare ESP8266
- **Solution**: MCP23017 I²C expander (16 extra GPIO, needs only 2 I²C pins from ESP)
- Upgrade path: Switch to NEMA 17 + A4988 (2 pins/motor) if speed/torque insufficient

#### MCP23017 I²C Expander
- Adafruit MCP23017 library well-supported on ESP8266
- 16 GPIO pins per chip, addressable via I²C (up to 8 chips = 128 pins)
- Can directly drive ULN2003 inputs from expander pins
- **Timing concern**: I²C communication adds latency to step pulses. For 28BYJ-48 slow speed this is fine. For fast NEMA 17 stepping, direct GPIO or shift registers preferred.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Standalone Arduino (not ESPHome) | Low latency, portability, no HA dependency (PRD §3.6) || **Node.js + Express + `ws`** | Raw WS protocol, single JS runtime, 22.7k star lib, best ESP compat, most community examples |
| **Pixi.js** for display | 450KB (3x smaller than Phaser), 2x faster rendering, no unused physics/audio overhead. Only need sprite + tween |
| **WiFiManager AP portal** | One-call setup, captive portal for WiFi creds at events, essential for portability |
| **gilmaimon/ArduinoWebsockets** | Modern C++ API, lambda callbacks, RFC-6455 complete, good ESP8266 support |
| Motor type **deferred** | Decision postponed to Phase 3 (physical board build) — needs physical track dimensions first |
## Issues Encountered
| Issue | Resolution |
|-------|------------|

## Resources
- PRD: docs/PRD.md
- ws library: https://github.com/websockets/ws (22.7k stars)
- gilmaimon/ArduinoWebsockets: https://github.com/gilmaimon/ArduinoWebsockets
- Links2004/arduinoWebSockets: https://github.com/Links2004/arduinoWebSockets
- WiFiManager: https://github.com/tzapu/WiFiManager
- Pixi.js: https://pixijs.com/ (43k stars)
- Phaser: https://phaser.io/ (36k stars)
- AccelStepper: https://www.airspayce.com/mikem/arduino/AccelStepper/
- MCP23017 Adafruit lib: https://github.com/adafruit/Adafruit-MCP23017-Arduino-Library
- RandomNerdTutorials ESP8266 stepper: https://randomnerdtutorials.com/stepper-motor-esp8266-websocket/
- Pixi vs Phaser comparison: https://generalistprogrammer.com/comparisons/phaser-vs-pixijs
