---
goal: LED Control Layer Phase 4 - Web Admin Interface & LED Preview
version: 1.0
date_created: 2026-04-07
last_updated: 2026-04-07
owner: Development Team
status: 'Planned'
tags: [feature, frontend, led-control, phase4, visualization]
---

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

# LED Control Layer Phase 4 — Web Admin Interface & LED Preview

**Feature**: Web admin interface for LED configuration with real-time visual preview/simulator  
**Epic**: LED Control Layer (Phases 1-4)  
**Dependencies**: Phase 3 (Server Configuration & Synchronization) ✅ Complete

---

## Section 1 — Requirements & Constraints

### Functional Requirements

- **REQ-001**: Web admin interface accessible at `/admin` serves LED configuration UI panel
- **REQ-002**: Browser-based LED strip simulator renders WS2812B LED animations in real-time using Canvas or WebGL
- **REQ-003**: Configuration editor allows setting LED count, pin assignment, topology type (strip/matrix/ring) per device
- **REQ-004**: Color picker integrated with existing player color system from `clients/assets/themes/shared/player-colors.json`
- **REQ-005**: Effect library UI displays all available animations with preview thumbnails
- **REQ-006**: Test effect controls allow triggering specific animations on target devices for verification
- **REQ-007**: Device list shows all connected clients with LED capability, hardware status (detected LED count), and live preview
- **REQ-008**: Configuration validation warns admin if LED count mismatch between config and detected hardware
- **REQ-009**: Real-time sync: configuration changes propagate to devices within 500ms of save
- **REQ-010**: LED simulator updates at 30+ FPS to accurately represent device animations

### Non-Functional Requirements

- **REQ-011**: Admin UI built with vanilla JavaScript (no framework) matching existing `clients/web/` architecture
- **REQ-012**: LED simulator runs entirely client-side (no WebGL shader compilation delays > 100ms)
- **REQ-013**: Configuration changes persist immediately to server `data/led-config.json`
- **REQ-014**: Admin panel responsive layout works on 1024px+ screens (laptop/desktop minimum)
- **REQ-015**: LED preview performance: 60 FPS target for strips up to 300 LEDs on modern browsers

### Security Requirements

- **SEC-001**: LED configuration changes allowed only when game state is `idle` (no mid-game reconfiguration)
- **SEC-002**: Test effect endpoint rate-limited to 1 request/sec per device (prevents spam/abuse)
- **SEC-003**: Configuration validation rejects LED counts > 300 per device (server-side limit)

### Constraints

- **CON-001**: Must integrate seamlessly with existing admin.html UI without disrupting current game controls
- **CON-002**: WebSocket message protocol already defined in Phase 3 (`led_config`, `test_effect`) — no protocol changes
- **CON-003**: LED simulator must support strip, matrix (zigzag/progressive), and ring topologies per PRD §4.1
- **CON-004**: No external libraries for LED rendering (Canvas 2D or WebGL only) to minimize load times on local networks

### Guidelines

- **GUD-001**: Follow existing color scheme and layout patterns from `clients/web/css/style.css`
- **GUD-002**: Reuse existing connection management from `clients/web/js/connection.js` (no duplicate WebSocket logic)
- **GUD-003**: Keep LED simulator code modular for potential reuse in display client (future enhancement)
- **GUD-004**: Animation effect names must match server effect library from Phase 2 (solid, blink, pulse, rainbow, chase, sparkle, fire, wave)

### Patterns

- **PAT-001**: Use namespace pattern `window.Derby.LED` for all LED admin module code
- **PAT-002**: Event-driven updates: listen to WebSocket `state` messages for device list changes
- **PAT-003**: Configuration form uses progressive disclosure (basic settings visible, advanced collapsed)

---

## Section 2 — Implementation Steps

### Implementation Phase 4.1 — Core LED Admin UI Structure

**GOAL-001**: Integrate LED configuration panel into existing web admin interface with device list and basic controls

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-001 | Create `clients/web/led.html` as new LED configuration page linked from main admin navigation |           |      |
| TASK-002 | Add LED panel section to existing `clients/web/index.html` admin page (tabbed interface)       |           |      |
| TASK-003 | Create `clients/web/js/led-admin.js` module with `window.Derby.LED` namespace                 |           |      |
| TASK-004 | Implement device list component showing connected devices with LED capability (sensor/motor)    |           |      |
| TASK-005 | Display device metadata: type, player assignment, detected LED count, connection status         |           |      |
| TASK-006 | Add device selection mechanism (click device card to select for configuration)                 |           |      |
| TASK-007 | Integrate with existing WebSocket connection from `clients/web/js/connection.js`               |           |      |

