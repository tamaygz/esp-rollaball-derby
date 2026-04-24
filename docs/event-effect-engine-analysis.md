# Event / Effect Engine — Analysis & Concept

> **Purpose:** In-depth analysis of the current event/effect pipeline across all projects, diagnosis of known pain points, and a concrete concept for a generalised, extensible future architecture.

---

## 1. Current System — How It Actually Works

### 1.1 End-to-End Pipeline

```
IR Sensor fires
   │
   ▼
ESP8266/ESP32 sensor  ──── WS: {type:"score"} ────────────────────────►  Node.js Server
                                                                              │
                                                              GameState.score() builds events[]
                                                              ["score_3", "took_lead", ...]
                                                                              │
                    ◄─── WS: {type:"scored", playerId, events[]} ────────────┤
                    ◄─── WS: {type:"scored", playerId, events[]} ──────────  │  ──►  Display client (Pixi.js)
                    ◄─── WS: {type:"scored", playerId, events[]} ──────────  │  ──►  Web admin
                                                                              │  ──►  ESP32 motor
```

Every client receives the same `scored` broadcast.  
Each client **independently decides** what to render based on its own capabilities and scope rules.

---

### 1.2 Actors and Their Scopes

| Actor | Type | Event Scope | LED / FX Output |
|---|---|---|---|
| **ESP8266/ESP32 sensor** | Hardware, Arduino | Player-local (filter by `playerId`) | WS2812 strip via `GameEventMapper` |
| **ESP32 motor** | Hardware, Arduino | Player-local + global lifecycle | WS2812 matrix via `MatrixDisplay` |
| **Display client** | Browser, Pixi.js | All players + global | Pixi animations, GSAP tweens |
| **Web admin** | Browser, vanilla JS | Observes state only | No FX output |
| **SoundManager** | Node.js, server-side | All players + global | OS audio (wav files) |

---

### 1.3 Event Taxonomy

#### Game Events (server → all clients)
| WS Type | Payload Key | Values |
|---|---|---|
| `scored` | `events[]` | `zero_roll`, `score_1`, `score_2`, `score_3`, `streak_zero_3x`, `streak_three_2x`, `took_lead`, `became_last` |
| `game_event` | `event` | `game_started`, `game_paused`, `game_resumed`, `game_reset` |
| `countdown` | `count` | `3`, `2`, `1` |
| `winner` | `playerId`, `name` | — |

#### LED Configuration (server → devices)
| WS Type | Direction | Notes |
|---|---|---|
| `led_config` | server → device | After registration or admin change; includes `deviceColor` hex |
| `test_effect` | server → single device | Admin-triggered one-shot effect |

#### Device → Server
| WS Type | From | Payload |
|---|---|---|
| `register` | sensor/motor | `type`, `playerName?`, `playerId?`, `chipId`, `chipType` (`ESP8266`\|`ESP32`), `ledCount?`, `ledCapabilities?` (LED pin/topology constraints), `deviceCapabilities?` (motor-only: buttons / matrix / steppers) |
| `score` | sensor | `playerId`, `points` |
| `button` | motor | `buttonIndex`, `action` (`start`/`reset`/`pause`/`resume`) |

