# Isolated Tasks — Event / Effect Refactor Pre-Work

> **Purpose:** Before the main event/effect refactor (Phases 1–5) begins, these self-contained
> tasks should each be executed as separate, focused PRs. Keeping them isolated limits blast
> radius, makes reviews easier, and allows parallel progress.
>
> Each task below has enough context to be handed to an agent or worked independently.

---

## T1 — Fix pre-existing server test failures (3 failures)

**Status:** ✅ **Done** — all 161 server tests pass as of 2026-04-24. The 3 pre-existing failures were resolved by prior PRs before this task was executed.  
**Scope:** Server only (`server/tests/`)  
**Risk:** Low — isolated test fixes, no production code change expected

~~**What:** As of 2026-04-24 `npm test` shows 3 pre-existing failures unrelated to the refactor.
Run `cd server && npm test`, identify the 3 failing tests, fix the root cause (or the test if it is incorrectly written), and verify `npm test` reaches 106/106 pass.~~

**Acceptance:** `npm test` output shows 0 failures before any Phase 0 work begins. ✅

---

## T2 — Fix Phase 0 bugs (two remaining items)

**Status:** ✅ **Done** — both items resolved by PR#24 (`copilot/add-esp32-support-to-client`, merged into `refactor-events`).  
**Scope:** `clients/web/js/led-admin.js`, `server/data/led-config.json`, `clients/esp8266-sensor/README.md`  
**Risk:** Low — small targeted fixes

~~**What:**~~
1. ~~**P9 — `PLATFORM.motor.defaultTopology` missing in `led-admin.js`**~~ → **Resolved:** `led-admin.js` now declares `defaultTopology: 'strip'` on every `CHIP` entry.
2. ~~**P9 / README — Sensor GPIO pin**~~ → **Resolved:** README corrected in PR#24.

**Acceptance:** Both ✅ verified.

---

## T3 — Add `clients/shared/js/gameEvents.js` (Phase 1 single-file quick win)

**Status:** ✅ **Done** — implemented in this PR.  
**Scope:** `clients/shared/js/` (new file), `clients/display/js/effects/ActionEffect.js`, `clients/display/js/main.js`, `server/src/ws/ConnectionManager.js`, `server/src/sound/SoundManager.js`, `server/tests/soundManager.test.js`

**What was done:**
1. Created `clients/shared/js/gameEvents.js` — dual-format shim (browser `<script>` + Node.js `require()`).
2. Added `/shared` static route to `server/src/index.js` so both display and web admin clients can load it.
3. Added `<script src="/shared/js/gameEvents.js">` to `clients/display/index.html`.
4. Replaced all raw event string literals in `ActionEffect.js` and `main.js` with `GameEvents.*` constants.
5. `SoundManager.js`: switched all `EVENT_FILE_MAP` keys to use `GameEvents.*` constants; added startup validation that warns if a `GameEvents` value has no sound mapping.
6. **Fixed bug in `ConnectionManager.js`**: `PRIORITY_EVENTS` used `'streak_three'`/`'streak_zero'` (wrong — these never matched `events[]`). Now uses `GameEvents.STREAK_THREE_2X`/`GameEvents.STREAK_ZERO_3X`. Streak sounds now play correctly.
7. Aligned zero-roll sound lookup: `zero_roll` game event → `score_0.wav`. All tests updated.

**Acceptance:** `npm test` → 168/168 pass ✅

---

## T4 — Chiptype-aware LED config defaults (Phase 0 / P4 complement)

**Status:** ✅ **Done** — implemented in this PR.  
**Scope:** `server/src/config/LedConfigManager.js`, `server/data/led-config.json`, `server/src/ws/ConnectionManager.js`, `server/tests/ledConfigManager.test.js`

**What was done:**
1. Added `sensor-esp32` entry to `led-config.json` (gpioPin: 4) and `sensor` corrected to gpioPin: 2.
2. Added `sensor-esp32` to `LedConfigManager.defaultConfig`.
3. Updated `getConfigForDeviceType(deviceType, chipType = null)` to check `"${type}-${chipType.toLowerCase()}"` before falling back to `"${type}"`.
4. Updated `getConfigForDevice(deviceType, chipId, chipType = null)` to thread `chipType` through to the type resolver.
5. Updated `ConnectionManager.js` at all three LED config dispatch points to pass `client.chipType`.
6. Added 7 new tests covering chiptype resolution, case-normalisation, ESP32 vs ESP8266 defaults, and per-device override precedence.

**Acceptance:** ✅ `npm test` → 168/168 pass. ESP32 sensors receive `gpioPin: 4`; ESP8266 sensors receive `gpioPin: 2`.

---

## T5 — `EventQueue<T, N>` shared C++ template (Phase 2 pre-work)

**Status:** ✅ **Done** — implemented in this PR.  
**Scope:** `clients/shared/leds/EventQueue.h` (new), `clients/esp8266-sensor/platformio.ini`, `clients/esp8266-sensor/test/test_event_queue/`  
**Risk:** Low — pure header, no hardware dependency

