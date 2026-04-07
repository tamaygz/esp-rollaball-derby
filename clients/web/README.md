# Roll-a-Ball Derby — Web Admin Client

Vanilla JS SPA served by the game server at `/admin`. Dual purpose: game host admin panel and sensor testing tool.

## Features

- **Game controls** — Start, pause/resume, reset
- **Config** — track length, max players, theme including auto-random (locked while game running)
- **Players** — live list with progress bars, inline rename, remove, connection status
- **Score simulator** — send +1 / +2 / +3 via WebSocket to test sensor flow
- **Server-side bots** — add/remove autonomous bot players via REST API
- **LED Configuration** — configure LED strips, preview effects in real-time, test on devices
- **Event log** — real-time feed of all game events
- **Auto-reconnect** — exponential backoff (1s → 30s) on WS disconnect

## Usage

No build step. The game server serves this directory statically at `/admin`.

1. Start the server: `cd server && npm start`
2. Open `http://localhost:3000/admin` in a browser
3. Open `http://localhost:3000/admin/devices.html` for the devices debug page

Player name is remembered in `localStorage` (key: `derby-player-name`).

## File Structure

```
clients/web/
├── index.html          — SPA shell, 9 card sections (added LED Configuration)
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
- `POST /api/leds/test` — Send test effect to specific device

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
