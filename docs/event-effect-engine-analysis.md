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
| `register` | sensor/motor | `chipId`, `chipType`, `ledCount`, `ledCapabilities` |
| `score` | sensor | `playerId`, `points` |
| `button` | motor | `buttonIndex`, `action` (`start`/`reset`/`pause`/`resume`) |

---

### 1.4 Firmware: The Canonical Shared Layer (`clients/shared/leds/`)

The shared C++ library is already the best-designed part of the system:

```
GameEvents.h          — canonical enum definitions (LocalEventType, GlobalEventType, LedTestEffectMessage)
GameEventMapper.h     — maps enums → AnimationManager.playEffect()
AnimationManager.h/cpp — non-blocking loop, FPS control, crossfade transitions
LedEffect.h           — pure-virtual base (begin, update, isDone)
LedController.h/cpp   — platform abstraction (UART1/DMA for ESP8266, RMT for ESP32)
LedPlatform.h         — compile-time type aliases per platform
effects/              — SolidEffect, BlinkEffect, PulseEffect, RainbowEffect, ChaseEffect, SparkleEffect
```

**Key insight:** `LocalEventType` enum values are ordered by priority — so firmware just picks `max(candidates)`.

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
This is an existing bug (detected by the code reviewer). At runtime the second call to `assignColor` will throw because `assignColor` now expects a `Set<number>` as its second parameter but receives a raw number.

### P4 — LED config is per-device-type, not per-device
`PUT /api/leds/config/:deviceType` broadcasts to **all** connected devices of that type. An admin updating one sensor's LED count sends the new config to every sensor. There is no per-`chipId` override path in the REST API.

### P5 — `deviceColor` not persisted on firmware
The assigned color is sent via `led_config.deviceColor` after registration but never written to NVS. On reboot, LEDs revert to white until the server reconnects and sends the next `led_config`. During that window, device-local scoring effects show the wrong (white) color.

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
`PLATFORM` map was refactored to point at `CHIP[...]` objects, but those don't include `defaultTopology`. Motor devices silently fall back to `'strip'` topology if the server config hasn't loaded yet.

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
- [ ] Fix `_applyMotorColorSync` to use `updateDeviceColor()` instead of `assignColor(chipId, number)` **(P3)**
- [ ] Fix `PLATFORM.motor.defaultTopology` missing in `led-admin.js` **(P9)**
- [ ] Revert `sensor.gpioPin` default to `2` in `led-config.json`; add chiptype-aware config lookup **(P4/comment #3134833016)**
- [ ] Fix README.md ESP32 strip pin (GPIO2 → GPIO4) **(comment #3134833090)**

### Phase 1 — Quick wins (single-file changes)
- [ ] Add `clients/shared/js/gameEvents.js` with string constants
- [ ] Replace hardcoded string literals in `ActionEffect.js`, `SoundManager.js`, `websocket.cpp` string maps
- [ ] Add `durationMs` and `stop_effect` to `test_effect` flow
- [ ] Persist `deviceColor` to NVS in firmware

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
| `GameEvents.h` in `clients/shared/leds/` | ✅ already shared | — |
| `GameEventMapper.h` | ✅ already shared | — |
| `AnimationManager` | ✅ already shared | — |
| All effect classes | ✅ already shared | — |
| `EventQueue<T,N>` | ❌ not yet created | Add to `clients/shared/leds/` |
| `EffectLayer` | ❌ not yet created | Add to `clients/shared/leds/` |
| `GameEvents.js` string constants | ❌ not yet created | Add to `clients/shared/js/` |
| `LedConfigDefaults.h` (chiptype map) | ❌ not yet created | Could centralise defaults |

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

*Last updated: 2026-04-24*  
*Authors: @copilot (analysis), @tamaygz (codebase)*