**What was done:**
1. Created `clients/shared/leds/EventQueue.h` — bounded FIFO ring buffer template with priority-based overflow eviction. On overflow, the lowest enum-value (lowest-priority) entry is replaced. Ties broken by FIFO age (oldest evicted first).
2. Added `[env:native_test]` environment to `clients/esp8266-sensor/platformio.ini`.
3. Created four native Unity tests in `clients/esp8266-sensor/test/test_event_queue/test_event_queue.cpp`:
   - `test_push_pop_fifo` — basic FIFO ordering for equal-priority items
   - `test_priority_eviction` — `TOOK_LEAD` survives 3× `SCORE_PLUS1` overflow
   - `test_empty_pop_returns_false` — pop on empty returns false, does not crash
   - `test_full_no_crash` — repeated overflow does not corrupt memory

**Acceptance:** `pio test -e native_test` passes all four tests. No ESP hardware required.

---

## T6 — Priority gate in `AnimationManager` (Phase 3 pre-work)

**Status:** ✅ **Done** — implemented in this PR.  
**Scope:** `clients/shared/leds/AnimationManager.h/.cpp`, `clients/shared/leds/LedPlatform.h`, `clients/esp8266-sensor/test/`  
**Risk:** Medium — touches core effect-dispatch path

**What was done:**
1. Added priority constants to `AnimationManager` (public, `constexpr`):
   - `PRIORITY_AMBIENT = 0` — idle / ambient effects
   - `PRIORITY_GAME    = 1` — normal game event effects (default for existing call sites)
   - `PRIORITY_ADMIN   = 2` — admin test / override effects
2. Updated `playEffect(LedEffect*, uint8_t priority = PRIORITY_GAME)` — default keeps all existing call sites unchanged.
3. Priority gate logic: if `_currentEffect != nullptr && priority < _activePriority`, the request is dropped silently.
4. `_activePriority` resets to `PRIORITY_AMBIENT` when an effect completes (in `loop()`) or is stopped (`stop()`).
5. Added `NATIVE_TEST` branch to `LedPlatform.h` so that `AnimationManager` and `LedController` compile on the host without ESP hardware or the Arduino SDK.
6. Created mock headers in `clients/esp8266-sensor/test/mocks/` (`Arduino.h`, `NeoPixelBus.h`, `NeoPixelBusLg.h`).
7. Created two native Unity tests in `test/test_animation_manager/test_animation_manager.cpp`:
   - `test_low_priority_dropped_while_high_active`
   - `test_priority_resets_after_effect_completes`

**Acceptance:**
- `pio test -e native_test` — both AnimationManager tests pass, all 4 EventQueue tests pass.
- Existing call sites that omit the `priority` argument are unaffected (default = `PRIORITY_GAME`).
- Hardware smoke tests H1–H9 pass after reflash (unchanged effect lifecycle — only gate logic added).

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

## T7 — Firmware `websocket.cpp` event string cleanup (Phase 1 remainder)

**Status:** 🔲 To do  
**Scope:** `clients/esp8266-sensor/src/websocket.cpp`, `clients/esp32-motor/src/websocket.cpp`  
**Risk:** Low — rename-only within firmware, no architecture change  
**Prerequisite:** T3 ✅

**What:** The `strcmp`-based string-to-enum mapping in `websocket.cpp` uses raw string literals (e.g. `"took_lead"`, `"game_started"`) that duplicate the values in `GameEvents.h`. These should be verified against the canonical `GameEvents.h` values and updated if any drift is found. No new code patterns — just alignment check and any corrections.

**Steps:**
1. `grep -n 'strcmp' clients/esp8266-sensor/src/websocket.cpp` to list all string comparisons.
2. Cross-reference each string against `GameEvents.h` enum comments/values.
3. Correct any that have drifted. Add a comment block above the mapping section that cites the canonical source (`GameEvents.h`).
4. Repeat for `esp32-motor`.

**Acceptance:** `grep 'strcmp.*took_lead\|score_3\|game_started' clients/*/src/websocket.cpp` — all values match the string constants in `clients/shared/js/gameEvents.js` and the comments in `GameEvents.h`. No new functionality introduced.

---

## T8 — Integrate `EventQueue<T,N>` into firmware (Phase 2 completion)

**Status:** 🔲 To do  
**Scope:** `clients/esp8266-sensor/src/websocket.h/.cpp`, `clients/esp32-motor/src/websocket.h/.cpp`  
**Risk:** Medium — changes the core event-dispatch path  
**Prerequisite:** T5 ✅, T7

**What:** Replace the single `_pendingLocalEvent` / `_pendingGlobalEvent` slots with `EventQueue<LocalEventType, 4>` / `EventQueue<GlobalEventType, 4>`. Update `pollLocalEvent()` / `pollGlobalEvent()` to drain the queue in main `loop()`. Process one event per loop tick to keep frame timing tight.

