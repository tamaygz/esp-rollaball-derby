---
goal: "Implement ESP32 peripheral controller — motors, LED matrix, buttons, sound — with graceful degradation"
version: 2.0
date_created: 2026-04-11
last_updated: 2026-04-11
owner: "@tamaygz"
status: "In Progress"
tags: [feature, firmware, esp32, arduino, platformio, motor, led-matrix, sound, buttons]
supersedes: "feature-client-esp8266-motor-1.md"
---

# Client ESP32 Peripheral Controller — Implementation Plan v2

![Status: In Progress](https://img.shields.io/badge/status-In%20Progress-yellow)

PlatformIO project for **ESP32** peripheral controller firmware. Receives game state from the server via WebSocket and drives motors, LED matrix, buttons, and Bluetooth audio. Each subsystem is optional — the client degrades gracefully (e.g. works as LED matrix + BT sound client without motors attached).

**Key changes from v1:**
- Platform switched from ESP8266 to ESP32 (more GPIOs, dual core, I2S, 520KB SRAM)
- Expanded scope: motors + LED matrix + control buttons + Bluetooth audio (A2DP to external speaker)
- Sound files hosted on game server, fetched via HTTP, streamed to BT speaker
- Motor calibration/setup routine accessible via admin UI — configure steps per track, calibrate start/end points per lane
- Physical motors have hard-assigned colors — server must reassign player/device colors to match
- Graceful degradation: every subsystem operates independently
- Server web admin (devices page) hosts unified interface for BT speaker management + motor calibration

## 1. Requirements & Constraints

### Functional — Core

| ID | Description | Priority |
|----|-------------|----------|
| REQ-001 | Connect to WiFi using WiFiManager captive portal (AP mode on first boot) | Must |
| REQ-002 | Discover server via mDNS (`_derby._tcp`) with fallback to configured IP/port | Must |
| REQ-003 | Register as `{ type: "motor" }` over WebSocket, include `chipId` (ESP32 MAC-based hex) | Must |
| REQ-004 | Receive and process `positions`, `scored`, `state`, `countdown`, `game_event`, `winner`, `led_config`, `test_effect` messages | Must |
| REQ-005 | Auto-reconnect WebSocket with exponential backoff (1 s → 30 s max) | Must |
| REQ-006 | Status LED: solid = WS connected, slow blink = WiFi only, fast blink = no WiFi | Must |
| REQ-007 | Persist runtime state (playerId, LED config, device color) to LittleFS with atomic writes | Must |

### Functional — Motors

| ID | Description | Priority |
|----|-------------|----------|
| REQ-010 | Drive 1–8 28BYJ-48 stepper motors (via ULN2003 driver boards) to physical positions proportional to game state | Must |
| REQ-011 | Non-blocking motor control using AccelStepper (smooth accel/decel) | Must |
| REQ-012 | Homing sequence on boot using limit switches (one per lane) — set position 0 | Must |
| REQ-013 | On game reset (all positions = 0), home all motors | Must |
| REQ-014 | Report available motor count in registration payload (`motorCount`) | Must |
| REQ-015 | Each physical motor has a hard-assigned color (wired/painted) — report `motorColors` array (color indices) in registration so server can align player colors with physical hardware | Must |
| REQ-016 | Abstract motor interface — support stepper or servo by config | Should |
| REQ-017 | Configuration via WiFiManager: number of motors, total track steps, motor type | Should |

### Functional — Motor Calibration & Setup

| ID | Description | Priority |
|----|-------------|----------|
| REQ-080 | Each motor lane has independent calibration: start position (steps), end position (steps), total track steps | Must |
| REQ-081 | Calibration routine: operator manually jogs motor forward/backward to find start point → save; jog to find end point → save; steps between = track length | Must |
| REQ-082 | Jog motor: move N steps forward or backward at configurable speed (for manual positioning during calibration) | Must |
| REQ-083 | Move-to-step(N): command a specific motor to move to absolute step position N | Must |
| REQ-084 | Reset/home motor: return a specific motor or all motors to start position (step 0 / limit switch) | Must |
| REQ-085 | Persist calibration data per lane to LittleFS: `{ laneId, startStep, endStep, totalTrackSteps, stepsPerMm, direction, maxSpeed, acceleration }` | Must |
| REQ-086 | REST API on ESP32 for calibration: `/api/motor/status`, `/api/motor/jog`, `/api/motor/moveto`, `/api/motor/home`, `/api/motor/calibrate`, `/api/motor/config` | Must |
| REQ-087 | Server admin UI (devices page): motor calibration panel per connected motor client — jog controls, save start/end, view/edit per-lane config | Must |
| REQ-088 | Position mapping uses calibrated start/end: `targetStep = startStep + (position / trackLength) * (endStep - startStep)` | Must |
| REQ-089 | Motor direction configurable per lane (CW/CCW) — string pull mechanism may require reverse direction | Must |
| REQ-090 | Max speed and acceleration configurable per lane via calibration UI | Should |
| REQ-091 | Live position feedback: ESP32 reports current step position per motor via REST or WS | Should |

### Functional — Color Synchronisation

| ID | Description | Priority |
|----|-------------|----------|
| REQ-020 | Send `motorColors: [colorIndex, ...]` in registration payload — one per lane, ordered by lane | Must |
| REQ-021 | Server uses `motorColors` to assign/reassign player `colorIndex` values so each player's digital color matches the physical motor lane color | Must |
| REQ-022 | Admin UI shows motor color assignments and allows manual override | Should |
| REQ-023 | On motor client reconnect, server re-validates color assignments | Must |

### Functional — Control Buttons

| ID | Description | Priority |
|----|-------------|----------|
| REQ-030 | 1–2 physical buttons for game actions (configurable mapping) | Must |
| REQ-031 | Default mapping: Button 1 = Start/Reset (start if idle, reset if finished), Button 2 = Pause/Resume | Should |
| REQ-032 | Send button press as `{ type: "button", payload: { button: 1\|2, action: "start"\|"reset"\|"pause" } }` over WebSocket | Must |
| REQ-033 | Server recognises `button` message type from motor clients and executes corresponding game action | Must |
| REQ-034 | Debounce buttons (200 ms) to prevent double triggers | Must |
| REQ-035 | Visual/audio feedback on press (LED flash + optional click sound) | Should |

### Functional — LED Matrix

| ID | Description | Priority |
|----|-------------|----------|
| REQ-040 | Drive a WS2812B LED matrix (8×8 or configurable rows × cols) | Must |
| REQ-041 | Display countdown numbers (3, 2, 1, GO!) during `countdown` messages | Must |
| REQ-042 | Display player name on `took_lead` event (scrolling text) | Must |
| REQ-043 | Display winner name/animation on `winner` message | Must |
| REQ-044 | Idle animation when game is idle (rainbow, device-color breathe) | Should |
| REQ-045 | LED matrix topology configurable: `matrix_zigzag` or `matrix_progressive` wiring | Must |
| REQ-046 | Use shared LED control library (`clients/shared/leds/`) for effects and animation management | Must |
| REQ-047 | Receive LED config from server via `led_config` WS message (pin, brightness, topology, count) | Must |

### Functional — Sound (Bluetooth A2DP)

| ID | Description | Priority |
|----|-------------|----------|
| REQ-050 | Stream audio to a Bluetooth speaker via ESP32 A2DP Source profile | Must |
| REQ-051 | Sound files (WAV/MP3) hosted on game server under `/assets/sounds/` — ESP32 fetches via HTTP and caches in PSRAM/SPIFFS | Must |
| REQ-052 | Countdown sounds: tick for 3/2/1, longer tone for GO! | Must |
| REQ-053 | Score sounds: short beep (+1), double beep (+2), triple beep (+3), sad tone (0) | Should |
| REQ-054 | Winner fanfare on `winner` message | Must |
| REQ-055 | Volume configurable (0–100%) via A2DP AVRCP or amplitude scaling | Should |
| REQ-056 | Sound module degrades silently if no BT speaker paired/connected | Must |
| REQ-057 | Fallback: if no BT speaker available, optionally play through local I2S DAC or PWM buzzer | Should |

### Functional — Bluetooth Management

| ID | Description | Priority |
|----|-------------|----------|
| REQ-070 | ESP32 acts as Bluetooth A2DP Source (audio sender) | Must |
| REQ-071 | Scan for nearby BT speakers / audio sinks on demand | Must |
| REQ-072 | Pair with a selected BT speaker; persist pairing in NVS | Must |
| REQ-073 | Auto-connect to last paired speaker on boot | Must |
| REQ-074 | Unpair / forget a saved speaker | Must |
| REQ-075 | Report BT connection status in registration `capabilities`: `{ bt: { paired: true, connected: true, deviceName: "JBL Flip" } }` | Must |
| REQ-076 | Admin UI: BT management panel — scan, pair, unpair, show connection status | Must |
| REQ-077 | REST API on ESP32 (`/api/bt/scan`, `/api/bt/pair`, `/api/bt/unpair`, `/api/bt/status`) for admin UI integration | Must |
| REQ-078 | BT and WiFi coexistence — ESP32 supports simultaneous WiFi + BT Classic | Must |

### Functional — Graceful Degradation

| ID | Description | Priority |
|----|-------------|----------|
| REQ-060 | Each subsystem (motors, LED matrix, buttons, sound) initialises independently | Must |
| REQ-061 | If no motors detected (motorCount = 0 in config), skip motor init — continue as LED/button/sound client | Must |
| REQ-062 | If no LED matrix (ledCount = 0), skip LED init | Must |
| REQ-063 | If no BT speaker paired/connected, skip sound playback (silent mode) | Must |
| REQ-064 | Registration payload includes `capabilities` object listing available subsystems: `{ motors: 4, motorCalibrated: [true, ...], leds: 64, buttons: 2, sound: true, bt: { paired: true, connected: true, deviceName: "..." } }` | Must |

### Security & Constraints

| ID | Description |
|----|-------------|
| SEC-001 | WiFi credentials stored in ESP32 NVS/flash — not in source code |
| SEC-002 | Validate all incoming WebSocket JSON: parse, check expected fields, reject malformed |
| CON-001 | PlatformIO project, platform `espressif32`, board `esp32dev` (or configurable) |
| CON-002 | ESP32 Arduino framework |
| CON-003 | `gilmaimon/ArduinoWebsockets` for WS client |
| CON-004 | `tzapu/WiFiManager` (ESP32-compatible) for WiFi config |
| CON-005 | `waspinator/AccelStepper` for non-blocking stepper control (HALF4WIRE mode for 28BYJ-48) |
| CON-006 | Shared LED library (`clients/shared/leds/`) for NeoPixelBus, effects, animation |
| CON-007 | 28BYJ-48 + ULN2003 requires 4 GPIO pins per motor (IN1–IN4) — max ~8 motors on ESP32 while reserving pins for LED, buttons, I2S, status LED |

### Guidelines

| ID | Description |
|----|-------------|
| PAT-001 | Non-blocking state machine in `loop()`: poll WS → update subsystems → yield |
| PAT-002 | Abstract motor interface for stepper/servo swapping |
| PAT-003 | Subsystem manager pattern: each peripheral has `begin()`, `loop()`, `isAvailable()` |
| PAT-004 | Mirror sensor firmware patterns: config.h, websocket.h/cpp, led.h/cpp, state persistence |

## 2. Implementation Steps

### Phase 1: Project Setup, WiFi & mDNS

GOAL-001: PlatformIO project for ESP32, WiFiManager with server IP/port, mDNS discovery.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-001 | Create `clients/esp32-motor/platformio.ini` — board `esp32dev`, framework `arduino`, lib_deps: ArduinoWebsockets, WiFiManager, ArduinoJson, AccelStepper, NeoPixelBus. Build flags: `-I../../shared` | | `clients/esp32-motor/platformio.ini` |
| TASK-002 | Create `clients/esp32-motor/src/config.h` — pin definitions, defaults, `LedConfig` struct, `MotorConfig` struct, `SoundConfig` struct, `BtConfig` struct | | `clients/esp32-motor/src/config.h` |
| TASK-003 | Create `clients/esp32-motor/src/main.cpp` — `setup()` / `loop()`, WiFiManager with AP name `"Derby-Motor-XXXX"`, mDNS discovery (`ESPmDNS.h`, query `_derby._tcp`), config load/save to LittleFS | | `clients/esp32-motor/src/main.cpp` |
| TASK-004 | WiFiManager custom parameters: `server_ip`, `server_port`, `player_name`, `motor_count`, `led_count` | | `clients/esp32-motor/src/main.cpp` |
| TASK-005 | State persistence: atomic write to LittleFS (playerId, LED config, device color) — mirror sensor firmware pattern | | `clients/esp32-motor/src/main.cpp` |

### Phase 2: WebSocket Connection & Protocol

GOAL-002: Connect to server, register as motor with capabilities, handle all message types.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-010 | Create `clients/esp32-motor/src/websocket.h/.cpp` — WS client wrapper, connect/reconnect with exponential backoff | | `clients/esp32-motor/src/websocket.h`, `websocket.cpp` |
| TASK-011 | Registration payload: `{ type: "motor", playerName, playerId, chipId, ledCount, chipType: "ESP32", motorCount, motorColors, capabilities }` | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-012 | Handle `registered` response — store playerId, name, colorIndex | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-013 | Handle `positions` → dispatch to motor subsystem | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-014 | Handle `scored` → dispatch local events to LED matrix + sound | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-015 | Handle `countdown` → dispatch to LED matrix (numbers) + sound (beeps) | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-016 | Handle `game_event` (`game_started`, `game_paused`, `game_resumed`, `game_reset`) → dispatch to all subsystems | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-017 | Handle `winner` → dispatch to LED matrix (winner name) + sound (fanfare) + motors (optional celebration move) | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-018 | Handle `led_config` → update LED matrix config, persist to state | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-019 | Handle `test_effect` → play effect on LED matrix | | `clients/esp32-motor/src/websocket.cpp` |

### Phase 3: Motor Subsystem

GOAL-003: Abstract motor control, map positions to steps, drive steppers non-blocking.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-020 | Create `clients/esp32-motor/src/motor_interface.h` — abstract base with `moveTo(targetSteps)`, `jogSteps(n)`, `home()`, `update()`, `isHomed()`, `isMoving()`, `getCurrentStep()` | | `clients/esp32-motor/src/motor_interface.h` |
| TASK-021 | Create `clients/esp32-motor/src/stepper_motor.h/.cpp` — AccelStepper `HALF4WIRE` implementation for 28BYJ-48 + ULN2003 (4 pins per motor: IN1–IN4). 4096 steps/rev (half-step with 64:1 gear ratio). De-energise coils when idle to reduce heat. | | `clients/esp32-motor/src/stepper_motor.h`, `stepper_motor.cpp` |
| TASK-022 | Create `clients/esp32-motor/src/motor_manager.h/.cpp` — manages N motors, position mapping, homing coordination | | `clients/esp32-motor/src/motor_manager.h`, `motor_manager.cpp` |
| TASK-023 | Position mapping using calibrated range: `targetStep = startStep + (position / trackLength) * (endStep - startStep)`, respecting lane direction | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-024 | Homing state machine: IDLE → HOMING → HOMED → RUNNING (non-blocking) | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-025 | Limit switch pins (INPUT_PULLUP), one per lane — configurable in config.h | | `clients/esp32-motor/src/config.h` |
| TASK-026 | `begin(motorCount)` — returns false if motorCount = 0, making manager inactive | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-027 | On `positions` message: iterate player array → update target for each motor | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-028 | On game reset: trigger home sequence for all motors | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-029 | `jogMotor(laneId, steps)` — move motor by N steps relative (positive/negative), for calibration jog | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-029b | `moveMotorTo(laneId, absoluteStep)` — move motor to absolute step position | | `clients/esp32-motor/src/motor_manager.cpp` |
| TASK-029c | `getMotorStatus(laneId)` — returns `{ currentStep, targetStep, isMoving, isHomed, calibrated }` | | `clients/esp32-motor/src/motor_manager.cpp` |

### Phase 3b: Motor Calibration & Setup Routine

GOAL-003b: Per-lane calibration of start/end positions, persist config, REST API for remote calibration from admin UI.

| ID | Task | Status | File(s) |
|----|------|--------|--------|
| TASK-100 | Create `clients/esp32-motor/src/motor_calibration.h/.cpp` — calibration state machine and per-lane config | | `clients/esp32-motor/src/motor_calibration.h`, `motor_calibration.cpp` |
| TASK-101 | `LaneCalibration` struct: `{ laneId, startStep, endStep, totalTrackSteps, stepsPerMm, direction (CW/CCW), maxSpeed, acceleration, calibrated (bool) }` | | `clients/esp32-motor/src/motor_calibration.h` |
| TASK-102 | `beginCalibration(laneId)` — enter calibration mode for a lane: disable game position updates for that motor, allow manual jog | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-103 | `setStartPosition(laneId)` — capture current motor step as start position for lane | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-104 | `setEndPosition(laneId)` — capture current motor step as end position; calculate `totalTrackSteps = abs(endStep - startStep)` | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-105 | `finishCalibration(laneId)` — validate start ≠ end, persist to LittleFS, re-enable game updates | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-106 | `loadCalibration()` — load all lane calibrations from LittleFS on boot; `isCalibrated(laneId)` check | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-107 | `setLaneDirection(laneId, CW\|CCW)` — configure motor direction for string-pull mechanism | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-108 | `setLaneSpeed(laneId, maxSpeed, acceleration)` — per-lane speed/accel tuning | | `clients/esp32-motor/src/motor_calibration.cpp` |
| TASK-109 | REST API `GET /api/motor/status` — returns status for all motors: `[{ lane, currentStep, targetStep, isMoving, isHomed, calibration: { startStep, endStep, totalTrackSteps, direction, maxSpeed, calibrated } }]` | | `clients/esp32-motor/src/main.cpp` |
| TASK-110 | REST API `POST /api/motor/jog` — body `{ lane, steps, speed? }` — jog motor by N steps (positive = fwd, negative = rev) | | `clients/esp32-motor/src/main.cpp` |
| TASK-111 | REST API `POST /api/motor/moveto` — body `{ lane, step, speed? }` — move motor to absolute step | | `clients/esp32-motor/src/main.cpp` |
| TASK-112 | REST API `POST /api/motor/home` — body `{ lane? }` — home one or all motors to start/limit switch | | `clients/esp32-motor/src/main.cpp` |
| TASK-113 | REST API `POST /api/motor/calibrate/start` — body `{ lane }` — begin calibration, save current position as start | | `clients/esp32-motor/src/main.cpp` |
| TASK-114 | REST API `POST /api/motor/calibrate/end` — body `{ lane }` — save current position as end, calculate track steps | | `clients/esp32-motor/src/main.cpp` |
| TASK-115 | REST API `POST /api/motor/calibrate/finish` — body `{ lane }` — finalize and persist calibration | | `clients/esp32-motor/src/main.cpp` |
| TASK-116 | REST API `PUT /api/motor/config` — body `{ lane, direction?, maxSpeed?, acceleration?, stepsPerMm? }` — update lane config | | `clients/esp32-motor/src/main.cpp` |
| TASK-117 | REST API `POST /api/motor/calibrate/reset` — body `{ lane? }` — clear calibration for one or all lanes | | `clients/esp32-motor/src/main.cpp` |
| TASK-118 | Include calibration status in WS registration `capabilities`: `{ motors: 4, motorCalibrated: [true, true, false, false] }` | | `clients/esp32-motor/src/websocket.cpp` |

### Phase 4: Color Synchronisation (Server-Side)

GOAL-004: Motor client reports physical lane colors; server aligns player colorIndex to match.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-030 | Extend `ConnectionManager._handleRegister()` to extract `motorCount` and `motorColors` from motor client registration | | `server/src/ws/ConnectionManager.js` |
| TASK-031 | When motor client registers with `motorColors`: assign each player's `colorIndex` to match the physical lane's color index | | `server/src/ws/ConnectionManager.js`, `server/src/game/GameState.js` |
| TASK-032 | Add `GameState.setPlayerColorIndex(playerId, colorIndex)` method for server-side color override | | `server/src/game/GameState.js` |
| TASK-033 | Store motor lane → color mapping in `led-config.json` under `motorLaneColors` for persistence across reconnects | | `server/src/config/LedConfigManager.js`, `server/data/led-config.json` |
| TASK-034 | On subsequent player joins: auto-assign to matching motor lane color if available; otherwise use normal color pool | | `server/src/ws/ConnectionManager.js` |
| TASK-035 | Broadcast updated `state` after color reassignment so all clients (display, web) reflect physical colors | | `server/src/ws/ConnectionManager.js` |

### Phase 5: Control Buttons

GOAL-005: Physical buttons for game start/reset/pause, with WS message and debounce.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-040 | Create `clients/esp32-motor/src/buttons.h/.cpp` — button manager with debounce (200 ms), configurable pin mapping | | `clients/esp32-motor/src/buttons.h`, `buttons.cpp` |
| TASK-041 | Support 1–2 buttons: scan in `loop()`, fire callback on press | | `clients/esp32-motor/src/buttons.cpp` |
| TASK-042 | Default mapping: Button 1 → `start` (if idle) / `reset` (if finished), Button 2 → `pause` (if running) / `resume` (if paused) | | `clients/esp32-motor/src/buttons.cpp` |
| TASK-043 | Send `{ type: "button", payload: { button: N, action: "start"\|"reset"\|"pause"\|"resume" } }` over WebSocket | | `clients/esp32-motor/src/websocket.cpp` |
| TASK-044 | Server: handle `button` message type in `ConnectionManager._handleMessage()` — validate sender is motor type, delegate to game actions | | `server/src/ws/ConnectionManager.js` |
| TASK-045 | Server: map button actions to existing game methods: `startWithCountdown()`, `gameState.reset()`, `gameState.pause()` | | `server/src/ws/ConnectionManager.js` |
| TASK-046 | `begin(buttonCount)` — returns false if buttonCount = 0, making manager inactive | | `clients/esp32-motor/src/buttons.cpp` |

### Phase 6: LED Matrix

GOAL-006: WS2812B matrix for countdown numbers, event text, winner animation.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-050 | Create `clients/esp32-motor/src/matrix_display.h/.cpp` — LED matrix wrapper using shared `LedController` with matrix topology | | `clients/esp32-motor/src/matrix_display.h`, `matrix_display.cpp` |
| TASK-051 | Font data: 5×3 pixel font for digits 0-9 and A-Z (stored as bitmasks in a header) | | `clients/esp32-motor/src/font5x3.h` |
| TASK-052 | `showNumber(n)` — display single digit/number centered on matrix | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-053 | `showText(str, scrollSpeed)` — scrolling text for player names (non-blocking) | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-054 | `showCountdown(n)` — large digit with color (white for 3/2/1, green for GO!) | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-055 | `showWinner(name)` — celebratory animation + scrolling name in gold | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-056 | Idle effect: device-color breathe or rainbow using shared `AnimationManager` | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-057 | `begin(ledCount, pin)` — returns false if ledCount = 0, making display inactive | | `clients/esp32-motor/src/matrix_display.cpp` |
| TASK-058 | Integrate `GameEventMapper` from shared library for scored/game events → LED effects | | `clients/esp32-motor/src/matrix_display.cpp` |

### Phase 7: Bluetooth Audio & Speaker Management

GOAL-007: BT A2DP connection to speaker, fetch sound files from server, play on game events.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-060 | Create `clients/esp32-motor/src/bt_audio.h/.cpp` — Bluetooth A2DP Source manager using ESP-IDF `esp_a2d_api.h` / `esp_avrc_api.h` via Arduino wrapper | | `clients/esp32-motor/src/bt_audio.h`, `bt_audio.cpp` |
| TASK-061 | BT initialisation: `btStart()`, configure A2DP Source profile, register callbacks for connection/disconnection/data request | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-062 | Implement BT scan: discover nearby A2DP Sink devices, return list `[{ name, address, rssi }]` | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-063 | Implement BT pair + connect: pair with selected device by address, persist bonding in NVS | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-064 | Implement BT auto-connect: on boot, attempt connection to last paired device (with timeout + retry) | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-065 | Implement BT unpair / forget: remove bonding from NVS, disconnect if active | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-066 | Implement BT status reporting: `isConnected()`, `getPairedDeviceName()`, `getConnectionState()` | | `clients/esp32-motor/src/bt_audio.cpp` |
| TASK-067 | Create `clients/esp32-motor/src/sound.h/.cpp` — sound manager, fetches WAV files from server via HTTP, decodes to PCM, feeds A2DP data callback | | `clients/esp32-motor/src/sound.h`, `sound.cpp` |
| TASK-068 | Sound file fetching: HTTP GET from `http://<server>/assets/sounds/<event>.wav`, cache in PSRAM (or SPIFFS if no PSRAM) | | `clients/esp32-motor/src/sound.cpp` |
| TASK-069 | Sound event mapping: countdown tick, countdown GO, score +1/+2/+3, score 0, winner fanfare, button click — each mapped to a WAV filename | | `clients/esp32-motor/src/sound.cpp` |
| TASK-069b | Non-blocking playback: ring buffer fed to A2DP data callback at 44.1 kHz / 16-bit stereo | | `clients/esp32-motor/src/sound.cpp` |
| TASK-069c | Volume control via AVRCP absolute volume or PCM amplitude scaling | | `clients/esp32-motor/src/sound.cpp` |
| TASK-069d | `begin()` — returns false if BT init fails, making sound inactive; `isAvailable()` checks BT speaker connected | | `clients/esp32-motor/src/sound.cpp` |
| TASK-069e | Fallback: if `BT_FALLBACK_PIN` defined in config.h, play tones via `ledcWriteTone()` on PWM buzzer when no BT speaker connected | | `clients/esp32-motor/src/sound.cpp` |

### Phase 7b: Bluetooth REST API & Admin UI

GOAL-007b: HTTP endpoints on ESP32 for BT speaker management, admin UI integration.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-069f | `GET /api/bt/status` — returns `{ paired, connected, deviceName, deviceAddress }` | | `clients/esp32-motor/src/main.cpp` |
| TASK-069g | `POST /api/bt/scan` — triggers BT scan, returns `{ devices: [{ name, address, rssi }] }` (blocking, ~10 s timeout) | | `clients/esp32-motor/src/main.cpp` |
| TASK-069h | `POST /api/bt/pair` — body `{ address }`, pair + connect to device | | `clients/esp32-motor/src/main.cpp` |
| TASK-069i | `POST /api/bt/unpair` — disconnect and forget paired device | | `clients/esp32-motor/src/main.cpp` |
| TASK-069j | Include BT status in WS registration `capabilities` so admin UI shows speaker info | | `clients/esp32-motor/src/websocket.cpp` |

### Phase 7c: Server Admin UI — Motor Calibration & BT Management

GOAL-007c: Admin devices page hosts calibration controls and BT management for connected motor clients. Server proxies REST calls to ESP32 HTTP endpoints.

| ID | Task | Status | File(s) |
|----|------|--------|--------|
| TASK-120 | Add motor calibration panel to `devices.html` — per-motor-client expandable section with lane list, shows only when motor client connected | | `clients/web/devices.html` |
| TASK-121 | Jog controls per lane: ◀ / ▶ buttons (configurable step size: 1, 10, 100, 1000 steps), speed slider | | `clients/web/js/devices.js` |
| TASK-122 | "Set Start" / "Set End" / "Finish Calibration" buttons per lane — calls ESP32 REST API through server proxy | | `clients/web/js/devices.js` |
| TASK-123 | Move-to-step input + Go button per lane | | `clients/web/js/devices.js` |
| TASK-124 | Home button per lane + Home All button | | `clients/web/js/devices.js` |
| TASK-125 | Lane config editor: direction (CW/CCW dropdown), max speed, acceleration, steps-per-mm | | `clients/web/js/devices.js` |
| TASK-126 | Live motor status display: current step, target step, moving indicator, calibration status per lane (✓/✗) — polls `GET /api/motor/status` every 2 s via proxy | | `clients/web/js/devices.js` |
| TASK-127 | BT speaker management panel: scan button → device list with pair/unpair, connection status indicator, auto-connect toggle | | `clients/web/js/devices.js` |
| TASK-128 | Server proxy route: `POST /api/devices/:clientId/motor/*` — resolves motor client IP from client record, forwards HTTP request to ESP32 REST API, returns response | | `server/src/routes/clients.js` |
| TASK-129 | Server proxy route: `POST /api/devices/:clientId/bt/*` — forwards BT REST calls (scan/pair/unpair/status) to motor client | | `server/src/routes/clients.js` |
| TASK-130 | Poll motor status on interval (2 s) when calibration panel is open — update live position display | | `clients/web/js/devices.js` |
| TASK-131 | Calibration wizard mode: step-by-step guided flow (1. select lane → 2. jog to start → 3. save start → 4. jog to end → 5. save end → 6. verify → 7. finish) | | `clients/web/js/devices.js` |

### Phase 8: Status LED & Integration

GOAL-008: Status LED, subsystem orchestration in main loop, serial diagnostics.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-070 | Create `clients/esp32-motor/src/led.h/.cpp` — status LED state machine (fast blink / slow blink / solid) using millis() | | `clients/esp32-motor/src/led.h`, `led.cpp` |
| TASK-071 | Main `loop()` orchestration: `wsClient.loop()` → `motorManager.loop()` → `matrixDisplay.loop()` → `soundManager.loop()` → `btAudio.loop()` → `buttons.loop()` → `statusLed.loop()` → `flushStateIfNeeded()` | | `clients/esp32-motor/src/main.cpp` |
| TASK-072 | Serial debug output: WiFi status, WS messages received, motor positions, subsystem availability | | `clients/esp32-motor/src/main.cpp` |
| TASK-073 | HTTP config endpoint (port 80): `POST /config` for OTA parameter updates (mirror sensor pattern) | | `clients/esp32-motor/src/main.cpp` |
| TASK-074 | mDNS self-registration: `derby-motor-XXXX.local` | | `clients/esp32-motor/src/main.cpp` |

### Phase 9: Server-Side Support

GOAL-009: Server handles motor capabilities, button messages, and color sync.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-080 | Store `motorCount`, `motorColors`, `capabilities` on client record in ConnectionManager | | `server/src/ws/ConnectionManager.js` |
| TASK-081 | Handle `button` WS message: validate motor client, map action to `startWithCountdown()` / `gameState.pause()` / `gameState.reset()`, broadcast state | | `server/src/ws/ConnectionManager.js` |
| TASK-082 | Include motor capabilities (incl. BT speaker status, calibration status per lane) in `_getDeviceList()` response for admin UI | | `server/src/ws/ConnectionManager.js` |
| TASK-083 | Expose motor lane colors in `GET /api/game` response when motor client is connected | | `server/src/routes/game.js` |
| TASK-084 | Tests for button message handling (valid actions, invalid sender type, debounce) | | `server/tests/connectionManager.test.js` |
| TASK-085 | Tests for motor color sync (registration with motorColors, player colorIndex reassignment) | | `server/tests/connectionManager.test.js` |
| TASK-086 | Create `server/public/assets/sounds/` directory with WAV sound files: `countdown-tick.wav`, `countdown-go.wav`, `score-1.wav`, `score-2.wav`, `score-3.wav`, `score-0.wav`, `winner.wav`, `button-click.wav` | | `server/public/assets/sounds/` |
| TASK-087 | Serve sound files via existing `/assets` static mount (already maps to `../clients/assets/`) — add `sounds/` directory to `clients/assets/` | | `clients/assets/sounds/` |
| TASK-088 | Add server proxy routes for motor calibration and BT management — forward admin UI requests to motor client HTTP endpoints | | `server/src/routes/clients.js` |

### Phase 10: Documentation & Web Install

GOAL-010: README, web-based firmware installer, docs updates.

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-090 | Create `clients/esp32-motor/README.md` — setup, wiring, configuration, subsystem overview | | `clients/esp32-motor/README.md` |
| TASK-091 | Create `clients/esp32-motor/web-install/` — ESP Web Tools manifest for browser-based flashing | | `clients/esp32-motor/web-install/` |
| TASK-092 | Update `server/README.md` — document `button` message type, `motorColors` in registration, capabilities, sound file hosting | | `server/README.md` |
| TASK-093 | Update `docs/progress.md` with motor controller milestone | | `docs/progress.md` |
| TASK-094 | Update `.github/copilot-instructions.md` — add ESP32 motor client to component table | | `.github/copilot-instructions.md` |
| TASK-095 | Document BT speaker setup and pairing workflow in ESP32 motor README | | `clients/esp32-motor/README.md` |
| TASK-096 | Document motor calibration workflow (step-by-step guide with screenshots) in ESP32 motor README | | `clients/esp32-motor/README.md` |
| TASK-097 | Update `server/README.md` — document motor proxy API and admin motor calibration UI | | `server/README.md` |

## 3. Alternatives

| ID | Option | Pros | Cons | Decision |
|----|--------|------|------|----------|
| ALT-001 | ESP8266 (original plan v1) | Cheaper, existing sensor code as reference | Limited GPIOs for motors+matrix+sound+buttons, single core, no I2S, 300 LED max | **Rejected** — ESP32 selected for GPIO count, dual core, I2S, 1000 LED support |
| ALT-002 | ESP32-S3 instead of ESP32 | USB-OTG, more GPIOs, PSRAM by default | Overkill, higher cost, less common boards | **Deferred** — standard ESP32 sufficient; S3 as future option |
| ALT-003 | I2S DAC (MAX98357A) wired speaker | No BT latency, reliable connection | Extra hardware, wired, no speaker flexibility | **Rejected** — BT speaker is wireless and user-friendly; I2S as optional local fallback |
| ALT-004 | PWM buzzer via `ledcWriteTone()` only | Simplest, no BT complexity | Terrible audio quality, no WAV playback | **Fallback only** — used when no BT speaker paired |
| ALT-008 | ESP32 BLE Audio (LE Audio / LC3) | Lower power, newer standard | Very few speakers support it (2024+), ESP-IDF support immature | **Rejected** — BT Classic A2DP has universal speaker support |
| ALT-009 | Sound files embedded in firmware (SPIFFS) | Offline, no server dependency | Large firmware, hard to update sounds, limited flash | **Rejected** — server-hosted WAV files allow easy updates; ESP32 caches after first fetch |
| ALT-010 | ESP-StepperMotor-Server library for motor control | Full stepper server with web UI, REST API, CLI built-in | Heavy dependency, own web server, not designed for integration into existing WS-based system | **Reference only** — borrow REST API patterns (jog, moveto, home, config) but implement lightweight custom solution |
| ALT-011 | Server-side calibration storage (not ESP32 LittleFS) | Centralized config, survives ESP32 replacement | Extra round-trip, ESP32 can't operate standalone without server | **Rejected** — calibration stored on ESP32 for offline resilience; server gets calibration status via registration |
| ALT-012 | Rotary encoder for manual motor jog (hardware) | Tactile, no WiFi dependency during calibration | Extra hardware, GPIO pins, complexity | **Deferred** — web UI jog sufficient for MVP; encoder as Phase 2 enhancement |
| ALT-013 | Direct browser → ESP32 REST (no server proxy) | Lower latency, simpler server | CORS config, user must know ESP32 IP, breaks admin UI isolation | **Alternative** — server proxy is default; direct access as documented fallback |
| ALT-005 | Server-side color assignment only (no motorColors in registration) | Simpler protocol | Digital colors don't match physical hardware | **Rejected** — physical-digital color sync is a core requirement |
| ALT-006 | Separate ESP32 per subsystem (one for motors, one for matrix) | Simpler per-device firmware | More hardware, latency between devices, more WS connections | **Rejected** — single ESP32 handles all subsystems |
| ALT-014 | NEMA 17 + A4988 driver instead of 28BYJ-48 + ULN2003 | Higher torque, faster, only 2 GPIO pins per motor (STEP + DIR) | Larger, 12V supply needed, louder, more expensive | **Upgrade path** — same `MotorInterface`; swap `stepper_motor.cpp` to `DRIVER` mode if 28BYJ-48 torque/speed insufficient |
| ALT-007 | MAX7219 LED matrix instead of WS2812B | Cheaper for 8×8, built-in driver IC | Monochrome only, no color effects, different library | **Rejected** — WS2812B provides color continuity with LED strip effects and existing shared library |

## 4. Dependencies

| ID | Dependency | Required By |
|----|-----------|-------------|
| DEP-001 | `gilmaimon/ArduinoWebsockets` ^0.5.4 — WebSocket client (RFC-6455) | TASK-010 |
| DEP-002 | `tzapu/WiFiManager` ^2.0.17 — WiFi config with captive portal (ESP32 support) | TASK-003 |
| DEP-003 | `bblanchon/ArduinoJson` ^7.x — JSON parse/serialize | TASK-010 |
| DEP-004 | `waspinator/AccelStepper` ^1.64 — non-blocking stepper control (`HALF4WIRE` for 28BYJ-48) | TASK-021 |
| DEP-005 | `makuna/NeoPixelBus` ^2.7.0 — WS2812B driver (RMT method on ESP32) | TASK-050 |
| DEP-006 | Shared LED library (`clients/shared/leds/`) | TASK-050, TASK-058 |
| DEP-007 | PlatformIO Core CLI or VSCode extension | TASK-001 |
| DEP-008 | ESP32 Arduino framework (platform: `espressif32`) | TASK-001 |
| DEP-009 | Game server (WebSocket + REST) — existing | TASK-010, TASK-044 |
| DEP-010 | `player-colors.json` palette — shared 16-color palette | TASK-030 |
| DEP-011 | `ESPmDNS.h` (ESP32 built-in) — mDNS discovery | TASK-003 |
| DEP-012 | ESP32 Bluetooth Classic (built-in) — A2DP Source profile for audio streaming | TASK-060 |
| DEP-013 | ESP-IDF A2DP/AVRCP APIs (`esp_a2d_api.h`, `esp_avrc_api.h`) via Arduino ESP32 core | TASK-060 |
| DEP-014 | Server-hosted WAV sound files at `/assets/sounds/` | TASK-068 |
| DEP-015 | ESP32 LEDC (built-in) — PWM fallback buzzer tone generation | TASK-069e |
| DEP-016 | ESP32 WebServer (`WebServer.h`, built-in) — HTTP REST API for calibration and BT management | TASK-109–117, TASK-069f–069i |

## 5. Files

### New Files (ESP32 firmware)

| ID | File | Action |
|----|------|--------|
| FILE-001 | `clients/esp32-motor/platformio.ini` | Create — PlatformIO config |
| FILE-002 | `clients/esp32-motor/src/main.cpp` | Create — setup/loop, WiFi, mDNS, subsystem orchestration |
| FILE-003 | `clients/esp32-motor/src/config.h` | Create — pins, defaults, config structs |
| FILE-004 | `clients/esp32-motor/src/websocket.h` / `.cpp` | Create — WS client, register, message dispatch |
| FILE-005 | `clients/esp32-motor/src/motor_interface.h` | Create — abstract motor base class |
| FILE-006 | `clients/esp32-motor/src/stepper_motor.h` / `.cpp` | Create — AccelStepper HALF4WIRE implementation for 28BYJ-48 + ULN2003 |
| FILE-007 | `clients/esp32-motor/src/motor_manager.h` / `.cpp` | Create — multi-motor coordination, homing, position mapping, jog, move-to |
| FILE-007b | `clients/esp32-motor/src/motor_calibration.h` / `.cpp` | Create — per-lane calibration state machine, start/end capture, LittleFS persistence |
| FILE-008 | `clients/esp32-motor/src/matrix_display.h` / `.cpp` | Create — LED matrix display (countdown, text, effects) |
| FILE-009 | `clients/esp32-motor/src/font5x3.h` | Create — bitmap font for matrix digits/letters |
| FILE-010 | `clients/esp32-motor/src/buttons.h` / `.cpp` | Create — button manager with debounce |
| FILE-011 | `clients/esp32-motor/src/bt_audio.h` / `.cpp` | Create — Bluetooth A2DP Source, scan/pair/connect/unpair |
| FILE-011b | `clients/esp32-motor/src/sound.h` / `.cpp` | Create — sound manager, HTTP WAV fetch, cache, A2DP playback, PWM fallback |
| FILE-012 | `clients/esp32-motor/src/led.h` / `.cpp` | Create — status LED state machine |
| FILE-013 | `clients/esp32-motor/README.md` | Create — documentation |
| FILE-014 | `clients/esp32-motor/web-install/` | Create — ESP Web Tools installer |

### Modified Files (server-side)

| ID | File | Action |
|----|------|--------|
| FILE-020 | `server/src/ws/ConnectionManager.js` | Modify — handle `button` messages, extract `motorColors`/`capabilities`/`motorCalibrated`, color sync |
| FILE-020b | `server/src/routes/clients.js` | Modify — add motor-proxy and bt-proxy routes for admin UI to reach ESP32 REST endpoints |
| FILE-021 | `server/src/game/GameState.js` | Modify — add `setPlayerColorIndex()` for motor color sync |
| FILE-022 | `server/src/config/LedConfigManager.js` | Modify — persist `motorLaneColors` mapping |
| FILE-023 | `server/data/led-config.json` | Modify — add `motorLaneColors` section |
| FILE-024 | `server/src/routes/game.js` | Modify — expose motor capabilities in game state |
| FILE-025 | `server/README.md` | Modify — document button message, motorColors, capabilities, sound file hosting |
| FILE-026 | `server/tests/connectionManager.test.js` | Modify — button and color sync tests |
| FILE-027 | `clients/assets/sounds/` | Create — WAV sound files for game events (countdown, scores, winner, button) |

### Modified Files (docs)

| ID | File | Action |
|----|------|--------|
| FILE-028 | `clients/web/devices.html` | Modify — add motor calibration panel + BT management panel sections |
| FILE-029 | `clients/web/js/devices.js` | Modify — add calibration UI logic (jog, set start/end, config editor, wizard) + BT management UI |
| FILE-030 | `.github/copilot-instructions.md` | Modify — add ESP32 motor client to component table |
| FILE-031 | `docs/progress.md` | Modify — motor controller milestone |

## 6. Testing

### Firmware (manual / integration)

| ID | Test | Validates |
|----|------|-----------|
| TEST-001 | WiFiManager: first boot → AP mode → configure → connects to WiFi | REQ-001 |
| TEST-002 | mDNS discovery: server running → ESP32 finds `_derby._tcp` → connects | REQ-002 |
| TEST-003 | WS register: sends `motorCount`, `motorColors`, `capabilities` → receives `registered` | REQ-003, REQ-014, REQ-064 |
| TEST-004 | WS reconnect: kill server → backoff retries → restart server → reconnects | REQ-005 |
| TEST-005 | Motor movement: send positions → motors move to correct proportional position within calibrated range | REQ-010, REQ-088 |
| TEST-006 | Smooth motion: AccelStepper accel/decel visible | REQ-011 |
| TEST-007 | Homing: boot → motors find limit switches → start position | REQ-012 |
| TEST-008 | Game reset: all positions = 0 → motors return home | REQ-013 |
| TEST-030 | Calibration jog: `POST /api/motor/jog { lane: 0, steps: 100 }` → motor moves 100 steps forward | REQ-082 |
| TEST-031 | Calibration flow: begin → jog to start → set start → jog to end → set end → finish → `totalTrackSteps` = abs(end - start) | REQ-081 |
| TEST-032 | Calibration persistence: calibrate → reboot ESP32 → calibration data loaded from LittleFS | REQ-085 |
| TEST-033 | Move-to-step: `POST /api/motor/moveto { lane: 0, step: 500 }` → motor at step 500 | REQ-083 |
| TEST-034 | Home motor: `POST /api/motor/home { lane: 0 }` → motor returns to start position | REQ-084 |
| TEST-035 | Position mapping: game position 5/10 → motor at midpoint between startStep and endStep | REQ-088 |
| TEST-036 | Direction config: set lane to CCW → motor reverses for positive game positions | REQ-089 |
| TEST-037 | Calibration reset: clear calibration → lane shows uncalibrated, falls back to default range | REQ-086 |
| TEST-038 | Admin UI proxy: admin sends jog via server `POST /api/devices/:id/motor/jog` → forwarded to ESP32 → motor moves | REQ-087 |
| TEST-039 | E-stop: uncalibrated motor cannot exceed safe default step range (prevents string snap) | REQ-080 |
| TEST-009 | Button start: press Button 1 in idle → server starts countdown | REQ-031, REQ-032 |
| TEST-010 | Button pause: press Button 2 in running → server pauses | REQ-031, REQ-032 |
| TEST-011 | LED matrix countdown: countdown message → numbers 3, 2, 1, GO! displayed | REQ-041 |
| TEST-012 | LED matrix took_lead: player takes lead → name scrolls on matrix | REQ-042 |
| TEST-013 | LED matrix winner: winner announced → name + animation on matrix | REQ-043 |
| TEST-014 | BT scan: trigger scan → discovers nearby BT speakers | REQ-071 |
| TEST-015 | BT pair + auto-connect: pair speaker → reboot → auto-connects on boot | REQ-072, REQ-073 |
| TEST-015b | BT unpair: unpair → no auto-connect on next boot | REQ-074 |
| TEST-015c | Sound fetch: ESP32 fetches WAV from server `/assets/sounds/countdown-tick.wav` → caches | REQ-051 |
| TEST-015d | Sound countdown: tick beeps on countdown via BT speaker, longer on GO! | REQ-052 |
| TEST-015e | Sound winner: fanfare plays on BT speaker on winner message | REQ-054 |
| TEST-015f | Sound fallback: no BT speaker → PWM buzzer plays tones (if fallback pin configured) | REQ-057 |
| TEST-015g | BT REST API: `GET /api/bt/status` returns connection info | REQ-077 |
| TEST-016 | Graceful degradation: motorCount=0 in config → motors skipped, LED+buttons+sound work | REQ-061 |
| TEST-017 | Status LED states: no WiFi = fast blink, WiFi = slow blink, WS = solid | REQ-006 |

### Server (automated)

| ID | Test | Validates |
|----|------|-----------|
| TEST-020 | Motor client registers with `motorColors` → players get matching colorIndex | TASK-031, TASK-035 |
| TEST-021 | `button` message from motor client → game state changes correctly | TASK-044, TASK-045 |
| TEST-022 | `button` message from non-motor client → rejected | TASK-044 |
| TEST-023 | Motor client reconnect → color assignments restored from persisted mapping | TASK-033 |
| TEST-024 | `_getDeviceList()` includes motor capabilities (incl. calibration status) | TASK-082 |
| TEST-025 | All existing server tests pass (no regressions) | — |

## 7. Risks & Assumptions

| ID | Risk | Mitigation |
|----|------|------------|
| RISK-001 | AccelStepper `run()` frequency impacted by WS polling + LED updates | ESP32 dual core: run motors on core 1, WS/LED/sound on core 0 if needed |
| RISK-002 | 28BYJ-48 has low torque (~3.4 mN·m) and slow max speed (~15 RPM) — may limit string pull speed | Acceptable for game speeds; calibrate maxSpeed per lane; upgrade to NEMA 17 + A4988 if needed (same `MotorInterface`) |
| RISK-003 | 28BYJ-48 uses 4 GPIOs per motor — 8 motors = 32 GPIOs, leaving few for other subsystems | Practical limit ~4-6 motors with LED matrix + buttons + BT; use I²C GPIO expander (PCF8574/MCP23017) if >6 motors needed; document pin map in README |
| RISK-004 | WS2812B matrix timing conflicts with WiFi | NeoPixelBus RMT method on ESP32 is hardware-accelerated and WiFi-safe |
| RISK-005 | BT A2DP latency (50–200 ms) may desync audio with LED matrix visuals | Accept for MVP — A2DP latency is inherent; visual cues are primary feedback, audio is supplementary |
| RISK-009 | BT + WiFi coexistence reduces WiFi throughput | ESP32 supports coex natively; game WS traffic is minimal (<1 KB/s); test under load |
| RISK-010 | BT pairing UX complexity — users unfamiliar with process | Provide clear admin UI panel + README walkthrough; auto-connect reduces friction after first pair |
| RISK-011 | PSRAM not available on all ESP32 boards for WAV caching | Fall back to SPIFFS cache (slower) or re-fetch on demand; document PSRAM-equipped board recommendation |
| RISK-012 | Uncalibrated motors overshoot physical track limits (string snaps or jams) | Firmware enforces step limits within calibrated range; warn in admin UI if uncalibrated; require calibration before first game |
| RISK-013 | Calibration data lost on ESP32 flash failure | LittleFS atomic writes (mirror sensor pattern); admin UI shows calibration status per lane; re-calibration takes ~2 min |
| RISK-014 | Admin UI ↔ ESP32 REST latency via server proxy adds lag to jog commands | Same LAN (<5 ms roundtrip); acceptable for calibration; direct ESP32 IP access as documented fallback |
| RISK-015 | String-pull mechanism has backlash / slack affecting position accuracy | Post-calibration verification step (move to known positions, measure); document mechanical tuning tips in README |
| RISK-016 | ESP32 WebServer conflicts with WebSocket client on same core | Run WebServer on core 0, motor control on core 1; or use async WebServer (ESPAsyncWebServer) |
| RISK-006 | Motor colors hardcoded on physical hardware differ from 16-color palette | Require motor colors to use palette indices (0-15); physical paint/stickers match palette |
| RISK-007 | Button debounce insufficient for physical buttons | 200 ms debounce + server-side rate limiting on game actions |
| RISK-008 | LittleFS write wear from frequent state saves | Debounce saves to max 1 write per 2 seconds (mirrors sensor pattern) |

| ID | Assumption |
|----|-----------|
| ASSUMPTION-001 | ESP32 DevKit v1 (or equivalent) with 38-pin breakout as target board |
| ASSUMPTION-002 | **28BYJ-48** 5V stepper motor + **ULN2003** driver board — 4096 steps/rev (half-step, 64:1 gear ratio), 4 GPIO pins per motor (IN1–IN4), AccelStepper `HALF4WIRE` mode |
| ASSUMPTION-011 | String/belt pull mechanism: motor rotates a spool or pulley that pulls a string linearly between start and end positions |
| ASSUMPTION-012 | Each lane requires independent calibration — track lengths and string tensions may differ between lanes |
| ASSUMPTION-013 | Calibration is performed once during physical setup, rarely repeated; persisted across reboots |
| ASSUMPTION-003 | Server and ESP32 on same WiFi network with <50 ms latency |
| ASSUMPTION-004 | PlatformIO installed on dev machine |
| ASSUMPTION-005 | One ESP32 controller handles all subsystems (motors + matrix + buttons + sound) |
| ASSUMPTION-006 | WS2812B 8×8 matrix (64 LEDs) as default; configurable via server |
| ASSUMPTION-007 | Any standard Bluetooth speaker/headphone (A2DP Sink) will work as sound output |
| ASSUMPTION-008 | WAV files are 44.1 kHz, 16-bit, stereo (standard CD quality for A2DP SBC codec) |
| ASSUMPTION-009 | Sound files total <2 MB — fits in PSRAM or SPIFFS cache |
| ASSUMPTION-010 | ESP32 board has BT Classic support (not ESP32-C3/S2 which lack BT Classic) |

## 8. Related Specifications / Further Reading

- [PRD: Roll-a-Ball Derby](../docs/PRD.md)
- [Superseded plan v1 (ESP8266)](feature-client-esp8266-motor-1.md)
- [Sensor Client Plan](feature-client-esp8266-sensor-1.md) — reference firmware patterns
- [Device Colors Plan](feature-device-colors-1.md) — server color assignment engine
- [LED Control Phase 1 Plan](feature-led-control-phase1-1.md)
- [Shared LED Library](../clients/shared/leds/README.md) — LedController, AnimationManager, GameEventMapper
- [Server README](../server/README.md) — WebSocket protocol, REST API
- [Player Colors](../clients/assets/themes/shared/player-colors.json) — 16-color shared palette
- [gilmaimon/ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets)
- [WiFiManager (ESP32)](https://github.com/tzapu/WiFiManager)
- [AccelStepper](https://www.airspayce.com/mikem/arduino/AccelStepper/) — `HALF4WIRE` mode for 28BYJ-48
- [28BYJ-48 + ULN2003 with AccelStepper](https://lastminuteengineers.com/28byj48-stepper-motor-arduino-tutorial/) — wiring, pin sequence, gear ratio details
- [NeoPixelBus](https://github.com/Makuna/NeoPixelBus)
- [ESP32 A2DP Source API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_a2d.html)
- [ESP32 Bluetooth Classic](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)
- [ESP32-A2DP Arduino Library](https://github.com/pschatzmann/ESP32-A2DP) — high-level A2DP wrapper (reference)
- [ESP32 LEDC PWM](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html) — fallback buzzer
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/)
- [ESP-StepperMotor-Server](https://github.com/pkerspe/ESP-StepperMotor-Server) — reference for stepper REST API patterns (jog, moveto, home, config persistence)
- [Belt/Pulley Calibration Tutorial](https://www.norwegiancreations.com/2015/07/tutorial-calibrating-stepper-motor-machines-with-belts-and-pulleys/) — steps/mm calculation for belt-driven linear motion
- [PlatformIO ESP32 DevKit](https://docs.platformio.org/en/stable/boards/espressif32/esp32dev.html)