### Implementation Phase 4.2 — LED Configuration Editor

**GOAL-002**: Build configuration form for editing LED settings per device with validation

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-008 | Create configuration form: LED count (input), GPIO pin (dropdown), topology type (select)      |           |      |
| TASK-009 | Populate topology dropdown: strip, matrix_zigzag, matrix_progressive, ring                     |           |      |
| TASK-010 | Add matrix dimensions inputs (rows, cols) shown conditionally when matrix topology selected    |           |      |
| TASK-011 | Implement color scheme editor with player color picker (fetch from `/player-colors.json`)      |           |      |
| TASK-012 | Add brightness slider (0-255) with live preview in simulator                                   |           |      |
| TASK-013 | Validation logic: warn if configured LED count ≠ detected count from device registration       |           |      |
| TASK-014 | Save button sends `PUT /api/leds/config/:deviceType` with updated configuration                |           |      |
| TASK-015 | Display save confirmation message and server validation response                               |           |      |

### Implementation Phase 4.3 — LED Strip Simulator (Canvas Renderer)

**GOAL-003**: Build browser-based LED strip simulator with Canvas 2D rendering at 30+ FPS

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-016 | Create `clients/web/js/led-simulator.js` module with `LEDSimulator` class                     |           |      |
| TASK-017 | Implement Canvas 2D renderer: draw LED strip as horizontal row of circles with spacing         |           |      |
| TASK-018 | Add topology renderers: strip (linear), ring (circular arc), matrix (grid with zigzag mapping)|           |      |
| TASK-019 | Implement pixel buffer abstraction: `setPixel(index, {r, g, b})` with double buffering         |           |      |
| TASK-020 | Add animation loop at 30 FPS using `requestAnimationFrame` with delta time calculation         |           |      |
| TASK-021 | Implement built-in test pattern: rainbow cycle for initial visual verification                 |           |      |
| TASK-022 | Add brightness adjustment: multiply RGB values by brightness factor before rendering           |           |      |
| TASK-023 | Memory optimization: limit maximum rendered LEDs to 300 (matches server constraint)            |           |      |

### Implementation Phase 4.4 — Animation Effect Library Integration

**GOAL-004**: Integrate Phase 2 animation effects into simulator with preview controls

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-024 | Create `clients/web/js/led-effects.js` with client-side implementations of core effects        |           |      |
| TASK-025 | Implement `solid(color)` effect: all LEDs set to single RGB color                              |           |      |
| TASK-026 | Implement `blink(color, speed)` effect: binary on/off at specified interval                    |           |      |
| TASK-027 | Implement `pulse(color, speed)` effect: sinusoidal brightness fade (breathing animation)       |           |      |
| TASK-028 | Implement `rainbow(speed)` effect: HSV hue rotation across strip                               |           |      |
| TASK-029 | Implement `chase(color, speed, size)` effect: moving window of lit LEDs                        |           |      |
| TASK-030 | Implement `sparkle(color, density)` effect: random LEDs flash briefly                          |           |      |
| TASK-031 | Add effect selector dropdown populated from effect library with human-readable labels          |           |      |
| TASK-032 | Effect preview: play selected effect in simulator when dropdown changes                        |           |      |
| TASK-033 | Effect parameter controls: dynamically show sliders for speed/color/size based on effect type  |           |      |

### Implementation Phase 4.5 — Test Effect Controls

**GOAL-005**: Build UI controls for triggering test effects on physical devices with real-time feedback

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-034 | Add "Test on Device" button in configuration panel                                            |           |      |
| TASK-035 | Implement effect test payload builder: effect name + parameters from UI controls               |           |      |
| TASK-036 | Send test effect via `POST /api/leds/test` with target device ID and effect payload           |           |      |
| TASK-037 | Display loading state on button during test (prevent duplicate clicks)                        |           |      |
| TASK-038 | Show confirmation toast/message when server accepts test request                               |           |      |
| TASK-039 | Add "Stop Effect" button to immediately halt currently running test                            |           |      |
| TASK-040 | Rate limit client-side: disable test button for 1 second after each use (matches server limit)|           |      |

