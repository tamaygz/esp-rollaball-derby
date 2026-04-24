# Isolated Tasks — Event / Effect Refactor Pre-Work

> **Purpose:** Before the main event/effect refactor (Phases 1–5) begins, these self-contained
> tasks should each be executed as separate, focused PRs. Keeping them isolated limits blast
> radius, makes reviews easier, and allows parallel progress.
>
> Each task below has enough context to be handed to an agent or worked independently.

---

## T1 — Fix pre-existing server test failures (3 failures)

**Status:** 🔴 Blocked — must merge before any refactor phase begins  
**Scope:** Server only (`server/tests/`)  
**Risk:** Low — isolated test fixes, no production code change expected

**What:** As of 2026-04-24 `npm test` shows 3 pre-existing failures unrelated to the refactor.
Run `cd server && npm test`, identify the 3 failing tests, fix the root cause (or the test if it is incorrectly written), and verify `npm test` reaches 106/106 pass.

**Acceptance:** `npm test` output shows 0 failures before any Phase 0 work begins.

---

## T2 — Fix Phase 0 bugs (two remaining items)

**Status:** 🟡 Ready — can be done in one PR  
**Scope:** `clients/web/js/led-admin.js`, `server/data/led-config.json`, `clients/esp8266-sensor/README.md`  
**Risk:** Low — small targeted fixes

**What:**
1. **P9 — `PLATFORM.motor.defaultTopology` missing in `led-admin.js`:** The `PLATFORM` map points to `CHIP[...]` objects that lack `defaultTopology`. Motor devices silently fall back to `'strip'`. Fix by adding `defaultTopology` to each `CHIP` entry or resolving it at use-site.
2. **P9 / README — Sensor GPIO pin:** `clients/esp8266-sensor/README.md` still documents the wrong GPIO pin for the LED data line. Update to match the actual firmware default.

**Acceptance:**
- `led-admin.js` loads motor topology without console errors.
- README reflects the correct default GPIO.
- `npm test` passes (same count as baseline).

---

## T3 — Add `clients/shared/js/gameEvents.js` (Phase 1 single-file quick win)

**Status:** 🟡 Ready — small, self-contained  
**Scope:** `clients/shared/js/` (new file), `clients/display/js/effects/ActionEffect.js`, `server/src/SoundManager.js`  
**Risk:** Low

**What:** Create `clients/shared/js/gameEvents.js` with all event string constants as a dual-format shim (works with both `<script src>` and `require()`):

```javascript
// clients/shared/js/gameEvents.js
var GameEvents = Object.freeze({
  ZERO_ROLL:       'zero_roll',
  SCORE_1:         'score_1',
  SCORE_2:         'score_2',
  SCORE_3:         'score_3',
  STREAK_ZERO_3X:  'streak_zero_3x',
  STREAK_THREE_2X: 'streak_three_2x',
  TOOK_LEAD:       'took_lead',
  BECAME_LAST:     'became_last',
  GAME_STARTED:    'game_started',
  GAME_PAUSED:     'game_paused',
  GAME_RESUMED:    'game_resumed',
  GAME_RESET:      'game_reset',
  COUNTDOWN_TICK:  'countdown_tick',
  WINNER:          'winner',
});

if (typeof module !== 'undefined' && module.exports) {
  module.exports = GameEvents;
}
```

Then replace all hardcoded string literals in `ActionEffect.js` and `SoundManager.js` with `GameEvents.*` constants. Add a `console.warn` in `SoundManager.js` startup for any `EVENT_FILE_MAP` key not found in `GameEvents`.

**Acceptance:**
- No raw event-name string literals remain in `ActionEffect.js` or `SoundManager.js`.
- `npm test` passes.
- File is served as static asset by the server (add to static file path if needed).

---

## T4 — Chiptype-aware LED config defaults (Phase 0 / P4 complement)

**Status:** 🟡 Ready  
**Scope:** `server/src/LedConfigManager.js`, `server/data/led-config.json`, server tests  
**Risk:** Low — additive server change; per-chipId override path is already merged

**What:** Add chiptype-aware config resolution so ESP32 sensors get `gpioPin: 4` by default
and ESP8266 sensors get `gpioPin: 2`:

```jsonc
{
  "sensor": { "gpioPin": 2, ... },
  "sensor-esp32": { "gpioPin": 4, ... },
  "motor": { ... }
}
```

`getConfigForDevice(type, chipType)` checks `"${type}-${chipType}"` before falling back to `"${type}"`.

**Acceptance:**
- Registering an ESP32 sensor receives `gpioPin: 4` in `led_config`.
- Registering an ESP8266 sensor receives `gpioPin: 2`.
- Type-wide `PUT /api/leds/config/sensor` still works.
- `npm test` passes (new test for chiptype resolution added).

---

## T5 — `EventQueue<T, N>` shared C++ template (Phase 2 pre-work)

**Status:** ⏳ Blocked on T1 + T2  
**Scope:** `clients/shared/leds/EventQueue.h` (new), PlatformIO native unit test  
**Risk:** Low — pure header, no hardware dependency

**What:** Add `clients/shared/leds/EventQueue.h` — a bounded FIFO ring buffer for firmware events. On overflow, replace the lowest-priority (lowest enum value) entry rather than the oldest:

```cpp
template<typename T, uint8_t N>
class EventQueue {
  T       _buf[N];
  uint8_t _head = 0, _tail = 0, _count = 0;
public:
  bool push(T ev);       // overwrites lowest-priority entry on full
  bool pop(T& out);      // FIFO pop into out; returns false if empty
  bool isEmpty() const;
  void clear();
};
```

Add a `[env:native_test]` environment to `clients/esp8266-sensor/platformio.ini` and four unit tests:
- `test_push_pop_fifo` — basic FIFO for equal-priority items
- `test_priority_eviction` — `TOOK_LEAD` survives 3× `SCORE_PLUS1` overflow
- `test_empty_pop_returns_none` — pop on empty does not crash
- `test_full_no_crash` — overflow does not corrupt memory

**Acceptance:** `pio test -e native_test` passes all four tests. No ESP hardware required.

---

## T6 — Priority gate in `AnimationManager` (Phase 3 pre-work)

**Status:** ⏳ Blocked on T5  
**Scope:** `clients/shared/leds/AnimationManager.h/.cpp`  
**Risk:** Medium — touches core effect-dispatch path

**What:** Extend `AnimationManager::playEffect()` to accept a `uint8_t priority` parameter.
Requests with lower priority than the currently active effect are dropped silently.
On `_onEffectComplete()`, reset priority to `PRIORITY_AMBIENT` so any new request fires.

```cpp
// Priority constants (PROGMEM-safe, match Layer 0/1/2 model)
static constexpr uint8_t PRIORITY_AMBIENT = 0;
static constexpr uint8_t PRIORITY_GAME    = 1;
static constexpr uint8_t PRIORITY_ADMIN   = 2;

void playEffect(LedEffect* effect, uint8_t priority = PRIORITY_GAME);
```

Existing call sites that do not pass a priority argument get `PRIORITY_GAME` by default (no breaking change to call sites).

**Acceptance:**
- PlatformIO native test: `playEffect(low)` while high-priority effect is active → request dropped.
- PlatformIO native test: high-priority effect completes → `_activePriority` resets, next request fires.
- Hardware smoke tests H1–H9 pass after reflash.

---

## Cross-project: Generalization for esp-buzzwire

Once T5 and T6 are merged, `EventQueue.h` and the updated `AnimationManager` (priority gate) can be dropped into `tamaygz/esp-buzzwire` without modification. The buzzwire game loop follows the same pattern:

| Derby concern | Buzzwire equivalent |
|---|---|
| `GameEvents.h` — event enums | `StateEvent` enum in buzzwire (WIRE_TOUCH, FINISH, etc.) |
| `EventQueue<T,N>` — event buffer | Same — prevents double-fire on debounce |
| `AnimationManager` priority gate | Same — admin test overrides game effects |
| `GameEventMapper` registration table | `LedEventMapper` table driven by game state |

Track this as a separate PR in the buzzwire repo once the shared lib is stable.

---

*Last updated: 2026-04-24*  
*Authors: @copilot, @tamaygz*