> **Note (post-PR#24):** `ledCapabilities` and `deviceCapabilities` are **separate** fields now. Before PR#24 the motor sent a single `capabilities` blob that mixed both concerns; it was split to prevent the LED subsystem from inheriting motor-only flags and vice versa. Any new refactor must preserve this separation.

---

### 1.4 Firmware: The Canonical Shared Layers (`clients/shared/`)

Since PR#24 the shared C++ code is split across **two** sibling libraries, both consumed via PlatformIO `lib_deps`:

```
clients/shared/leds/                 — LED effect engine (event → pixels)
  GameEvents.h          — canonical enum definitions (LocalEventType, GlobalEventType, LedTestEffectMessage)
  GameEventMapper.h     — maps enums → AnimationManager.playEffect()
  AnimationManager.h/cpp — non-blocking loop, FPS control, crossfade transitions
  LedEffect.h           — pure-virtual base (begin, update, isComplete)
  LedController.h/cpp   — platform abstraction (UART1/DMA for ESP8266, RMT for ESP32)
  LedPlatform.h         — per-platform type aliases, LED_MAX_COUNT, LED_GPIO_MAX (ESP32 only),
                          and the canonical pin-validation helper `ledPinIsValid(pin)`
  effects/              — SolidEffect, BlinkEffect, PulseEffect, RainbowEffect, ChaseEffect, SparkleEffect

clients/shared/io/                   — device identity & low-level IO helpers
  device_info.h         — DERBY_CHIP_ID_HEX_MAX_LEN (17), derbyChipType(), derbyChipSuffix16(),
                          derbyChipIdHex() — single source of truth for the firmware `chipId` string
  color_utils.h         — derbyParseHexColor("#RRGGBB", r, g, b) used by both sensor & motor `websocket.cpp`
  StatusLed             — onboard LED helper (ESP32 NeoPixel / ESP8266 GPIO fallback)
  ButtonManager         — debounced button array (motor)
  library.json          — PlatformIO manifest so sensor/motor both `lib_deps = ^clients/shared/io`
```

**Key insight:** `LocalEventType` enum values are ordered by priority — so firmware just picks `max(candidates)`.

**Why the split matters for the refactor:** The event queue, effect-layer, and shared JS constants proposed in §4 all live in `clients/shared/leds/`. The `io/` sibling is *upstream* of the LED engine (device identity must be established before LED config arrives). Keep that layering: `leds/` may `#include` `<io/device_info.h>` but never the reverse.

**Pin validation is already centralised.** `ledPinIsValid(pin)` in `LedPlatform.h` returns true only for ESP8266 GPIO2/GPIO3 or ESP32 GPIO0–39. Any future component that accepts a user-configurable LED pin (sensor config, motor config, admin UI round-trip) must call this helper — do not re-derive the rules.

**Firmware event flow:**
```
WS message arrives (ISR-like callback)
  → _pendingLocalEvent or _pendingGlobalEvent set (single slot)
main loop()
  → ws.pollLocalEvent() / ws.pollGlobalEvent()
  → ledManager.onLocalEvent() / onGlobalEvent()
  → GameEventMapper maps to effect
  → AnimationManager.playEffect() (interrupts current)
  → AnimationManager.loop() renders frames
  → On isDone() → return to ambient effect
```

---

### 1.5 Display Client: Multi-Event Fan-out

Unlike firmware (single active effect), the display client **runs all events in parallel**:

```javascript
// ActionEffect.js showEffect(lane, events)
// Base effects fire immediately:
if (hasZeroRoll) _fxZeroRoll(lane);
if (hasScore3)   _fxScore3(lane);
// Streaks delayed 150-200ms so pop-ups layer visually:
if (hasStreakThree2x) setTimeout(() => _fxStreakThree2x(lane), 200);
// Rank changes delayed last (most prominent):
if (hasTookLead) setTimeout(() => _fxTookLead(lane), 300);
```

This is intentional — GSAP tweens on different DOM/Pixi properties don't conflict.

---

### 1.6 SoundManager: Server-Side Audio

The server plays WAV files through the host OS. Event-to-file mapping is independent of the LED system:

```
scored  → priority pick from: took_lead / became_last / streak events > score_N
game_event → direct map (game_started, game_paused, ...)
countdown → countdown_tick / countdown_go
winner → winner.wav
```

---

## 2. Diagnosed Problems

### P1 — Single-slot event buffer in firmware
`_pendingLocalEvent` and `_pendingGlobalEvent` are single variables. If two events arrive in the same WS callback (e.g., a `scored` message followed immediately by a `game_event`), the second overwrites the first.

**Symptom observed:** Race between local scoring effect and a near-simultaneous `game_started`.

### P2 — Global events unconditionally cancel local effects
`AnimationManager.playEffect()` always interrupts the current effect. A `GAME_PAUSED` message arriving milliseconds after a local `SCORE_PLUS3` silently kills the sparkle.

**There is no grace period, priority comparison, or queue.**

### P3 — `_applyMotorColorSync` calling `assignColor(chipId, firstLaneColor)` where `firstLaneColor` is a number, not a `Set<number>`
**Status (post-PR#24): STILL OPEN.** `server/src/ws/ConnectionManager.js:701` still calls `this.ledConfigManager.assignColor(client.chipId, firstLaneColor)` where `firstLaneColor` is a plain number, while `LedConfigManager.assignColor(chipId, alreadyUsed = new Set())` expects an iterable. The spread `[...alreadyUsed]` will throw `TypeError: number is not iterable` the moment the persisted color is unavailable. The reason it hasn't surfaced in practice is that the first-lane color is almost always already in `deviceColorMap`, so the early-return branch fires before the spread runs.

Fix is trivial — use `updateDeviceColor(chipId, colorIndex)` (or pass `new Set([firstLaneColor])`) — but must be retained in Phase 0.

### P4 — LED config is per-device-type, not per-device
`PUT /api/leds/config/:deviceType` broadcasts to **all** connected devices of that type. An admin updating one sensor's LED count sends the new config to every sensor. There is no per-`chipId` override path in the REST API.

**Status (post-PR#24): PARTIALLY MITIGATED on the client, unchanged on the server.** `clients/web/js/led-admin.js` now carries a `CHIP` table (ESP8266/ESP32) with per-platform `defaultPin`, `defaultTopology`, and `maxLeds`, and validates pins against `pinGroup` before PUT. The server's `LedConfigManager.getConfigForDeviceType(deviceType)` still ignores `chipType`, so an ESP8266 sensor and an ESP32 sensor connected simultaneously still receive the same broadcast config. `validateDeviceLedCount(deviceType, reportedCount, chipType)` does take chipType but only for the count ceiling. §4.6 below is unchanged as a refactor target.

### P5 — `deviceColor` not persisted on firmware
**Status (post-PR#24): RESOLVED.** Both `clients/esp8266-sensor/src/main.cpp` and `clients/esp32-motor/src/main.cpp` now persist `device_color_r/g/b` and `hasDeviceColor` into the LittleFS `state.json` (alongside WiFi creds / serverIp). `websocket.cpp` writes the saved config on `led_config` receipt via `derbyParseHexColor()`; `setup()` reads it back before the first WS frame. The white-flash-after-reboot window is closed.

Keep this behaviour during the event/effect refactor — any new config fields that require low-latency recovery on boot should follow the same `LittleFS state.json` pattern (not NVS KV — the project standardised on LittleFS).

### P6 — `test_effect` has no TTL / stop message
Admin-triggered effects run indefinitely with no automatic stop. The only way out is: re-send a different effect, disconnect/reconnect the device, or wait for the next `led_config`. This makes test-mode sticky.

### P7 — Event string names duplicated across all layers
Server emits string names (`"took_lead"`, `"streak_zero_3x"`, ...).  
Firmware `websocket.cpp` has a hand-rolled `strcmp` mapping to `LocalEventType`.  
Display `ActionEffect.js` has `events.indexOf('took_lead')` literals.  
SoundManager has `EVENT_FILE_MAP` string literals.  
If a new event name is added or renamed, all four places must be updated manually.

### P8 — No event versioning or sequence numbers
Events carry no wall-clock timestamp or sequence number. Clients have no way to:
- Detect out-of-order delivery
- Debounce duplicate `scored` broadcasts after reconnect
- Know whether an event is stale

### P9 — PLATFORM defaultTopology missing in led-admin.js
**Status (post-PR#24): RESOLVED.** `clients/web/js/led-admin.js` now declares `defaultTopology: 'strip'` on every `CHIP` entry, and the per-device resolution (line ~383) is `stored.topology || chip.defaultTopology || 'strip'`. No silent fall-back path remains.

---

## 3. Best-Practice Research

### Industry patterns for distributed real-time effect systems

**Authoritative-server model (game networking best practice):**
> "Clients send intents; the server broadcasts facts."  
> — [Gaffer On Games: State Synchronisation](https://gafferongames.com/post/snapshot_compression/)

Applied here: sensors send `score`, server computes `events[]`, all clients react. ✅ Already correct.

**Event sourcing for reproducibility:**
Storing events as an append-only log allows:
- Replay for debugging
- Reconnect re-hydration (send missed events since last sequence)
- A/B testing different effect implementations against the same event stream

**Finite State Machine (FSM) for game lifecycle:**
Game states (`idle → countdown → running → finished → idle`) are already implicitly present in `GameState.status`. Making them explicit as an FSM prevents invalid transitions and makes event dispatch conditional (e.g., `GAME_PAUSED` is only valid from `running`).

**Priority queue for effects on constrained hardware:**
Instead of a single-slot buffer, use a small bounded queue (depth 3-4). On firmware this is just a circular buffer of `uint8_t` enums — negligible SRAM cost.

**Effect layers / channels:**  
Separate effects by channel so they can blend without one cancelling another:
- **Ambient layer** (base state: connection status, game pause)
- **Event layer** (transient: scoring, streaks, rank changes)
- **Admin layer** (test effects, overrides)

Higher channel wins; lower channel resumes when higher completes.

**Shared schema (single source of truth for event names):**  
Define event names in one canonical file (already done for C++ in `GameEvents.h`). The JavaScript side should mirror this — e.g., a `clients/shared/js/gameEvents.js` module that exports the same string constants.

---

## 4. Proposed Architecture — Generalised Event-Effect Engine

### 4.1 Core Principle: Three-Layer Stack

```
┌──────────────────────────────────┐
│  LAYER 3 — ADMIN / OVERRIDE      │  test_effect (admin), config override
│  (always wins while active)      │
├──────────────────────────────────┤
│  LAYER 2 — GAME EVENTS           │  scored, winner, countdown, lifecycle
│  (transient, duration-bound)     │
├──────────────────────────────────┤
│  LAYER 1 — AMBIENT               │  connection status, game pause, idle
│  (always running underneath)     │
└──────────────────────────────────┘
```

Each layer has its own `AnimationManager` (or equivalent). When L3 effect ends, L2 resumes; when L2 ends, L1 resumes.

Cost on firmware: 3× `AnimationManager` instances ≈ 3 × ~40 bytes of state — well within SRAM budget.

### 4.2 Event Queue (firmware)

Replace the single `_pendingLocalEvent` / `_pendingGlobalEvent` slots with small ring buffers:

```cpp
// In websocket.h / GameEventQueue.h (shared)
template<typename T, uint8_t N>
class EventQueue {
  T    _buf[N];
  uint8_t _head = 0, _tail = 0, _count = 0;
public:
  bool push(T ev) { /* overwrite oldest if full (drop lowest priority) */ }
  bool pop(T& out);
  bool isEmpty() const { return _count == 0; }
};

// Usage (N=4 events each, negligible overhead)
EventQueue<LocalEventType,  4> _localQueue;
EventQueue<GlobalEventType, 4> _globalQueue;
```

On full queue, overwrite the **lowest-priority** entry (not the oldest), so `TOOK_LEAD` is never lost to three `SCORE_PLUS1` events.

### 4.3 Shared Event Name Constants (JS side)

Add `clients/shared/js/gameEvents.js` (served as static asset, importable everywhere):

```javascript
// clients/shared/js/gameEvents.js
var GameEvents = Object.freeze({
  // Local events (sensor/motor filtered by playerId)
  ZERO_ROLL:       'zero_roll',
  SCORE_1:         'score_1',
  SCORE_2:         'score_2',
  SCORE_3:         'score_3',
  STREAK_ZERO_3X:  'streak_zero_3x',
  STREAK_THREE_2X: 'streak_three_2x',
  TOOK_LEAD:       'took_lead',
  BECAME_LAST:     'became_last',
  // Global lifecycle events
  GAME_STARTED:    'game_started',
  GAME_PAUSED:     'game_paused',
  GAME_RESUMED:    'game_resumed',
  GAME_RESET:      'game_reset',
  // Special
  COUNTDOWN_TICK:  'countdown_tick',
  WINNER:          'winner',
});
```

Use `GameEvents.TOOK_LEAD` instead of `'took_lead'` literals everywhere in JS.  
The C++ `GameEvents.h` enum already covers the firmware side.

### 4.4 Effect Channel Interface (shared C++ concept)

```cpp
// clients/shared/leds/EffectLayer.h  (new, shared)
class EffectLayer {
public:
  enum Priority : uint8_t { AMBIENT = 0, GAME = 1, ADMIN = 2 };

  // Set a looping background effect for this layer.
  void setAmbient(LedEffect* effect);
  // Play a one-shot transient on top; returns to setAmbient() when done.
  void playTransient(LedEffect* effect, uint16_t fadeMsOut = 0);
  // Layer update (call every loop())
  void loop();
  // Is a transient currently active?
  bool isTransientActive() const;

private:
  LedEffect*        _ambient    = nullptr;
  AnimationManager  _ambientAnim;
  AnimationManager  _transientAnim;
};
```

`LedManager` (in each firmware project) owns three `EffectLayer` instances. Event routing becomes:

```cpp
// Admin test_effect → highest layer
_layers[ADMIN].playTransient(&_testEffect);

// Global game event → mid layer
_layers[GAME].playTransient(&_mappedEffect);

// Ambient state → bottom layer
_layers[AMBIENT].setAmbient(&_connectionEffect);
```

### 4.5 Per-Device LED Config (server)

The current `PUT /api/leds/config/:deviceType` path is coarse (type-wide). Add a per-device override:

```
GET  /api/leds/config/:deviceType/:chipId   — read device override (falls back to type default)
PUT  /api/leds/config/:deviceType/:chipId   — write device override
DELETE /api/leds/config/:deviceType/:chipId — revert to type default
```

`broadcastLedConfig()` computes the resolved config per device: `deviceOverride ?? typeDefault`.

This unblocks: setting `sensor/ESP8266` to GPIO2 and `sensor/ESP32` to GPIO4 without separate device types, and generally allows per-player LED customisation.

### 4.6 Chiptype-Aware LED Config Defaults

> **Status (post-PR#24): PARTIALLY IMPLEMENTED on the client; NOT YET on the server.** The web admin's `CHIP` table in `led-admin.js` already resolves per-platform defaults (`ESP8266 → defaultPin 2`, `ESP32 → defaultPin 4`, each with their own `maxLeds` and `pinGroup`). The server broadcasts whatever is stored in `led-config.json` verbatim; it does not re-resolve per recipient.

The immediate fix for comment #3134833016 is to store chiptype-aware defaults in `led-config.json`:

```jsonc
{
  "sensor": {
    "gpioPin": 2,          // ← safe default (ESP8266)
    "ledCount": 30,
    "topology": "strip",
    ...
  },
  "sensor-esp32": {        // ← override for ESP32 sensors
    "gpioPin": 4,
    "ledCount": 30,
    "topology": "strip",
    ...
  },
  "motor": { ... }
}
```

`getConfigForDeviceType(type, chipType)` resolves `"sensor-esp32"` first, falls back to `"sensor"`.  
`broadcastLedConfig()` passes `client.chipType` to select the right config before sending.

**Current state:** `getConfigForDeviceType(deviceType)` takes only one argument; `validateDeviceLedCount(deviceType, reportedCount, chipType)` already threads `chipType` through but only for max-count validation. Extending the resolver is a 1-function change plus a JSON schema bump — still a valid Phase 0/1 target.

### 4.7 Sequence Numbers on Events

Add an incrementing `seq` counter to `GameState` and attach it to every broadcast:

```json
{ "type": "scored", "seq": 1042, "payload": { ... } }
```

Clients that reconnect send their last-seen `seq` in the `register` message. Server can log or replay missed events (even if not acting on them). This gives an audit trail and makes stale-event detection trivial.

### 4.8 `test_effect` TTL and Stop Message

Add a `durationMs` field to `test_effect` payload:

```json
{ "type": "test_effect", "payload": { "effectName": "rainbow", "durationMs": 5000 } }
```

`durationMs: 0` = indefinite (current behaviour, still supported).  
Add `{ "type": "stop_effect" }` as a companion message to explicitly end a test.

On firmware, `AnimationManager` already supports `durationMs` in `EffectParams` — this is just wiring the value through from the JSON payload, which is already partially done.

### 4.9 `deviceColor` NVS Persistence (firmware)

After receiving `led_config.deviceColor`, write the hex string to NVS:

```cpp
// After parsing led_config:
if (hasDeviceColor) {
    NVS.putString("deviceColor", deviceColorHex);  // already done for other prefs
    _ledManager.setDeviceColor(parsedColor);
}
// In setup(), before connecting:
String saved = NVS.getString("deviceColor", "");
if (saved.length() == 7) {
    _ledManager.setDeviceColor(parseHex(saved));
}
```

This makes color survive reboots and available immediately before the first `led_config` arrives.

---

## 5. Migration Path (Incremental, Low-Risk)

### Phase 0 — Immediate bug fixes (no architecture change)
- [ ] Fix `_applyMotorColorSync` to use `updateDeviceColor()` instead of `assignColor(chipId, number)` **(P3 — still open)**
- [x] Fix `PLATFORM.motor.defaultTopology` missing in `led-admin.js` **(P9 — fixed in PR#24)**
- [ ] Make `getConfigForDeviceType(deviceType, chipType)` chiptype-aware server-side **(P4 — client resolved, server pending)**
- [x] Fix README.md ESP32 strip pin (GPIO2 → GPIO4) **(fixed in PR#24)**

### Phase 1 — Quick wins (single-file changes)
- [ ] Add `clients/shared/js/gameEvents.js` with string constants
- [ ] Replace hardcoded string literals in `ActionEffect.js`, `SoundManager.js`, `websocket.cpp` string maps
- [ ] Add `durationMs` and `stop_effect` to `test_effect` flow
- [x] Persist `deviceColor` across reboots **(P5 — done via LittleFS `state.json`, both firmware clients)**

### Phase 2 — Event queue (firmware only)
- [ ] Add `EventQueue<T, N>` ring buffer to `clients/shared/leds/GameEventQueue.h`
- [ ] Replace `_pendingLocalEvent` / `_pendingGlobalEvent` with `EventQueue` in both `esp8266-sensor` and `esp32-motor`
- [ ] Drain queue in main loop (process oldest, drop duplicates of same type)

### Phase 3 — Layered effect system (shared firmware)
- [ ] Implement `EffectLayer` class in shared lib
- [ ] Refactor `LedManager` in both firmware projects to use three layers
- [ ] Move ambient (connection-state) effects to layer 0
- [ ] Move game-event effects to layer 1
- [ ] Move admin test effects to layer 2

### Phase 4 — Per-device LED config (server)
- [ ] Add per-`chipId` override storage in `LedConfigManager`
- [ ] Add `GET/PUT/DELETE /api/leds/config/:deviceType/:chipId` REST endpoints
- [ ] Update `broadcastLedConfig` to resolve per-device config
- [ ] Update LED admin page to expose per-device overrides

### Phase 5 — Sequence numbers and event log
- [ ] Add `seq` counter to `GameState`
- [ ] Add `seq` to all WS broadcasts
- [ ] Accept `lastSeq` in `register` for reconnect
- [ ] (Optional) ring-buffer last N events in server for replay

---

## 6. What Should Move to Shared

| Currently | Should be shared | Notes |
|---|---|---|
| `GameEvents.h` in `clients/shared/leds/` | ✅ already shared | Canonical enums used by both firmware clients |
| `GameEventMapper.h` | ✅ already shared | |
| `AnimationManager` | ✅ already shared | |
| All effect classes | ✅ already shared | |
| `device_info.h` (chipId / chipType) | ✅ already shared in `clients/shared/io/` **(PR#24)** | Replaces duplicated `chipIdHex()` helpers |
| `color_utils.h` (`derbyParseHexColor`) | ✅ already shared in `clients/shared/io/` **(PR#24)** | Replaces duplicated hex parsers in sensor/motor |
| `ledPinIsValid(pin)` | ✅ already shared in `clients/shared/leds/LedPlatform.h` **(PR#24)** | Single source of truth for LED GPIO rules |
| `StatusLed`, `ButtonManager` | ✅ already shared in `clients/shared/io/` | |
| `EventQueue<T,N>` | ❌ not yet created | Add to `clients/shared/leds/` |
| `EffectLayer` | ❌ not yet created | Add to `clients/shared/leds/` |
| `GameEvents.js` string constants | ❌ not yet created | Add to `clients/shared/js/` |
| `LedConfigDefaults.h` (chiptype map) | ❌ not yet created | Could centralise defaults; or expose same data as a server-side JSON schema so JS and firmware stay in sync |

---

## 7. What NOT to Change

- **Server-is-authoritative** model — already correct. Do not move game logic to clients.
- **Display client multi-effect fan-out** — GSAP/Pixi do not have the same single-active-effect constraint as the LED firmware. Keep `ActionEffect.js` firing all events in parallel.
- **`scored` broadcast to all clients** — even though only the owning device reacts locally, the display client and sound manager need the full event for all players. Keep the global broadcast.
- **JSON / WebSocket protocol** — adding optional fields (`seq`, `durationMs`) is backward-compatible. Avoid breaking message type renames.

---

## 8. Summary Diagram — Proposed Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          Node.js Server                                  │
│  GameState (seq counter) ──► events[] ──► ConnectionManager              │
│  broadcastScored / broadcastGameEvent / broadcastWinner                  │
│    all carry {type, seq, payload}                                        │
│  LedConfigManager: type defaults + per-chipId overrides                  │
│  SoundManager: event→wav mapping                                         │
└──────────────┬───────────────────┬──────────────────────────────────────┘
               │  WS               │  WS (same broadcast)
   ┌───────────▼─────────┐  ┌──────▼──────────────────────────────────┐
   │ ESP8266/ESP32 Sensor │  │       Display Client (Pixi.js)          │
   │  EventQueue (local)  │  │  ActionEffect.js — multi-event fan-out  │
   │  EventQueue (global) │  │  WinnerOverlay / CountdownEffect        │
   │  EffectLayer[0]=amb  │  │  (no single-active constraint)          │
   │  EffectLayer[1]=game │  └─────────────────────────────────────────┘
   │  EffectLayer[2]=admin│
   │  AnimationManager×3  │
   │  NVS: deviceColor    │
   └─────────────────────┘

   ┌─────────────────────────────────────────┐
   │         ESP32 Motor                     │
   │  Same shared layer as sensor            │
   │  + MatrixDisplay for 8×8 matrix effects │
   │  + positions → stepper motors           │
   └─────────────────────────────────────────┘

   shared C++: GameEvents.h, GameEventMapper.h, AnimationManager,
               EffectLayer (new), EventQueue (new), all effects
   shared JS:  GameEvents.js (new)
```

---

## 9. Architectural Critique & Open Issues

> **Context:** This section critiques the proposals above from a senior-architect perspective, flags hidden assumptions, and records unresolved design questions. Nothing here invalidates the migration plan — it sharpens it.

---

### C1 — Three `AnimationManager` instances share one `LedController`: no real compositing

**Proposal (§4.1/§4.4):** Three `EffectLayer` instances, each wrapping its own `AnimationManager`.

**Problem:** All three `AnimationManager` instances write to the *same* `LedController`, which calls `strip.show()` at the end of every frame. Layer-2 effects will clobber Layer-1 outputs pixel-by-pixel because there is no per-layer pixel buffer or blending stage. The last layer to call `_controller->show()` in a given `loop()` iteration wins.

**Better approach — Option A (minimal cost):** Single `AnimationManager` with a priority enum attached to each `playEffect()` call. A lower-priority request is silently ignored if a higher-priority effect is already active; it only fires when the current effect is complete or has lower priority.

```cpp
// Single-animator priority gate (no extra SRAM)
// Convention: higher numeric value = higher priority (matches LocalEventType enum ordering)
// PRIORITY_AMBIENT=0, PRIORITY_GAME=1, PRIORITY_ADMIN=2
void playEffect(LedEffect* effect, uint8_t priority) {
    if (_activePriority > priority) return; // currently-running effect has higher priority, drop request
    _activePriority = priority;
    _playEffectInternal(effect);
}
void _onEffectComplete() {
    _activePriority = PRIORITY_AMBIENT; // reset when done so any new request is accepted
}
```

**Better approach — Option B (richer but ~3× SRAM for pixel buffers):** Keep three `AnimationManager` instances but introduce a `LedCompositor` that owns the physical strip. Each layer writes to its own `RgbColor[]` buffer; the compositor alpha-blends them on each tick before calling `strip.show()`.

Option A is recommended for Phase 3 on ESP8266 (80 KB SRAM). Option B is viable on ESP32 (520 KB SRAM) if per-layer visual blending is desired.

---

### C2 — Priority replacement in `EventQueue` is underspecified across categories

**Proposal (§4.2):** "On full queue, overwrite the lowest-priority entry."

**Problem:** `LocalEventType` and `GlobalEventType` are separate enums with independent ordinal values. A `LocalEventType::TOOK_LEAD` (value 7) is numerically larger than `GlobalEventType::GAME_PAUSED` (value 4), but they are semantically incommensurable — cross-category comparison will produce nonsense.

**Clarification needed:**
- Local and Global queues must stay separate (as proposed). Priority replacement applies *within* each queue independently.
- Within `LocalEventType`: enum ordering already gives priority (higher value = higher priority). ✅
- Within `GlobalEventType`: `WINNER_SELF > WINNER_OTHER > GAME_STARTED > COUNTDOWN_TICK > GAME_PAUSED > GAME_RESUMED > GAME_RESET` is a reasonable order — add a comment in `GameEvents.h` stating that enum value order encodes priority.
- A `GlobalEventType` in the global queue should never evict a `LocalEventType` in the local queue and vice versa.

---

### C3 — `GameEventMapper` violates the Open/Closed Principle

**Current state:** `onLocalEvent()` and `onGlobalEvent()` are large `switch` statements. Adding a new event type requires modifying the mapper.

**Risk for refactoring:** Any new effect addition risks inadvertent side-effects from touching the same function.

**Proposed alternative — table-driven mapper:**

```cpp
// In GameEventMapper.h — registration table instead of switch
// Pre-allocated effect instances (same as current) are referenced by pointer.
// The table is const and can be stored in flash (PROGMEM) on ESP8266.
struct LocalEffectEntry {
    LocalEventType event;
    LedEffect*     effect;         // pointer to pre-allocated effect instance
    uint16_t       durationMs;
    uint8_t        priority;       // PRIORITY_AMBIENT=0, PRIORITY_GAME=1, PRIORITY_ADMIN=2
};

// Example registration table (replaces the switch in onLocalEvent)
static const LocalEffectEntry LOCAL_EFFECTS[] PROGMEM = {
    { LocalEventType::SCORE_PLUS1, &_blinkEffect,   200, PRIORITY_GAME },
    { LocalEventType::TOOK_LEAD,   &_chaseEffect,  1000, PRIORITY_GAME },
    // ... one entry per LocalEventType value
};

void onLocalEvent(LocalEventType event) {
    for (auto& entry : LOCAL_EFFECTS) {
        if (entry.event == event) {
            _configureEffect(entry);
            _animator->playEffect(entry.effect, entry.priority);
            return;
        }
    }
}
```

This keeps new effects as data rather than code changes. Effect parameters can later be loaded from NVS/flash for runtime customisation without firmware reflash.

---

### C4 — `GameEvents.js` module format cannot serve both server (Node.js) and browser without modification

**Proposal (§4.3):** `var GameEvents = Object.freeze({...})` — a browser global.

**Problem:** `SoundManager.js` runs in Node.js and uses `require()`. The proposed format is not importable via `require()` without wrapping. If the file is served as a static asset and also `require()`'d by the server, there will be two separate copies with no shared mutation surface.

**Fix — dual-format shim:**

```javascript
// clients/shared/js/gameEvents.js
var GameEvents = Object.freeze({
  ZERO_ROLL:       'zero_roll',
  SCORE_1:         'score_1',
  // ...
  GAME_STARTED:    'game_started',
  // ...
});

// CommonJS export for Node.js (server-side SoundManager, tests)
if (typeof module !== 'undefined' && module.exports) {
  module.exports = GameEvents;
}
```

Both `require('./gameEvents')` (Node.js) and `<script src="...gameEvents.js">` (browser) then produce the same frozen object without modification.

---

### C5 — Sequence numbers are counterproductive for transient LED effects on reconnect

**Proposal (§4.7):** Clients send `lastSeq` on reconnect; server "can log or replay missed events."

**Problem:** LED effects are fire-and-forget animations with duration on the order of 200–1500 ms. Replaying a `scored` event that occurred 3 seconds ago after a sensor reconnects will trigger a stale sparkle effect that has no relationship to current game state. This is confusing to the player standing at the sensor.

**Recommendation:**
- Firmware clients: **do not replay** missed events after reconnect. Request the current `state` snapshot instead (already sent on connection by the server). The `seq` counter is still useful for *deduplication* (detect duplicate `scored` messages) and *logging* (server-side diagnostics), but firmware should explicitly discard events with `seq ≤ _lastSeenSeq`.
- Display client and web admin: replay is beneficial (they show scores/history, not time-critical LED pulses). Consider a separate replay flag per client type in the `register` message: `"replayMissed": true`.

---

### C6 — `test_effect` `stop_effect` message should target a specific layer

**Proposal (§4.8):** `{ "type": "stop_effect" }` stops the current test.

**Problem:** If an admin accidentally sends `stop_effect` while a game-triggered `WINNER_SELF` rainbow is running on the admin layer, it could terminate the wrong effect.

**Fix:** Scope the stop message to the admin layer explicitly:

```json
{ "type": "stop_effect", "payload": { "layer": "admin" } }
```

Firmware interprets this as "cancel any active admin-layer transient and return to game/ambient." Without a layer qualifier, `stop_effect` is ambiguous.

---

### C7 — Ambient vs. connection-status conflation in Layer 0

**Proposal (§4.1):** Layer 1 = Ambient (connection status, game pause, idle) — a 3-layer model.

**Problem:** "Connection status" and "game pause" are different concerns. Connection status is a **hardware diagnostic** — it should be visible even when a game is running. "Game pause" is a **game event** that arguably belongs in Layer 2.

**Revised conceptual model (4 layers):**

| Layer | Name | Contents | Survives reconnect? |
|---|---|---|---|
| 0 — Diagnostic | WiFi/WS status (connecting blink, disconnected red pulse) | Always | Yes |
| 1 — Ambient | Idle breathing, game-paused pulse, post-reset state | Game-context-dependent | Yes |
| 2 — Game Event | Scoring, streaks, rank changes, winner, countdown | Transient (duration-bound) | No (drop on reconnect) |
| 3 — Admin | test_effect overrides | Transient (TTL-bound) | No (stop on reconnect) |

**Reconciling with the 3-layer model in §4.1:** The 3-layer model in §4.1 (Ambient / Game / Admin) remains the *firmware implementation target* because adding a 4th layer costs SRAM. The 4-layer model is the *conceptual design* — on ESP8266, Layers 0 and 1 are collapsed into a single ambient `AnimationManager` driven by an explicit state machine (idle → paused → disconnected → ...), while the priority gate handles Layers 2 and 3. On ESP32, all four can be distinct. This is captured in OQ4 at the end of this section.

---

### C8 — No explicit backward-compatibility contract for the WebSocket protocol

**§7 ("What NOT to Change")** implicitly covers this, but there is no formal statement of what is and isn't a breaking change.

**Recommended contract (add to §7):**
- **Non-breaking:** Adding optional fields to existing message payloads (`seq`, `durationMs`, `layer`).
- **Non-breaking:** Adding new `type` values that old clients silently ignore.
- **Breaking:** Renaming existing `type` values (e.g., `scored` → `score_event`).
- **Breaking:** Removing payload fields that existing clients read.
- **Breaking:** Changing numeric encoding of `events[]` strings.

All Phases 1–5 proposals are non-breaking by this definition. ✅

---

### C9 — Lack of firmware testability strategy

The analysis has no mention of how to test the proposed changes before shipping firmware.

**Minimum viable firmware test plan:**
1. **`EventQueue` unit tests** — host-side (no hardware): use PlatformIO's `native` environment to compile and run `tests/test_event_queue.cpp` with `unity`. Test: push/pop ordering, priority eviction, overflow behaviour.
2. **`EffectLayer` / priority gate tests** — same native environment: mock `LedController`, verify that a high-priority `playEffect()` call suppresses a low-priority one.
3. **Integration smoke test** — flash a sensor or motor, observe Serial output via `pio device monitor`, verify that rapid successive `scored` → `game_event` WS messages produce the expected effect sequence without silent drops.
4. **Regression test checklist** (manual, per firmware build):
   - [ ] LED strip lights on connection
   - [ ] `SCORE_PLUS3` effect fires and returns to ambient
   - [ ] `GAME_PAUSED` amber pulse starts and stops on `GAME_RESUMED`
   - [ ] `test_effect` from admin UI runs and stops after TTL
   - [ ] Device color survives reboot (NVS persistence)

---

### C10 — Effect parameter data model (EffectParams) is closed to extension

**Current:** `EffectParams` is a plain struct with fixed fields. Adding a new effect type (e.g., "wave", "fire") that needs custom parameters requires modifying the shared struct.

**Risk:** Every firmware project that includes `LedEffect.h` must be rebuilt when `EffectParams` changes, even if the effect is only used by one project.

**Options (pick one):**
- **Option A — subclass params:** Each `LedEffect` subclass holds its own extra params via additional setters (already done: `BlinkEffect::setBlinkParams()`, `ChaseEffect::setChaseParams()`). ✅ Continue this pattern; don't add new fields to `EffectParams`.
- **Option B — tag/variant field:** Add a `uint8_t tag` + `uint8_t data[8]` union to `EffectParams` for effect-specific extras. Avoids virtual setters but sacrifices type safety.

Option A is already the direction the code is taking. Formalise it: `EffectParams` carries *only* universal properties (color, brightness, durationMs, speed); all effect-specific configuration goes through named setters on the concrete class.

---

### C11 — `isComplete()` / `isDone()` naming inconsistency in the analysis

The analysis doc (§1.4) uses `isDone()` but the actual `LedEffect.h` API uses `isComplete()`. All future documentation and code should use `isComplete()` to match the existing interface.

---

### Open Questions (decision required before Phase 3)

| # | Question | Options | Recommended |
|---|---|---|---|
| OQ1 | Single animator + priority gate vs. multi-layer compositor? | A: priority gate / B: compositor | **A for ESP8266, B for ESP32** |
| OQ2 | Should `GAME_PAUSED` effect live in Layer 1 (ambient) or Layer 2 (game event)? | Layer 1 (persistent until resumed) vs Layer 2 (transient) | **Layer 1** — it must survive other game events |
| OQ3 | Should firmware replay events after reconnect? | Yes (risky) / No — full state sync only | **No — full state sync** |
| OQ4 | Three-layer or four-layer model? | 3 (ambient/game/admin) vs 4 (diagnostic/ambient/game/admin) | **3 for firmware, 4 conceptually** |
| OQ5 | `GameEvents.js` — dual-format shim or two separate files? | Shim / separate `gameEvents.cjs` + `gameEvents.js` | **Shim** — one source of truth |

---

## 10. Codebase Baseline Update — changes since the analysis was written

> **Context:** This document's Sections 1–9 were authored against the `refactor-events` branch head at commit `2c8b9bc` (22 Apr 2026). Between that commit and the current HEAD, PR#24 (`copilot/add-esp32-support-to-client`, squash-merge `f006592`, merge commit `2f22a31`) landed. This section is a one-stop diff-summary so future contributors do not have to re-derive which claims in the analysis still apply.

### 10.1 What PR#24 changed (code)

| Area | Change | Impact on this document |
|---|---|---|
| **Device identity** | `device_info.h` moved from `esp8266-sensor/src/` to new shared library `clients/shared/io/`. Canonical constants: `DERBY_CHIP_ID_HEX_MAX_LEN = 17`, `DERBY_CHIP_ID_ESP8266_HEX_LEN = 9`. Functions: `derbyChipType()`, `derbyChipSuffix16()`, `derbyChipIdHex()`. | §1.4 rewritten to show two-library layout. Future `EventQueue`/`EffectLayer` work may `#include <io/device_info.h>` but must not create a reverse dependency. |
| **Color parsing** | New `clients/shared/io/color_utils.h` exports `derbyParseHexColor("#RRGGBB", r, g, b)`. Replaces duplicated parsers in `esp8266-sensor/src/websocket.cpp` and `esp32-motor/src/websocket.cpp`. | §6 "What Should Move to Shared" table updated. |
| **Pin validation** | `ledPinIsValid(int pin)` is now the canonical helper in `clients/shared/leds/LedPlatform.h`. ESP8266 allows `{2, 3}`; ESP32 allows `0 ≤ pin ≤ LED_GPIO_MAX (39)`. `LED_GPIO_MAX` is deliberately undefined on ESP8266 — there is no numeric upper bound for that platform. | §1.4 now mandates callers use this helper. §4.6 chiptype-aware config must call it for validation. |
| **Register payload split** | Motor `register` payload split `capabilities` into `ledCapabilities` (LED pin/GPIO/topology) + `deviceCapabilities` (buttons, matrix size, stepper count). Sensor sends only `ledCapabilities`. | §1.3 event-taxonomy table updated. Any refactor that adds new capability flags must pick the correct sibling object. |
| **ESP32 sensor target** | `clients/esp8266-sensor/platformio.ini` now also builds under `[env:esp32dev]` against the ESP32 RMT driver. Same shared `leds/`+`io/` libs. | §1.2 actor table already reads "ESP8266/ESP32 sensor" — no further change. The refactor design must assume dual-platform sensors going forward, not ESP8266-only. |
| **deviceColor persistence** | Sensor + motor `main.cpp` both persist `device_color_r/g/b` + `hasDeviceColor` via LittleFS `state.json` on every `led_config` receipt and re-load in `setup()`. | P5 marked RESOLVED. |
| **led-admin.js chip table** | `CHIP.ESP8266` and `CHIP.ESP32` now declare `defaultPin`, `defaultTopology`, `maxLeds`, `pinGroup`. Client-side pin validator rejects `ESP8266.gpioPin ∉ {2, 3}` and `ESP32.gpioPin ∉ [0, 39]` before PUT. | P9 marked RESOLVED. §4.6 chiptype defaults now partially implemented (client only). |
| **Server `validateDeviceLedCount`** | Now takes `chipType` and warns if `reportedCount` exceeds the platform ceiling (300 for ESP8266, 1000 for ESP32). | Informational — doesn't change the refactor design, but confirms `chipType` is already threaded as far as `ConnectionManager` → `LedConfigManager`; extending `getConfigForDeviceType` to take it is a mechanical change. |

### 10.2 Problems carried forward (still open)

- **P1** single-slot event buffer in firmware — unchanged
- **P2** global events unconditionally cancel local effects — unchanged
- **P3** `_applyMotorColorSync` type-mismatch on `assignColor` — unchanged (see revised §2 P3)
- **P4** server-side per-device-type LED config — client partially addressed, server unchanged
- **P6** `test_effect` has no TTL / stop message — unchanged
- **P7** event string names duplicated across layers — unchanged
- **P8** no event versioning or sequence numbers — unchanged

### 10.3 Senior-architect notes on future-proofing the refactor

The PR#24 landing clarified two architectural boundaries that the refactor design must respect:

1. **`clients/shared/io/` is upstream of `clients/shared/leds/`.** Device identity, IO primitives, and color parsing are general-purpose and have no dependency on the LED effect engine. The reverse is not true — `leds/GameEventMapper` and any future `leds/EventQueue` legitimately consume `io/device_info.h` to tag events with a `chipId`. Do not collapse the two libraries, and do not introduce cycles.
2. **`ledCapabilities` ≠ `deviceCapabilities`.** Any new shared schema (e.g., `clients/shared/js/gameEvents.js` from §4.3, or a `LedConfigDefaults.h` from §6) must preserve this split. When the server broadcasts an `led_config`, the consumer has a purely-LED contract; when it sends a motor-specific command (pause button scan, stepper calibrate), it uses the separate device-capability channel.

A third boundary is implicit and should be documented here while it is fresh:

3. **LittleFS `state.json` is the firmware's canonical persistence format.** P5's resolution establishes the pattern: a single JSON file holding `wifi`, `server`, and `led_config` subtrees. Any future event-system additions that need survivability (e.g., last-seen `seq` from §4.7, per-device ambient-effect override) should extend that file, not introduce parallel NVS key-value stores. This keeps firmware boot-path simple and makes factory-reset a single `LittleFS.remove(STATE_FILE)` call.

### 10.4 Document conventions

- **Section numbering is stable.** §1–§9 retain their original numbering and intent; §10 is the living update log appended per baseline refresh.
- **Problem IDs (P1…P9) are immutable.** When a problem is resolved, annotate in §2; do not renumber. This preserves cross-references in commit messages, PR descriptions, and `plan/` entries.
- **Critique IDs (C1…C11) and Open Questions (OQ1…OQ5) are likewise immutable.**

---

*Last updated: 2026-04-24 (baseline); §10 added post-PR#24 (`2f22a31`).*  
*Authors: @copilot (analysis, critique, baseline update), @tamaygz (codebase).*