### Implementation Phase 4.6 — Real-Time Device Status & Preview

**GOAL-006**: Display live LED state for all connected devices with mini preview indicators

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-041 | Add mini LED preview canvas (50x10px) to each device card in device list                       |           |      |
| TASK-042 | Subscribe to WebSocket `state` messages for device connection status updates                   |           |      |
| TASK-043 | Update device card UI when devices connect/disconnect (add/remove from list dynamically)       |           |      |
| TASK-044 | Show detected LED count badge on device card (fetched from device metadata in `state` message) |           |      |
| TASK-045 | Highlight device card with validation warning color if LED count mismatch detected             |           |      |
| TASK-046 | Add tooltip on device card showing last config sync timestamp and success/failure status       |           |      |
| TASK-047 | Implement "Sync All" button: broadcasts current config to all connected LED-capable devices    |           |      |

### Implementation Phase 4.7 — Styling & Responsive Layout

**GOAL-007**: Polish UI with consistent styling matching existing admin interface

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-048 | Add CSS rules for LED admin panel in `clients/web/css/style.css`                              |           |      |
| TASK-049 | Style device cards with hover states, selection indicator, and status badges                   |           |      |
| TASK-050 | Style configuration form with consistent input sizing and label alignment                      |           |      |
| TASK-051 | Add CSS animations for save confirmation (fade-in/out toast notification)                      |           |      |
| TASK-052 | Ensure simulator canvas scales correctly on different screen sizes (min 1024px width)          |           |      |
| TASK-053 | Add responsive layout for device list: grid on wide screens, vertical stack on narrow          |           |      |
| TASK-054 | Style effect preview selector with thumbnails or color swatches for each effect                |           |      |

### Implementation Phase 4.8 — Documentation & Integration

**GOAL-008**: Document LED admin features and integrate with existing admin workflows

| Task     | Description                                                                                     | Completed | Date |
|----------|-------------------------------------------------------------------------------------------------|-----------|------|
| TASK-055 | Update `clients/web/README.md` with LED admin panel usage instructions                        |           |      |
| TASK-056 | Add LED configuration screenshots to documentation                                             |           |      |
| TASK-057 | Document LED simulator architecture in code comments (topology mapping, rendering pipeline)    |           |      |
| TASK-058 | Add troubleshooting section for common LED issues (count mismatch, no preview, etc.)           |           |      |
| TASK-059 | Update server README API section with links to LED admin UI routes                             |           |      |
| TASK-060 | Create LED admin quick start guide for game hosts (minimal technical jargon)                   |           |      |

---

## Section 3 — Alternatives

- **ALT-001**: Use React/Vue for LED admin UI instead of vanilla JS
  - **Rejected**: Main PRD §3.2 and existing codebase uses vanilla JS with namespace pattern; introducing framework adds build complexity and breaks architecture consistency
  
- **ALT-002**: WebGL renderer instead of Canvas 2D for LED simulator
  - **Consideration**: Provides better performance for 300+ LEDs, enables shader-based effects
  - **Decision**: Defer to optimization phase; Canvas 2D sufficient for 30 FPS at 300 LEDs based on benchmarks; WebGL adds ~5KB overhead and initialization complexity

- **ALT-003**: Separate `/led` route instead of integrating into main `/admin` page
  - **Rejected**: Creates fragmented admin experience; LED config is core game setup, should be alongside player/game settings; decision per TASK-001 notes

- **ALT-004**: Use Socket.IO for real-time updates instead of WebSocket state broadcasts
  - **Rejected**: Main PRD §3.2 explicitly chose raw WebSocket; introducing Socket.IO for admin only creates dual protocol complexity

- **ALT-005**: Server-side LED preview generation (send pre-rendered frames to admin)
  - **Rejected**: Adds server CPU load, increases network traffic; client-side rendering keeps server stateless and scales to multiple concurrent admins

- **ALT-006**: D3.js or Three.js for physical layout editor (drag-drop LED strip positioning)
  - **Deferred to v2.0**: PRD §5 mentions "drag-drop designer" but not in MVP scope; Phase 4 focuses on configuration form, visual layout editor is future enhancement

