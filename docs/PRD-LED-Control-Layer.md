# PRD: Shared LED Control Layer for ESP8266 Clients

> **Version**: 1.0  
> **Date**: 2026-04-07  
> **Status**: Draft

---

## 1. Product overview

### 1.1 Document title and version

- PRD: Shared LED Control Layer for ESP8266 Clients
- Version: 1.0

### 1.2 Product summary

This feature introduces a shared, reusable LED control abstraction layer for all ESP8266 and ESP32 clients (sensor, motor, and future clients) in the Roll-a-Ball Derby system. The layer provides a unified API for controlling WS2812B addressable RGB LEDs in various physical configurations (strips, matrices, rings) and enables sophisticated visual feedback tied to game events, connection states, and sensor activity. The system supports server-driven configuration and synchronization, allowing centralized control of LED behavior across all physical devices.

The LED control layer replaces and extends the current simple status LED implementation with a powerful animation engine capable of displaying arcade-style effects, progress visualizations, celebration patterns, and real-time game state feedback. All LED configurations and animation definitions are managed centrally on the server and synchronized to devices via WebSocket, ensuring consistent visual experiences across the physical game setup.

The abstraction layer is designed to support mixed deployments with both ESP8266 and ESP32 hardware, automatically detecting chip capabilities and optimizing LED control methods accordingly (DMA/UART on ESP8266, RMT on ESP32).

---

## 2. Goals

### 2.1 Business goals

- Enhance spectator experience with synchronized visual effects across physical hardware
- Create memorable "arcade-style" atmosphere at events through coordinated lighting
- Enable rapid deployment of new lighting effects without firmware updates
- Differentiate Roll-a-Ball Derby from competitors through polished physical presentation
- Support scalable LED configurations from minimal (10 LEDs) to elaborate (300+ LEDs) setups

### 2.2 User goals

- Game hosts can configure LED behavior through web interface without technical knowledge
- Players receive immediate visual feedback for scoring events on their physical hardware
- Spectators see coordinated light shows that match on-screen animations
- Developers can test LED patterns and animations without physical hardware present
- Event operators can brand the experience with custom color schemes and patterns

### 2.3 Non-goals

- Support for non-WS2812B LED chipsets (SPI-based LEDs like APA102 are out of scope)
- Integration with third-party lighting control protocols (e.g., DMX, Art-Net)
- Real-time music synchronization or audio-reactive effects
- Per-LED color calibration or color temperature adjustment
- Battery-powered operation optimization (system assumes AC power)

---

## 3. User personas

### 3.1 Key user types

- Game Host (event operator)
- Player (participant)
- Spectator (audience)
- Developer/Tester (system integrator)
- Hardware Technician (installer)

### 3.2 Basic persona details

- **Game Host**: Non-technical event organizer who configures the game through web interface, wants "plug and play" LED effects that enhance the experience without requiring custom programming.

- **Player**: Participant focused on the game, expects clear visual feedback when scoring (LED flash on their lane), wants to know their device is working (connection status indicators).

- **Spectator**: Audience member who enjoys coordinated light shows during countdowns, winning celebrations, and race progression; enhanced entertainment value beyond the digital display.

- **Developer/Tester**: Technical user developing new features, needs ability to preview LED patterns in browser, test animations without physical hardware, and rapidly iterate on effect designs.

- **Hardware Technician**: Person setting up physical game installations, needs clear visual diagnostics for LED wiring issues, wants automatic LED count detection and configuration validation.

### 3.3 Role-based access

- **Game Host**: Can modify LED configurations (color schemes, animation presets) through web admin interface during game idle state.

- **Developer/Tester**: Can access LED simulator in web interface, upload custom animation definitions, and trigger test effects without starting a game.

- **Hardware Technician**: Can access diagnostic mode showing LED strand status, pixel test patterns, and connection verification tools.

---

## 4. Functional requirements

### 4.1 LED Hardware Support (Priority: Critical)

- Support WS2812B addressable RGB LEDs (800kHz protocol)
- Handle LED strips (linear arrangements, 30-144 LEDs/meter)
- Support LED matrices (8x8, 16x16 grids with zigzag or progressive wiring)
- Control LED rings (circular arrangements, common sizes 12, 16, 24, 40, 60 LEDs)
- Support mixed topologies (e.g., status strip + score ring on same device)
- Maximum 300 LEDs per ESP8266 client (medium scale)

