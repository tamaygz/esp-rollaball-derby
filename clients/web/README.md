# Roll-a-Ball Derby — Web Admin Client

Vanilla JS SPA served by the game server at `/admin`. Dual purpose: game host admin panel and sensor testing tool.

## Features

- **Game controls** — Start, pause/resume, reset
- **Config** — track length, max players, theme including auto-random (locked while game running)
- **Players** — live list with progress bars, inline rename, remove, connection status
- **Score simulator** — send +1 / +2 / +3 via WebSocket to test sensor flow
- **Server-side bots** — add/remove autonomous bot players via REST API
- **LED Configuration** — configure LED strips, preview effects in real-time, test on devices
- **Motor Control** — configure track colors, jog motors, calibrate positions for ESP32 devices
- **Event log** — real-time feed of all game events
- **Auto-reconnect** — exponential backoff (1s → 30s) on WS disconnect

## Usage

No build step. The game server serves this directory statically at `/admin`.

1. Start the server: `cd server && npm start`
2. Open `http://localhost:3000/admin` in a browser
3. Open `http://localhost:3000/admin/devices` for the devices debug page

Player name is remembered in `localStorage` (key: `derby-player-name`).

## File Structure

All pages are EJS templates served by Express from `server/views/admin/`:

| Route | Template |
|-------|----------|
| `/admin` | `admin/index.ejs` |
| `/admin/devices` | `admin/devices.ejs` |
| `/admin/leds` | `admin/leds.ejs` |
| `/admin/debug-player` | `admin/debug-player.ejs` |

```
clients/web/
├── css/
│   └── style.css       — dark theme, CSS custom properties, responsive grid, LED UI
└── js/
    ├── connection.js   — WebSocket client (Derby.Connection)
    ├── state.js        — State tracker + DOM renderer (Derby.State)
    ├── admin.js        — REST game controls + player rename/remove (Derby.Admin)
    ├── test.js         — Score simulation panel (Derby.Test)
    ├── bots.js         — Server-side bot management (Derby.Bots)
    ├── led-admin.js    — LED device config + test controls (Derby.LED)
    ├── led-simulator.js— Canvas-based LED strip renderer (Derby.LEDSimulator)
    ├── led-effects.js  — Animation effect library (Derby.LEDEffects)
    └── main.js         — Entry point, message router
```

Scripts load in order: `connection → state → admin → test → bots → led-effects → led-simulator → led-admin → main`.

## Module Namespace

All modules live on `window.Derby`:

| Module | Responsibilities |
|--------|-----------------|
| `Derby.Connection` | WS open/close/send/onMessage, reconnect backoff |
| `Derby.State` | Render full game state to DOM, log entries, winner banner |
| `Derby.Admin` | REST POST start/pause/reset, PUT config/rename, DELETE player |
| `Derby.Test` | Score button state management, send score via WS |
| `Derby.Bots` | REST GET/POST/DELETE /api/bots, render bot list with status badges |
| `Derby.LED` | LED device config, test controls, simulator integration |
| `Derby.LEDSimulator` | Canvas 2D renderer for LED strips/matrices/rings with animation loop |
| `Derby.LEDEffects` | Effect library (solid, blink, pulse, rainbow, chase, sparkle) |

## Security

- User-supplied strings rendered via `textContent` or `_esc()` helper — no raw `innerHTML`
- Player name edit uses `document.createElement('input')`, not template strings
- Rename API calls use `encodeURIComponent` on player IDs

## LED Configuration

The LED Configuration section provides real-time preview and management of LED strips connected to ESP8266 devices.

### Features

1. **Device List** — Shows all connected sensor/motor devices with LED capability
   - Live connection status indicators (green = connected, gray = disconnected)
   - LED count badges showing detected hardware
   - Mini LED preview showing current device state
   - Click to select device for configuration

2. **Configuration Form** — Per-device LED settings
   - **LED Count** — Number of LEDs in strip (1-300)
   - **GPIO Pin** — Hardware pin (e.g., D4, GPIO2)
   - **Topology** — Strip layout type:
     - `strip` — Linear horizontal strip
     - `matrix-zigzag` — Zigzag wiring (even rows reversed)
     - `matrix-progressive` — Sequential wiring (all rows same direction)
     - `ring` — Circular ring layout
   - **Matrix Dimensions** — Rows × Columns (only for matrix topologies)
   - **Brightness** — Global brightness percentage (0-100%)

3. **LED Simulator** — Canvas-based real-time preview
   - Renders up to 300 LEDs with glow effects
   - Updates at 30+ FPS using `requestAnimationFrame`
   - Reflects topology, count, and brightness settings
   - Shows effect preview before sending to device

4. **Effect Library** — 6 built-in animations
   - **Solid** — Static color fill
   - **Blink** — Binary on/off toggle
   - **Pulse** — Smooth breathing animation (sinusoidal)
   - **Rainbow** — HSV hue rotation across strip
   - **Chase** — Moving window of lit LEDs
   - **Sparkle** — Random LED flashing

5. **Test Controls** — Send effects to physical devices
   - Select effect from dropdown
   - Choose color from 16-color player palette
   - Adjust animation speed (100-5000ms)
   - Test on Device button (1-second rate limit)

### API Integration

LED admin uses Phase 3 REST endpoints:

- `GET /api/leds/config` — Fetch current LED configurations
- `PUT /api/leds/config/:deviceType` — Save configuration (broadcast to devices)
- `POST /api/leds/effects/test` — Send test effect to specific device

### Architecture

**Three-layer design:**

1. **`led-admin.js`** — UI controller
   - Device list management
   - Form population and validation
   - API calls for save/test operations
   - Event handling for user interactions

2. **`led-simulator.js`** — Canvas renderer
   - Pixel buffer abstraction (Array of RGB objects)
   - Topology renderers (strip/matrix/ring)
   - Animation loop with delta time
   - Brightness control (0-255)

3. **`led-effects.js`** — Effect library
   - Effect classes with `update(deltaTime)` pattern
   - Color utilities (`hexToRgb`, `hsvToRgb`)
   - Stateful animation instances

### Usage Example

```javascript
// Initialize LED admin (called from main.js)
Derby.LED.init();

// Update device list from WebSocket state messages
Derby.LED.updateDeviceList(devices);

// Programmatically select a device
Derby.LED.selectDevice('device-id-here');

// Use simulator directly
var sim = new Derby.LEDSimulator();
sim.init('canvas-id');
sim.setConfig({ ledCount: 60, topology: 'strip' });
sim.start();
sim.playEffect('rainbow', { speed: 1000 });
```

### Troubleshooting

**Simulator not showing effects:**
- Check browser console for JavaScript errors
- Verify `Derby.LEDSimulator` and `Derby.LEDEffects` are defined before `Derby.LED.init()` runs
- Ensure canvas element has correct ID and is visible

**Device not responding to test effects:**
- Verify device is connected (green status indicator)
- Check device firmware is running Phase 3 LED Control Layer
- Review server logs for WebSocket broadcast errors
- Confirm device has valid LED configuration saved

**Mini previews not animating:**
- Mini simulators use subtle pulse effects
- Check that `device.playerColor` is set (fallback: #90cdf4)
- Verify device LED count is detected (`device.ledCount`)

**Configuration save fails:**
- Check LED count is within 1-300 range
- For matrix topologies, ensure rows × cols ≤ 300
- Verify GPIO pin format matches device expectations
- Review server response for validation errors

## Motor Control & Track Colors

The Devices page (`/admin/devices`) provides motor control and track color configuration for ESP32 motor devices.

### Features

1. **Motor Device List** — Shows all connected ESP32 motor devices
   - Device name, ID, and connection status
   - Motor count badge (number of stepper lanes)
   - "Motor Control" button to open configuration panel

2. **Track Colors Configuration**
   - Visual color picker for each physical track/lane
   - Live swatch preview showing selected colors
   - Color palette from `clients/assets/themes/shared/player-colors.json` (16 distinct colors)
   - Save button persists configuration to ESP32 and server

3. **Motor Jog Controls**
   - Individual lane jog buttons (forward/backward)
   - Configurable step size (1–100 steps)
   - Real-time status display

4. **Motor Calibration**
   - Per-lane calibration workflows
   - Position tracking and verification
   - Reset to default positions

### Track Color Mapping

Each physical lane is assigned a color index (0–15). During gameplay, the ESP32 matches player positions to lanes based on `g_motorColors[]`:

```cpp
// Example: Lane 0 is assigned Red (index 0)
for (int lane = 0; lane < motorCount; lane++) {
  for (int p = 0; p < positions[p].count; p++) {
    if (g_motorColors[lane] == positions[p].colorIndex) {
      // Move lane to match player position
      motorManager.moveLaneToNormalized(lane, positions[p].position);
    }
  }
}
```

### Usage

1. Navigate to `/admin/devices`
2. Find ESP32 motor device in "Motor Devices" section
3. Click "Motor Control" button
4. In "Track Colors" section:
   - Select color for each lane from dropdown
   - Preview changes with color swatches
   - Click "Save Track Colors" to persist
5. Server validates colors (clamps to 0–15 range)
6. On success, ESP32 saves to `/state.json` for persistence across reboots

### API Integration

Track colors use the following endpoints:

- `GET /api/clients` — Returns `motorCount` and `motorColors` for motor devices
- `POST /api/clients/:id/motor/colors` — Update track colors (proxies to ESP32 `/api/motor/colors`)

### Configuration Persistence

- **ESP32:** Saved in `/state.json` via atomic write (STATE_TMP → STATE_FILE rename)
- **Server:** Updated in-memory on successful ESP32 save only (no optimistic updates)
- **Validation:** All layers clamp color indices to 0–15 range

### Troubleshooting

**"Motor client not found" error:**
- Verify device type is `'motor'` (check GET /api/clients response)
- Ensure device completed WebSocket registration
- Check device ID matches URL parameter

**Colors not saving:**
- Check browser console for fetch errors
- Verify ESP32 is reachable on local network
- Review server logs for proxy failures
- Confirm ESP32 firmware has POST /api/motor/colors endpoint

**Device shows old colors after restart:**
- Check ESP32 logs for state file load errors
- Verify `/state.json` exists on ESP32 filesystem
- Re-save colors to force new write
- Check file system not full (ESP32 has ~3MB SPIFFS)

**Color swatches not previewing:**
- Verify `PLAYER_COLORS` array loaded from JSON
- Check browser console for player-colors.json fetch errors
- Ensure color indices are 0–15 (validation logs in console)