---

## Section 4 — Dependencies

- **DEP-001**: Phase 3 complete with `/api/leds/*` REST endpoints functional
- **DEP-002**: `LedConfigManager` (Phase 3) running and persisting to `server/data/led-config.json`
- **DEP-003**: WebSocket protocol extensions (`led_config`, `test_effect`) implemented in `ConnectionManager.js`
- **DEP-004**: Device registration protocol includes LED metadata (chip type, detected LED count)
- **DEP-005**: `clients/assets/themes/shared/player-colors.json` exists with color definitions
- **DEP-006**: Existing admin web client architecture (`clients/web/js/connection.js`, `state.js`, `admin.js`)
- **DEP-007**: Server REST API returns device list with LED capability flags in `/api/game` state response
- **DEP-008**: Animation effect names standardized in Phase 2 effect library (referenced in simulator)

---

## Section 5 — Files

### Created Files

- **FILE-001**: `clients/web/js/led-admin.js` — created — LED admin panel module with device list, config editor, test controls
- **FILE-002**: `clients/web/js/led-simulator.js` — created — Canvas-based LED strip/matrix/ring renderer with animation loop
- **FILE-003**: `clients/web/js/led-effects.js` — created — Client-side effect implementations (solid, blink, pulse, rainbow, chase, sparkle)
- **FILE-004**: `clients/web/css/led-admin.css` — created — Stylesheet for LED configuration panel, device cards, simulator canvas (alternative: inline in existing style.css)

### Modified Files

- **FILE-005**: `clients/web/index.html` — modified — Add LED configuration panel section with tabbed interface or accordion
- **FILE-006**: `clients/web/css/style.css` — modified — Add CSS rules for LED admin components (if not creating separate led-admin.css)
- **FILE-007**: `clients/web/js/main.js` — modified — Initialize LED admin module, connect to existing WebSocket state updates
- **FILE-008**: `clients/web/js/state.js` — modified — Parse LED-related fields from server state messages (device LED metadata)
- **FILE-009**: `clients/web/README.md` — modified — Add LED admin features documentation section
- **FILE-010**: `server/README.md` — modified — Add link to LED admin UI from API documentation section

---

## Section 6 — Testing

### Automated Tests

- **TEST-001**: Unit test for `LEDSimulator.setPixel()` — verify pixel buffer correctly updated with RGB values
- **TEST-002**: Unit test for topology mapping — verify index 0 maps to correct canvas position in strip/matrix/ring modes
- **TEST-003**: Animation frame rate test — measure FPS for 300 LED strip, assert ≥30 FPS on Chrome/Firefox
- **TEST-004**: Effect library validation — verify all 7 core effects (solid, blink, pulse, rainbow, chase, sparkle, fire, wave) render without errors
- **TEST-005**: Configuration save API test — POST valid LED config, verify server responds 200 and persists to `led-config.json`
- **TEST-006**: Validation logic test — configure LED count 100, mock device reports 50 detected, assert warning displayed in UI

### Manual Tests

- **TEST-007**: Device list updates in real-time — connect ESP8266 sensor, verify device card appears within 2 seconds
- **TEST-008**: Configuration editor workflow — select device, change LED count to 60, save, verify no errors and confirmation shown
- **TEST-009**: LED simulator visual accuracy — play rainbow effect, compare to physical device output, assert colors match
- **TEST-010**: Test effect on physical device — select "pulse" effect, click "Test on Device", verify sensor's LEDs pulse blue
- **TEST-011**: Multiple devices tested — connect 3 sensor clients, configure different LED counts (30, 50, 100), verify simulator shows each correctly
- **TEST-012**: Validation warning — configure device with 200 LEDs but device reports 150 detected, verify orange warning badge shown
- **TEST-013**: Rate limiting — click "Test on Device" 5 times rapidly, verify button disabled after first click for 1 second
- **TEST-014**: Responsive layout — resize browser to 1024px width, verify LED panel layout does not break or overflow
- **TEST-015**: WebSocket reconnection — disconnect server, verify device list shows "disconnected" states, reconnect, verify updates resume

### Integration Tests

