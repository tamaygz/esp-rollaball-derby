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

*Last updated: 2026-04-24 (T4–T6 completed)*  
*Authors: @copilot, @tamaygz*