**Steps:**
1. Add `#include "EventQueue.h"` to `websocket.h`.
2. Replace `LocalEventType _pendingLocalEvent` with `EventQueue<LocalEventType, 4> _localQueue`.
3. Replace all `_pendingLocalEvent = X` with `_localQueue.push(X)`.
4. Update `pollLocalEvent()` to `_localQueue.pop(ev)`.
5. Repeat for Global queue.
6. Verify `pio test -e native_test` still passes (no changes needed there — queue behaviour is already tested).
7. Flash both firmware projects. Run smoke tests H1–H9.

**Acceptance:**
- `pio test -e native_test` → all 6 tests pass (4 EventQueue + 2 AnimationManager).
- Rapid-fire test: 3 consecutive `scored` messages within 100 ms → no events silently lost.
- `TOOK_LEAD` arriving simultaneously with 3× `SCORE_PLUS1` → `TOOK_LEAD` effect fires.

---

## T9 — Wire priority constants into firmware call sites (Phase 3 integration)

**Status:** 🔲 To do  
**Scope:** `clients/esp8266-sensor/src/led.cpp`, `clients/esp32-motor/src/led.cpp`, `clients/shared/leds/GameEventMapper.h`  
**Risk:** High — touches every effect dispatch path  
**Prerequisite:** T6 ✅, T8

**What:** All existing `_animator->playEffect(&effect)` calls use the default `PRIORITY_GAME`. Three call sites need explicit priority:
1. **Ambient effects** (`restoreAmbient()`, connection-status effects) → `PRIORITY_AMBIENT` (0)
2. **Game event effects** (scoring, streaks, lifecycle) → `PRIORITY_GAME` (1) — already default, verify
3. **Admin `test_effect`** → `PRIORITY_ADMIN` (2)

Also: refactor `GameEventMapper::onLocalEvent()` and `onGlobalEvent()` switch statements to the registration-table pattern (C3 / OCP fix).

**Steps:**
1. In both firmware projects, find all `playEffect()` calls and annotate them with the correct priority constant.
2. Change `restoreAmbient()` to call `playEffect(&_ambientEffect, AnimationManager::PRIORITY_AMBIENT)`.
3. Change `test_effect` handling to call `playEffect(&_testEffect, AnimationManager::PRIORITY_ADMIN)`.
4. In `GameEventMapper.h`, replace the `switch (event)` blocks with `LOCAL_EFFECTS[]` and `GLOBAL_EFFECTS[]` registration tables (each entry: `{event, &effect, durationMs, priority}`). See §9/C3 in the analysis doc for the proposed struct definition.
5. Flash and run smoke tests H1–H9, paying special attention to H4 (admin test_effect), H5 (stop_effect), H8 (game_started while scoring is in progress).

**Acceptance:**
- `pio test -e native_test` → all 6 tests pass.
- `GAME_PAUSED` amber pulse persists through a `SCORE_PLUS1` effect.
- `test_effect` (admin) overrides any game effect; game/ambient resumes after stop.
- Adding a new `LocalEventType` requires only a new row in the table — no switch modification.

---

## T10 — LED admin per-device override UI (Phase 4 completion)

**Status:** 🔲 To do  
**Scope:** `clients/web/js/led-admin.js`, `clients/web/led-admin.html`  
**Risk:** Low — additive UI, server REST endpoints already exist  
**Prerequisite:** T4 ✅

**What:** The server exposes `GET/PUT/DELETE /api/leds/config/:deviceType/:chipId` (per-device override). The admin UI currently only shows the type-wide config. Add a per-device section that lists connected devices, shows their effective config (override or type default), and allows setting/clearing overrides.

**Acceptance:**
- Admin page shows a per-device config row for each connected `sensor` and `motor` client (chipId, chipType, effective pin/count/topology).
- `PUT /api/leds/config/sensor/<chipId>` sends and the device receives `led_config` immediately.
- `DELETE /api/leds/config/sensor/<chipId>` falls back to type default and sends `led_config`.

---

## T11 — Sequence numbers (Phase 5)

**Status:** 🔲 To do  
**Scope:** `server/src/game/GameState.js`, `server/src/ws/ConnectionManager.js`, `clients/display/js/main.js`  
**Risk:** Low — additive optional field  
**Prerequisite:** None (independent)

**What:**
1. Add `_seq = 0` counter to `GameState`; increment on every state-changing event.
2. Attach `seq` to all WS broadcasts (`scored`, `game_event`, `winner`, `countdown`).
3. Accept `lastSeq` in `register` message; server logs or skips re-sending known events.
4. Display client: if `scored.seq <= _lastSeenSeq`, skip rendering (deduplication).

**Acceptance:**
- `npm test` → 168/168 pass (add 2 new tests: monotonic counter, dedup on replay).
- Every `scored`/`game_event`/`winner`/`countdown` carries `"seq": <number>`.
- Display client re-registration with same `lastSeq` does not trigger duplicate effects.

---

*Last updated: 2026-04-24 (T4–T6 completed; T7–T11 added as next-phase tasks)*  
*Authors: @copilot, @tamaygz*