- **TEST-016**: End-to-end configuration flow — save LED config in admin → verify server broadcasts `led_config` → mock ESP8266 receives message
- **TEST-017**: Sync on device registration — register new sensor with 40 LEDs → verify admin panel immediately shows device with correct count
- **TEST-018**: Effect test propagation — trigger "rainbow" effect from admin → verify server sends `test_effect` message to target device only
- **TEST-019**: Multi-admin scenario — open admin panel in 2 browsers, change config in browser A, verify browser B sees updated device list
- **TEST-020**: Game state lock — start game (state = "playing"), attempt to save LED config, verify server rejects with 409 Conflict

---

## Section 7 — Risks & Assumptions

### Risks

- **RISK-001**: Canvas rendering performance degrades below 30 FPS on older browsers or low-end hardware
  - *Mitigation*: Profile on minimum spec hardware (5-year-old laptop), add frame skip logic if delta time exceeds 50ms, fallback to static preview if <15 FPS
  
- **RISK-002**: LED count mismatch between config and hardware causes confusion for non-technical game hosts
  - *Mitigation*: Clear visual warnings with actionable text ("Device detected 50 LEDs but config says 100. Update config or check wiring?"), color-coded badges (green=match, orange=mismatch)

- **RISK-003**: WebSocket state messages become large with many devices and frequent updates, impacting admin UI responsiveness
  - *Mitigation*: Server sends incremental updates (only changed devices), admin UI debounces re-renders to every 100ms, limit device list to 16 devices (system max)

- **RISK-004**: Client-side effect implementations diverge from actual device firmware effects, causing preview/reality mismatch
  - *Mitigation*: Share effect parameters in JSON schema between server and client, document exact algorithms in Phase 2, visual regression testing with screenshots

- **RISK-005**: Game hosts attempt to change LED config mid-game causing devices to flicker or lose state
  - *Mitigation*: Disable all config save buttons when game state ≠ idle, show clear message "LED configuration locked during active game"

- **RISK-006**: Test effect flooding: rapid clicking sends too many WebSocket messages, overwhelming server/devices
  - *Mitigation*: Client-side rate limit (button disabled 1sec), server-side rate limit (1 req/sec per device), queue effects if rapid clicks occur

### Assumptions

- **ASSUMPTION-001**: Device firmware (ESP8266/ESP32) correctly reports detected LED count in registration payload (coordinated implementation)
- **ASSUMPTION-002**: Admin user has basic technical knowledge (understands "GPIO pin", "LED count" terminology)
- **ASSUMPTION-003**: At most 16 devices connected simultaneously (system max per main PRD), device list UI does not need pagination
- **ASSUMPTION-004**: Game hosts configure LEDs before first game start, not during active event (config is "setup" activity)
- **ASSUMPTION-005**: Browser supports Canvas 2D API and `requestAnimationFrame` (all modern browsers do)
- **ASSUMPTION-006**: Phase 2 animation effects follow naming convention and parameter schema consistent with Phase 4 client-side implementations
- **ASSUMPTION-007**: Network latency on local network is <50ms (fast LED config propagation without buffering/retry logic)

---

## Section 8 — Related Specifications / Further Reading

- [`docs/PRD-LED-Control-Layer.md`](../docs/PRD-LED-Control-Layer.md) — Product requirements for LED Control Layer (Phases 1-4)
- [`docs/PRD.md`](../docs/PRD.md) — Main Roll-a-Ball Derby product requirements (architecture, tech stack decisions)
- [`plan/feature-led-control-phase3-1.md`](./feature-led-control-phase3-1.md) — Phase 3: Server Configuration & Synchronization (prerequisite)
- [`plan/feature-led-control-phase2-1.md`](./feature-led-control-phase2-1.md) — Phase 2: Animation Engine & Effects Library (reference for effect names)
- [`plan/feature-led-control-phase1-1.md`](./feature-led-control-phase1-1.md) — Phase 1: Core Abstraction Layer (hardware context)
- [`clients/web/README.md`](../clients/web/README.md) — Web client architecture and conventions
- [`server/README.md`](../server/README.md) — Server API documentation including LED endpoints
- [Canvas 2D API](https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D) — MDN reference for rendering implementation
- [requestAnimationFrame](https://developer.mozilla.org/en-US/docs/Web/API/window/requestAnimationFrame) — Animation loop best practices
- [WS2812B Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf) — LED protocol timing specifications (for accurate simulation)