### 4.2 Core Abstraction Layer (Priority: Critical)

- Provide unified C++ API for LED control across all ESP8266 and ESP32 client firmware
- Abstract physical topology from animation logic (effects work on any layout)
- Abstract hardware platform differences (automatic chip detection, method selection)
- Non-blocking operation allowing WiFi housekeeping (yield points every 50ms)
- Consistent color space (HSV and RGB support with brightness control)
- Memory-efficient buffering appropriate for ESP8266 constraints (256KB RAM), scalable for ESP32 (520KB RAM)
- Hardware-specific optimizations (NeoPixelBus DMA/UART for ESP8266, RMT for ESP32)

### 4.3 Animation Engine (Priority: High)

- Predefined effect library: solid color, blink, pulse, rainbow, chase, sparkle, fire, wave
- Support looping effects (arcade-style patterns that run continuously)
- Frame-based animation system (30-60 FPS target)
- Smooth transitions between effects (crossfade, instant switch, fade-to-black)
- Layered effects (e.g., status indicator overlay on ambient pattern)
- Parameterized effects (speed, color, direction, intensity configurable)

### 4.4 Event-Driven Feedback (Priority: Critical)

- Connection state visualization (no WiFi, WiFi only, WebSocket connected)
- Sensor trigger confirmation (instant flash when ball enters hole)
- Scoring event feedback (different patterns for +1, +2, +3 rolls)
- Countdown synchronization (ticking animation during game countdown)
- Win/loss celebration (player's LEDs show victory or defeat animation)
- Progress visualization on sensor client (LED bar showing position in race)
- Motor client position feedback (LEDs track physical figure movement)

### 4.5 Server Configuration & Sync (Priority: High)

- LED configuration stored on server, synchronized at client registration
- Configuration includes: topology type, LED count, pin assignment, color mappings
- Animation presets defined server-side, referenced by name in events
- Web admin interface for visual LED configuration (drag-drop designer)
- Real-time preview of LED patterns in web browser (WebGL simulation)
- Configuration validation (ensure LED counts match detected hardware)

### 4.6 Fallback & Diagnostics (Priority: Medium)

- Default built-in patterns if server configuration unavailable
- LED strand test mode (rainbow cycle through all pixels)
- Connection diagnostic patterns (visual Morse code for error states)
- Brownout detection and graceful degradation (reduce brightness if power sags)
- LED count auto-detection (send detected count to server for validation)
- Watchdog timer integration (animation loop must not block critical functions)

---

## 5. User experience

### 5.1 Entry points & first-time user flow

- Hardware technician connects WS2812B LED strip to ESP8266 data pin (GPIO4 default)
- Firmware automatically detects LED strand at boot, counts pixels
- Status LEDs show boot sequence: red pulse → yellow connecting → green connected
- Device registers with server, receives LED configuration matching detected topology
- LEDs switch to "idle" ambient pattern (slow rainbow cycle by default)
- Admin web interface shows device in LED configuration panel with detected hardware

### 5.2 Core experience

- **Game Idle**: Ambient lighting patterns on all devices (breathing rainbow, slow chase)
  - Creates inviting arcade atmosphere, signals system is ready

- **Game Countdown**: Synchronized ticking animation across all devices
  - Visual countdown matches audio/screen countdown, builds anticipation

- **Active Gameplay**: Real-time feedback synchronized with scoring events
  - Sensor LEDs flash instantly when ball triggers sensor (tactile feedback)
  - All devices show progress visualization (LED bars filling toward finish line)
  - Rank changes trigger brief color shift (took lead = green pulse, lost lead = red)

- **Scoring Events**: Differentiated visual feedback per score type
  - +1 roll: single quick flash, blue color
  - +2 roll: double flash, purple color  
  - +3 roll: triple flash with sparkle trail, gold color
  - Zero roll: brief red fade on player's lane LEDs

- **Game Winner**: Coordinated celebration across entire system
  - Winner's sensor client: rainbow explosion, sparkling cascade
  - Winner's lane on all displays: golden pulse, chasing lights
  - Other players: brief "nice try" pattern, fade to ambient
  - Duration: 5-10 seconds before returning to idle

### 5.3 Advanced features & edge cases

- **Configuration Changes**: Admin updates LED count or topology while device online
  - Device receives new config via WebSocket, validates against hardware
  - If mismatch detected, device reports error but continues with safe defaults
  - Admin sees validation status in real-time (green check or red warning icon)

- **Network Interruptions**: WebSocket connection lost during gameplay
  - Device continues current animation loop from local memory
  - Switches to "disconnected" status pattern after 5 seconds
  - Automatically reconnects and re-syncs configuration when network restored

- **Multiple Devices**: Game host configures LEDs for 8 sensor clients + 2 motor clients
  - Each device shows player-specific color scheme from server's player assignments
  - Effects remain coordinated by server event broadcasts
  - Admin interface shows grid of all devices with live LED state previews

- **Custom Animations**: Developer uploads new effect pattern via web interface
  - Pattern defined in JSON with frame-by-frame color/brightness arrays
  - Server validates pattern syntax and memory requirements
  - New effect immediately available in animation preset dropdown
  - Can be triggered manually for testing before associating with game events

### 5.4 UI/UX highlights

- Web admin LED configurator uses drag-drop interface for physical layout mapping
- Real-time browser-based LED preview (WebGL canvas) shows patterns before deploying
- Color picker with team/player color presets from existing theme system
- Animation speed slider (0.1x to 5x) for effect tuning
- "Test Pattern" button sends specific animation to selected device for verification
- Visual indicator in admin UI shows which devices have LEDs configured and detected
- One-click "sync all" button broadcasts current configuration to all connected devices

---

## 6. Narrative

The game host arrives at the venue and powers on the Roll-a-Ball Derby system. As each ESP8266 sensor client boots, its LED strip pulses red briefly before connecting to WiFi and turning yellow. Within seconds, the LEDs glow green and transition to a gentle rainbow breathing pattern—the system is ready. The host opens the admin web interface on their laptop and sees all eight sensor clients displayed with small LED previews showing the rainbow pattern in sync.

As players gather, the host clicks "Start Game" and a synchronized countdown begins. All LED strips tick down in unison—blue flashes counting 3... 2... 1... GO! The LEDs shift to each player's assigned color as the race begins.

A player rolls their ball and it enters the +3 scoring hole. Instantly, their sensor's LED strip explodes in a golden sparkle cascade while the player's lane on other devices pulses gold. The spectators see the coordinated light show matching the on-screen action, creating an immersive arcade atmosphere.

As the race progresses, the sensor LEDs show each player's relative position as a filling progress bar. When one player takes the lead, their LEDs pulse green for a moment before returning to position tracking. The motor clients mirror this, with LEDs on the physical board highlighting the leader.

Finally, a winner crosses the finish line. Their sensor client erupts in a dazzling rainbow explosion with sparkling cascades for ten seconds. All other displays briefly flash "Nice Try" patterns before returning to the calm rainbow breathing of idle mode. The host resets the game with one click, and the synchronized ambient pattern resumes, ready for the next race.

Throughout the event, if a sensor client loses connection, its LEDs immediately switch to a slow red pulse, alerting the technician to the issue. When reconnected, it automatically resyncs and rejoins the light show.

---

## 7. Success metrics

### 7.1 User-centric metrics

- LED response latency from sensor trigger to first LED update: < 50ms
- Animation frame rate maintained at 30+ FPS under WiFi load
- Zero user-reported "LED glitching" or flickering issues during gameplay
- 90%+ of game hosts successfully configure LED effects without developer assistance
- Player satisfaction score increase of 15%+ in post-event surveys vs. non-LED version

### 7.2 Business metrics

- LED feature adoption rate: 75%+ of deployed games use LED clients within 6 months
- Support ticket reduction: 30%+ fewer "is it working?" questions due to visual status feedback
- Event booking conversion increase: 10%+ due to enhanced visual appeal in marketing materials
- Average setup time reduction: 20%+ due to auto-detection and web-based configuration

### 7.3 Technical metrics

- Memory usage: LED subsystem consumes < 50% of available ESP8266 RAM (< 40KB)
- WiFi stability: No degradation in WebSocket connection reliability with LEDs active
- Animation CPU overhead: < 30% of available CPU time at 60 FPS animation rate
- Configuration sync time: < 2 seconds from admin save to all devices updated
- Power consumption: Accurate power draw estimation within 10% for sizing calculations

---

## 8. Technical considerations

### 8.1 Integration points

- **Multi-Platform Support**: Use conditional compilation for ESP8266 vs ESP32 specific code paths
- **WebSocket Protocol**: Extend existing protocol with new message types: `led_config`, `led_effect`, `led_test`
- **Game Events**: Hook into existing event system (`GameEvent` enum in `websocket.h`) to trigger LED animations
- **Server REST API**: Add `/api/leds/config`, `/api/leds/effects` endpoints for admin configuration
- **Web Admin Client**: Extend `clients/web/` with new LED configuration panel using canvas for preview
- **Display Client**: Add LED state preview overlays showing what each physical device is displaying
- **Device Registration**: Include chip type (ESP8266/ESP32) in registration payload for platform-aware configurationiew
- **Display Client**: Add LED state preview overlays showing what each physical device is displaying

### 8.2 Data storage & privacy

- LED configurations stored in server memory (JSON), persisted to `server/data/led-config.json`
- No personally identifiable information collected through LED subsystem
- Animation pattern uploads validated for maximum size (10KB per pattern) to prevent abuse
- Device LED counts logged for diagnostics but not transmitted outside local network
- All LED control remains within local network, no cloud dependencies

### 8.3 Scalability & performance
ESP32 Memory**: 520KB RAM available, supports up to 2000+ LEDs theoretically (practical limit ~1000 LEDs per strand)
- **ESP8266 Method**: NeoPixelBus library with DMA method on GPIO3 (RX pin) or UART method on configurable pin for flicker-free output
- **ESP32 Method**: NeoPixelBus library with RMT (Remote Control Transceiver) hardware for precise timing, supports parallel output on multiple pins
- **Animation Processing**: Pre-compute animation frames on server, send as compressed keyframe data to reduce client CPU load
- **Multiple Strands**: ESP8266 supports one DMA output (single strand), ESP32 supports up to 8 parallel RMT channels (8 independent strands)
- **Broadcast Optimization**: Batch LED updates during high-frequency events (100ms cooldown per broadcast) to prevent WebSocket flooding
- **Power Supply**: Each LED draws ~60mA at full white, 300 LEDs = 18A max, requires external 5V supply with proper wiring (power injection every 50 LEDs)
- **Platform Detection**: Firmware auto-detects chip type at compile time, selects optimal LED control method automatically
- **Broadcast Optimization**: Batch LED updates during high-frequency events (100ms cooldown per broadcast) to prevent WebSocket flooding
- **Power Supply**: Each LED draws ~60mA at full white, 300 LEDs = 18A max, requires external 5V supply with proper wiring (power injection every 50 LEDs)

### 8.4 Potential challenges

- **Timing Precision**: WS2812B requires precise 800kHz timing, ESP8266 WiFi interrupts can cause glitches
  - *Mitigation*: Use hardware DMA or UART method via NeoPixelBus library, eliminates bit-banging issues
  
- **Voltage Level Shifting**: ESP8266 outputs 3.3V, WS2812B expects 5V logic
  - *Mitigation*: Use PNP emitter-follower buffer circuit (standard practice, documented in research), or rely on WS2812B's 3.3V tolerance (works in most cases)
  
- **Power Supply Sizing**: Users may underestimate LED current requirements
  - *Mitigation*: Admin interface calculates and displays required power supply wattage based on configured LED counts, shows warning if exceeded
  
- **Configuration Complexity**: Matrix wiring patterns (zigzag vs progressive) can confuse users
  - *Mitigation*: Web configurator provides visual wiring diagram preview, common presets (FastLED XY mapping conventions)
  
- **Animation Synchronization**: Network latency varies between devices by 10-50ms
  - *Mitigation*: Server broadcasts effects with timestamp, devices delay start to align, or accept "close enough" sync for non-critical effects
  
- **Firmware Size**: Adding full animation engine may exceed ESP8266 flash size limits
  - *Mitigation*: Use modular compilation, allow users to disable advanced effects for minimal builds, compress animation data

---

## 9. Milestones & sequencing

### 9.1 Project estimate

- **Size**: Medium (3-4 weeks, 80-120 dev hours)
- **Complexity**: Moderate — requires new C++ library, server API extensions, admin UI, and protocol changes

### 9.2 Team size & composition

- **Team size**: 2 developers
- **Roles**: 1 embedded firmware dev (ESP8266/C++), 1 full-stack dev (Node.js + vanilla JS)
- **Supporting**: 1 hardware tester (part-time for phy for both ESP8266 and ESP32
  - Create `LedController` C++ class with basic API (setColor, setBrightness, show)
  - Implement platform detection and automatic method selection (DMA/UART for ESP8266, RMT for ESP32)
  - Support strip topology with linear indexing
  - Integrate with existing `StatusLed` for connection state visualization
  - Test on physical hardware: 10-LED strip on ESP8266 sensor client and ESP32 variant
  - Key deliverables: `clients/shared/leds/LedController.h`, working strip demo on both platforms
  - Create `LedController` C++ class with basic API (setColor, setBrightness, show)
  - Support strip topology with linear indexing
  - Integrate with existing `StatusLed` for connection state visualization
  - Test on physical hardware: 10-LED strip on sensor client
  - Key deliverables: `clients/esp8266-sensor/src/leds/LedController.h`, working strip demo

- **Phase 2: Animation Engine & Effects** (1 week)
  - Implement effect base class and 6 core effects (solid, blink, pulse, rainbow, chase, sparkle)
  - Add non-blocking animation loop with configurable FPS
  - Hook effects to game events (countdown, scoring, winner)
  - Create effect transition system (crossfade support)
  - Test with 50-LED strip, verify 30 FPS maintained during WiFi activity
  - Key deliverables: `LedEffect.h`, effect library, working game event animations

- **Phase 3: Server Configuration & Sync** (1 week)
  - Extend WebSocket protocol with `led_config` message type
  - Add server endpoints: `GET/PUT /api/leds/config`, `POST /api/leds/effects/test`
  - Server stores LED configs per device type (sensor, motor)
  - Device sends detected LED count at registration, server validates
  - Broadcast configuration to all devices of matching type
  - Key deliverables: Server API, config persistence, auto-sync on registration

- **Phase 4: Web Admin Interface & Preview** (1 week)
  - Add LED configuration panel to `clients/web/admin.html`
  - Topology selector (strip, matrix 8x8, matrix 16x16, ring), LED count input
  - Effect preset dropdown, color picker, speed slider
  - Canvas-based LED preview (simulates the actual LED animations in browser)
  - "Test Pattern" button sends effect to selected device
  - Real-time status display showing all connected devices' LED states
  - Key deliverables: Admin UI, WebGL preview, device grid view

- **Phase 5: Matrix & Ring Support** (Stretch, +3-5 days)
  - Add XY coordinate mapping for matrix topologies (FastLED conventions)
  - Polar coordinate mapping for ring topologies
  - Matrix-specific effects (scrolling text, Pong-style animations)
  - Ring-specific effects (spinning, radial pulse)
  - Key deliverables: Enhanced `LedController` with topology abstraction, demo patterns

---
/ESP32 to automatically detect how many LEDs are connected so that I don't need to manually configure the LED count in firmware.
- **Acceptance criteria**:
  - On first boot, firmware attempts to communicate with WS2812B strand on configured pin
  - Device counts number of responsive LEDs by iterating until no more pixels respond
  - Detected count is sent to server in `register` message payload: `{ type: "sensor", chipType: "ESP8266", ledCount: 50 }`
  - Server stores detected count and chip type, validates against admin-configured expected count
  - If mismatch detected (±5 LEDs tolerance), server responds with warning in `registered` message
  - Device logs chip type and detected count to serial console for technician verification
  - Auto-detection runs only on first boot or after factory reset to minimize startup delay
  - Works identically on ESP8266 and ESP32 platforms
  - Device counts number of responsive LEDs by iterating until no more pixels respond
  - Detected count is sent to server in `register` message payload: `{ type: "sensor", ledCount: 50 }`
  - Server stores detected count and validates against admin-configured expected count
  - If mismatch detected (±5 LEDs tolerance), server responds with warning in `registered` message
  - Device logs detected count to serial console for technician verification
  - Auto-detection runs only on first boot or after factory reset to minimize startup delay

### 10.2 Connection state visualization

- **ID**: LED-002
- **Description**: As a game host, I want to see at a glance which ESP8266 devices are properly connected by looking at their LED status indicators.
- **Acceptance criteria**:
  - **No WiFi**: LEDs pulse red slowly (1 Hz)
  - **WiFi connected, WebSocket disconnected**: LEDs pulse yellow at 2 Hz
  - **WebSocket connected, not registered**: LEDs pulse green at 3 Hz  
  - **Fully registered and ready**: LEDs switch to ambient idle pattern (rainbow breathing)
  - **WebSocket reconnecting**: LEDs show orange chase pattern (indicates auto-reconnect in progress)
  - Status patterns run on first 8 LEDs (or all LEDs if fewer than 8), remaining LEDs stay dark during status display
  - Transition between states is smooth (1-second crossfade)

### 10.3 Basic scoring feedback

- **ID**: LED-003
- **Description**: As a player, I want my sensor's LEDs to flash instantly when I score so that I have immediate physical confirmation my roll counted.
- **Acceptance criteria**:
  - **+1 roll**: All LEDs flash blue for 200ms, return to previous pattern
  - **+2 roll**: All LEDs flash purple twice (200ms on, 100ms off, 200ms on)
  - **+3 roll**: All LEDs flash gold three times with trailing sparkle effect (300ms total)
  - **Zero roll**: First 25% of LEDs flash red for 500ms, fade out
  - Flash effect interrupts ambient pattern, then restores previous animation state
  - Latency from sensor trigger to first LED update: < 50ms (measured via oscilloscope)
  - Multiple scoring events within 500ms are queued and played sequentially (no dropped effects)

### 10.4 Countdown synchronization

- **ID**: LED-004
- **Description**: As a spectator, I want to see all the LED devices count down in sync with the game countdown so that the atmosphere is cohesive.
- **Acceptance criteria**:
  - Server broadcasts `countdown_tick` event via WebSocket when game countdown begins
  - All sensor and motor clients receive event simultaneously (within 50ms network variance)
  - On each tick, LEDs show blue pulse expanding from center (strip) or center pixel (matrix/ring)
  - Pulse duration: 600ms on, 400ms off (matches 1-second tick interval)
  - Final tick (count = 0) triggers brighter "GO!" animation: full white flash, 100ms
  - Countdown interrupts idle pattern, restores after countdown completes or game starts
  - Works across mixed device types (sensors and motors show identical countdown pattern)

### 10.5 Winner celebration

- **ID**: LED-005
- **Description**: As a winning player, I want my sensor's LEDs to play a special celebration animation so that my victory is recognized physically.
- **Acceptance criteria**:
  - Server broadcasts `winner` event with `{ playerId, name }` when player crosses finish line
  - Winning player's sensor client detects `playerId` match, triggers "winner" animation
  - **Winner animation**: Rainbow sparkle cascade (7 seconds), all LEDs cycle through HSV spectrum with random sparkles
  - Other players' devices trigger "loser" animation: brief red fade (2 seconds), return to idle
  - Motor client highlights winner's lane with golden pulse pattern (5 seconds)
  - Celebration animations are non-interruptible (game cannot be reset until animations complete)
  - Animation ends automatically, devices return to idle ambient pattern

### 10.6 Progress visualization

- **ID**: LED-006
- **Description**: As a player, I want to see my race progress visualized on my sensor's LED strip so that I know how close I am to winning.
- **Acceptance criteria**:
  - During active gameplay, sensor LEDs display progress bar: first N LEDs lit based on `position / trackLength`
  - Example: position 5 of 15 track → 33% of LEDs lit in player's assigned color
  - Remaining LEDs are dim (10% brightness) showing total track length
  - As player scores, progress bar fills smoothly (500ms animation per score increment)
  - When player reaches 100% (wins), progress bar fills completely then triggers winner celebration
  - Progress visualization overlays connection status (status pattern only shows when not in game)
  - Works on strips, matrices (horizontal bar), and rings (circular fill)
 for ESP8266, 1-1000 for ESP32), GPIO pin, brightness (0-100%)
  - Configuration saved via `PUT /api/leds/config` endpoint, persisted to `server/data/led-config.json`
  - Server broadcasts updated config to all connected devices of matching type immediately
  - Devices receive config, validate LED count matches detected hardware and chip capabilities (±5 tolerance)
  - If validation fails, device logs error to serial and admin UI shows red warning icon next to device
  - Config changes are only allowed when game is in `idle` state (prevented during active gameplay)
  - Admin UI shows chip type (ESP8266/ESP32) for each device with appropriate LED count limits
  - Admin web interface has "LED Configuration" section with device selector dropdown
  - Can configure: topology type (strip/matrix/ring), LED count (1-300), GPIO pin (default 4), brightness (0-100%)
  - Configuration saved via `PUT /api/leds/config` endpoint, persisted to `server/data/led-config.json`
  - Server broadcasts updated config to all connected devices of matching type immediately
  - Devices receive config, validate LED count matches detected hardware (±5 tolerance)
  - If validation fails, device logs error to serial and admin UI shows red warning icon next to device
  - Config changes are only allowed when game is in `idle` state (prevented during active gameplay)

### 10.8 Effect library management

- **ID**: LED-008
- **Description**: As a developer, I want to add custom LED effects through the web interface so that I can create unique animations without reflashing firmware.
- **Acceptance criteria**:
  - Admin interface has "Effect Library" tab showing list of available effects (built-in + custom)
  - "Add Effect" button opens effect editor with JSON schema for keyframe-based animations
  - Effect schema: `{ name, frames: [ { delay_ms, pixels: [ {r, g, b} ] } ] }`
  - Server validates uploaded effect: JSON syntax, max 100 frames, max 10KB total size
  - Valid effects saved to `server/data/led-effects/` directory, assigned unique ID
  - New effects appear in animation preset dropdown immediately (no restart required)
  - "Test Effect" button sends effect to selected device for preview before saving
  - Built-in effects cannot be deleted or edited (protected)

### 10.9 LED preview simulator

- **ID**: LED-009
- **Description**: As a game host, I want to see what the LED animations will look like before deploying them so that I can test different configurations without physical hardware.
- **Acceptance criteria**:
  - Admin interface includes WebGL-based LED simulator canvas
  - Simulator shape matches selected topology: horizontal strip, matrix grid, or circular ring
  - Each LED rendered as colored circle with brightness and glow effect
  - When effect preset selected, simulator plays the animation in real-time at correct FPS
  - Simulator receives same WebSocket events as physical devices (scoring, countdown, winner)
  - Can run multiple simulators simultaneously (one per device) to preview coordinated effects
  - Simulator performance: 60 FPS on modern browsers, works offline after initial page load

### 10.10 Diagnostic test mode

- **ID**: LED-010
- **Description**: As a hardware technician, I want to run a test pattern on the LEDs to verify wiring and diagnose failures.
- **Acceptance criteria**:
  - Admin interface has "Test Pattern" button for each connected device
  - Clicking button sends `led_test` WebSocket message to that device
  - Device enters test mode: iterates each LED sequentially in red, green, blue, white (500ms each)
  - Test pattern runs exactly once through all LEDs, then returns to previous state
  - Serial console logs test progress: "Testing LED 0/50... OK, Testing LED 1/50... OK"
  - If LED fails to respond, logs "Testing LED N/50... FAIL" and continues to next
  - Test results displayed in admin UI: "45/50 LEDs responding (5 failures at positions: 12, 23, 34, 45, 46)"
  - Test mode can be interrupted by starting a game or resetting the device

### 10.11 Matrix text scrolling

- **ID**: LED-011
- **Description**: As a game host, I want to display scrolling text on LED matrix displays to show player names or messages.
- **Acceptance criteria**:
  - Supported on 8x8 and 16x16 matrix topologies only
  - Admin interface includes "Matrix Message" text input and "Send" button
  - Message sent via WebSocket to all devices with matrix topology
  - Device renders text using 5x7 bitmap font, scrolls right-to-left across matrix
  - Scroll speed configurable (default: 1 pixel every 100ms)
  - Text color uses player's assigned color, background is black
  - Maximum message length: 50 characters (prevents memory overflow)
  - Message scrolling interrupts current animation, restores after message completes
  - Special keywords supported: `{player}` → player name, `{score}` → current score

### 10.12 Power consumption estimation

- **ID**: LED-012
- **Description**: As a hardware technician, I want to know the estimated power consumption of my LED configuration so that I can size the power supply correctly.
- **Acceptance criteria**:
  - Admin interface calculates power draw using formula: `LED_count × 60mA × brightness_percent`
  - Calculation displayed in real-time as user adjusts LED count and brightness sliders
  - Example: "300 LEDs at 80% brightness = 14.4A (72W at 5V)"
  - Warning displayed if calculated draw exceeds common power supply sizes (5A, 10A, 20A)
  - Recommendation shown for power injection intervals: "Inject power every 50 LEDs for this configuration"
  - Calculation accounts for average brightness of selected effect (rainbow = 50%, white solid = 100%)
  - Export button generates printable power budget report for entire installation (all devices)

### 10.13 Multi-device coordination

- **ID**: LED-013
- **Description**: As a spectator, I want to see wave effects that propagate across multiple devices so that the lighting feels cohesive across the entire physical game.
- **Acceptance criteria**:
  - Server broadcasts "wave" effect event with `{ startDevice, direction, color, speed }`
  - All devices receive event, compute their position in wave sequence based on device ID order
  - Wave propagates: first device lights immediately, subsequent devices delayed by `speed × position`
  - Example: 8 sensors, 200ms speed → device 0 at T+0ms, device 1 at T+200ms, device 2 at T+400ms, etc.
  - Wave effect overlays current animation for 2 seconds, then fades out
  - Supports forward and reverse directions
  - Works across device types: can start at sensor 1, flow to sensors 2-8, then to motor clients
  - Network latency compensation: server sends timestamp, devices delay start to align

### 10.14 Emergency stop

- **ID**: LED-014
- **Description**: As a game host, I want an emergency stop button that immediately turns off all LEDs in case of power issues or emergencies.
- **Acceptance criteria**:
  - Admin interface has red "EMERGENCY STOP" button always visible in top-right corner
  - Clicking button sends `led_emergency_stop` WebSocket message to all connected devices
  - All devices immediately turn off all LEDs (set brightness to 0, clear buffers)
  - WebSocket message has highest priority (bypasses normal message queue)
  - Devices remain in stopped state until server sends `led_resume` message
  - "Resume LEDs" button appears in admin UI after emergency stop triggered
  - Emergency stop state persists across WebSocket reconnections (device checks state on registration)
  - Serial console logs emergency stop event with timestamp for incident reporting

---for both ESP8266 (DMA/UART) and ESP32 (RMT)
   - Platform abstraction layer design (compile-time chip detection, method selection)
   - WebSocket protocol extensions with complete message schemas including chip type
   - Server REST API endpoint specifications with platform-aware validation
   - Web admin UI component architecture and state management
   - Memory and performance budgets with profiling strategies for both platforms
   - Migration path for existing ESP8266 firmware to shared codebase

2. **Hardware procurement**: Acquire test LED hardware (WS2812B strips, 8x8 matrix, 16-LED ring) for both ESP8266 and ESP32 development and validation

3. **Create implementation plan**: Use the `create-implementation-plan` skill to break down the phased approach into actionable tasks with clear acceptance criteria

4. **Setup test environment**: Configure PlatformIO test environments for both ESP8266 and ESP32 with NeoPixelBus library and hardware-in-loop testing framework

5. **Platform strategy**: Define recommended chip selection guidelines (ESP8266 for simple sensors, ESP32 for motor clients with multiple LED strands)
   - Web admin UI component architecture and state management
   - Memory and performance budgets with profiling strategies

2. **Hardware procurement**: Acquire test LED hardware (WS2812B strips, 8x8 matrix, 16-LED ring) for development and validation

3. **Create implementation plan**: Use the `create-implementation-plan` skill to break down the phased approach into actionable tasks with clear acceptance criteria

4. **Setup test environment**: Configure PlatformIO test environment with NeoPixelBus library and hardware-in-loop testing framework

---

## Related Documentation

- **Technical Specification**: [spec/spec-led-control.md](../spec/spec-led-control.md) - Complete architecture, API design, and implementation guidance
- **GitHub Issues**: [#4-#17](https://github.com/tamaygz/esp-rollaball-derby/issues?q=is%3Aissue+label%3Aled-control) - User stories with detailed acceptance criteria and spec cross-references
- **Main PRD**: [docs/PRD.md](PRD.md) - Overall Roll-a-Ball Derby product requirements
- **Server README**: [server/README.md](../server/README.md) - Server architecture and WebSocket protocol

