# Refactor Safety Guide — Event / Effect Engine

> **Purpose:** Companion to [event-effect-engine-analysis.md](./event-effect-engine-analysis.md).
> Covers everything needed to execute the refactor *without* breaking existing functionality:
> backward-compatibility contracts, regression tests, rollback plans, and per-phase acceptance criteria.

---

## 1. Backward-Compatibility Contract

### 1.1 WebSocket Protocol

The server–client WebSocket protocol is the primary integration boundary. All clients (firmware, display, web admin) parse incoming JSON independently. Breaking this contract breaks the whole system.

| Change type | Classification | Examples |
|---|---|---|
| Add **optional** field to existing payload | ✅ Non-breaking | `+seq`, `+durationMs`, `+layer` |
| Add new `type` value that old clients ignore | ✅ Non-breaking | New `stop_effect`, `ack` messages |
| Remove or rename a `type` value | ❌ **Breaking** | `scored` → `score_event` |
| Remove a payload field clients already read | ❌ **Breaking** | Removing `events[]` from `scored` |
| Change the string values inside `events[]` | ❌ **Breaking** | `took_lead` → `lead_taken` |
| Change numeric encoding to binary | ❌ **Breaking** | — |

**Rule:** Every field added in Phases 1–5 must be optional with a safe default. Existing clients that do not read the new field must continue to work as before.

---

### 1.2 REST API

| Endpoint | Current clients | Contract |
|---|---|---|
| `PUT /api/leds/config/:deviceType` | Web admin LED page | **Keep as-is.** New per-chipId endpoints are additive. |
| `POST /api/game/start` etc. | Web admin | No change planned. |
| `GET /api/health` | Monitoring | No change planned. |

---

### 1.3 Firmware Shared Library (`clients/shared/leds/`)

| Symbol | Contract |
|---|---|
| `LedEffect::begin()`, `update()`, `isComplete()`, `reset()` | **Public API — do not rename or change signatures.** |
| `EffectParams` struct fields | **Closed to removal.** New fields may be added with default values. |
| `GameEvents.h` enum names and values | **Closed to renaming.** New values may be added at the end of each enum. |
| `GameEventMapper::onLocalEvent()` / `onGlobalEvent()` | Public API — signature fixed. Internal implementation may change. |
| `AnimationManager::playEffect()`, `transitionTo()`, `stop()` | **Public API — do not rename or change signatures.** |

---

## 2. Pre-Refactor Baseline (capture before any change)

Before executing any migration phase, record the current behaviour as a test baseline. This creates an objective "before" against which regressions can be measured.

### 2.1 Server Test Baseline

```bash
cd server
npm test
```

All existing tests must pass. Record the count:

```
# Expected (as of 2026-04-24): 103 pass, 3 fail (pre-existing), 106 total
```

The 3 pre-existing failures are **not your responsibility** to fix before the refactor, but must be documented so they are not confused with regressions you introduced. Record the exact count before starting each phase.

### 2.2 Manual Hardware Smoke Test (per firmware project)

Run this checklist on a real device before every firmware change, and again after:

| # | Action | Expected result |
|---|---|---|
| H1 | Power on sensor with no WiFi | Blue slow-blink (connecting) |
| H2 | Connect to WiFi + server | LED goes solid device-color or ambient pulse |
| H3 | Score a ball (IR sensor fires) | Blink / sparkle / chase fires, then returns to ambient |
| H4 | Admin sends `test_effect` (rainbow) | Rainbow starts on device |
| H5 | Admin sends `stop_effect` | Rainbow stops, returns to ambient |
| H6 | Admin changes `deviceColor` | Color updates live |
| H7 | Reboot device | Ambient LED comes on before WiFi connects (from NVS color) |
| H8 | Server sends `game_started` | Green chase fires across all devices simultaneously |
| H9 | Server sends `winner` | Rainbow (self) or red pulse (others) until `game_reset` |

### 2.3 Display Client Visual Smoke Test

| # | Action | Expected result |
|---|---|---|
| D1 | Score +3 for any player | Gold bounce, ⭐ "+3!" popup, returns to normal |
| D2 | Trigger `took_lead` | 👑 "LEAD!" popup with gold aura |
| D3 | Trigger `streak_three_2x` | 🔥 "HOT!" popup, delayed 200 ms after score |
| D4 | Game start / pause / reset | Appropriate overlay / effects |
| D5 | Disconnect display and reconnect mid-game | State re-hydrates, scores correct |

---

## 3. Phase-by-Phase Acceptance Criteria and Rollback Plans

### Phase 0 — Immediate bug fixes

**Scope:** Fix `_applyMotorColorSync` (P3), `PLATFORM.motor.defaultTopology` (P9), `sensor.gpioPin` default and chiptype-aware config (P4), README GPIO pin (P9).  
**Risk:** Low — these are pure bug fixes with no architectural change.

**Acceptance criteria:**
- [ ] `npm test` passes (same count as baseline).
- [ ] `led-admin.js` loads motor topology without console errors.
- [ ] Sending `led_config` to an ESP32 sensor uses `gpioPin: 4`; ESP8266 sensor uses `gpioPin: 2`.
- [ ] Manually run smoke tests H1–H8, all pass.

**Rollback:**
```bash
git revert <phase-0-commit-sha>
cd server && npm start   # verify server starts
```
No firmware reflash required unless the GPIO pin change was shipped to devices.

---

### Phase 1 — Quick wins (single-file changes)

**Scope:** Add `clients/shared/js/gameEvents.js`; replace hardcoded string literals; add `durationMs`/`stop_effect`; `deviceColor` NVS persistence.

**Risk:** Low for JS changes. Medium for firmware NVS write (if NVS key collides with existing Preferences keys).

**Acceptance criteria:**
- [ ] `npm test` passes.
- [ ] `ActionEffect.js` uses `GameEvents.TOOK_LEAD` (etc.) — no raw string literals remain.
- [ ] `SoundManager.js` uses `GameEvents` constants — `EVENT_FILE_MAP` keys are verified against the constants at startup (add a `console.warn` for unknown keys).
- [ ] `test_effect` with `durationMs: 3000` auto-stops on firmware after 3 s (verify via Serial monitor).
- [ ] `stop_effect` from admin UI cancels an indefinite `test_effect`.
- [ ] `deviceColor` hex is written to NVS after first `led_config`. After reboot, device shows correct color before WiFi connects.
- [ ] All hardware smoke tests H1–H9 pass.

**Rollback:**
```bash
git revert <phase-1-commit-sha>   # JS changes, no server state affected
```
For firmware: re-flash previous `.bin`. NVS key `deviceColor` is benign if left in flash — it will be ignored by the reverted firmware.

**NVS key collision check:** Before writing, confirm no existing `Preferences` call in `esp8266-sensor` or `esp32-motor` uses the key `"deviceColor"`. Search:
```bash
grep -r 'putString\|getString\|putUInt\|getUInt' clients/esp8266-sensor/src clients/esp32-motor/src
```

---

### Phase 2 — Event queue (firmware only)

**Scope:** Add `EventQueue<T, N>` to `clients/shared/leds/`; replace `_pendingLocalEvent` / `_pendingGlobalEvent` in both firmware projects.

**Risk:** Medium — changes the core event-dispatch path in firmware. A bug here will cause effects to not fire or fire in wrong order.

**Pre-condition:** Phase 1 complete and verified on hardware.

**Acceptance criteria:**
- [ ] `EventQueue` has unit tests compiled under PlatformIO `native` environment (see §4.1).
- [ ] Rapid-fire test: trigger 3 consecutive `scored` messages within 100 ms (use the bot system) — no events are silently lost; at least the highest-priority one fires.
- [ ] `TOOK_LEAD` arriving simultaneously with three `SCORE_PLUS1` events → `TOOK_LEAD` effect fires (priority preservation).
- [ ] Near-simultaneous `scored` + `game_started` messages → `scored` local effect runs to completion, then `game_started` global effect fires (no silent cancellation).
- [ ] All hardware smoke tests H1–H9 pass.

**Rollback:**
```bash
git revert <phase-2-commit-sha>
# Reflash both sensor and motor firmware
```

---

### Phase 3 — Layered effect system (shared firmware)

**Scope:** Implement `EffectLayer` / priority gate; refactor `LedManager` to use it; move ambient, game, and admin effects to the appropriate layer/priority.

**Risk:** High — touches every effect dispatch path. A wrong priority assignment can make effects invisible or permanently override the ambient state.

**Pre-condition:** Phase 2 complete and verified. Open Question OQ1 resolved (priority gate vs. compositor — see analysis §9/C1).

**Acceptance criteria:**
- [ ] Unit test: `playEffect(lowPriority)` while a higher-priority effect is active → request is dropped or queued (not applied).
- [ ] Unit test: high-priority effect completes → system reverts to the previous lower-priority effect, not to black/off.
- [ ] `GAME_PAUSED` amber pulse persists through subsequent `SCORE_PLUS1` events. The specific mechanism depends on the resolution of OQ2 (whether `GAME_PAUSED` lives in the ambient layer or is a higher-priority game effect), but in both cases a `SCORE_PLUS1` must *not* cancel the paused indicator. Verify this on hardware before closing Phase 3.
- [ ] `test_effect` (admin layer) overrides any game effect; when admin effect ends, game/ambient resumes correctly.
- [ ] On `WINNER_SELF` (indefinite rainbow): device holds rainbow until `game_reset` fires, then returns to ambient — not to black.
- [ ] All hardware smoke tests H1–H9 pass.
- [ ] SRAM budget: build with `pio run --target size`. SRAM usage must not increase by more than 10% compared to Phase 2 baseline.

**Rollback:**
```bash
git revert <phase-3-commit-sha>
# Reflash both firmware projects
```

---

### Phase 4 — Per-device LED config (server)

**Scope:** Add per-`chipId` override storage in `LedConfigManager`; new REST endpoints; update `broadcastLedConfig`.

**Risk:** Low-medium — additive server change. The existing type-wide path must continue to work as before.

**Acceptance criteria:**
- [ ] `npm test` passes (new tests for per-chipId CRUD added).
- [ ] `PUT /api/leds/config/sensor` (type-wide) still works and broadcasts to all sensors.
- [ ] `PUT /api/leds/config/sensor/<chipId>` broadcasts only to the matching device.
- [ ] After setting a per-device override, the override persists in `led-config.json` and survives server restart.
- [ ] `DELETE /api/leds/config/sensor/<chipId>` removes the override and device falls back to type default.

**Rollback:**
```bash
git revert <phase-4-commit-sha>
# Restore previous led-config.json from git history if it was modified:
git checkout <phase-4-commit-sha>~1 -- server/data/led-config.json
```

---

### Phase 5 — Sequence numbers and event log

**Scope:** Add `seq` counter to `GameState`; attach to all WS broadcasts; accept `lastSeq` in `register`.

**Risk:** Low — `seq` is an additive optional field. Old clients that don't read it are unaffected.

**Acceptance criteria:**
- [ ] `npm test` passes.
- [ ] Every `scored`, `game_event`, `winner`, `countdown` broadcast includes `"seq": <number>`.
- [ ] Sequence is monotonically increasing across a game session.
- [ ] Firmware that does not send `lastSeq` registers successfully (backward compatible).
- [ ] Display client deduplicates a `scored` message received twice with the same `seq` (simulate by manually calling `ws.send()` twice with same payload in devtools).

**Rollback:**
```bash
git revert <phase-5-commit-sha>
```
Firmware and clients do not need reflashing — `seq` was never a required field.

---

## 4. Testing Infrastructure

### 4.1 Firmware Unit Tests (PlatformIO native)

Add a `test` environment to each firmware's `platformio.ini`:

```ini
[env:native_test]
platform = native
test_framework = unity
# Update test_filter progressively: add test_effect_layer after Phase 3 is implemented
test_filter = test_event_queue
```

Test file locations:
```
clients/esp8266-sensor/test/test_event_queue.cpp
clients/esp32-motor/test/test_effect_layer.cpp
```

Shared tests can live in:
```
clients/shared/leds/test/   (symlinked or duplicated)
```

Minimum test cases required before Phase 2 is merged:

```cpp
// test_event_queue.cpp
void test_push_pop_fifo();          // basic FIFO for equal-priority items
void test_priority_eviction();      // TOOK_LEAD survives 3× SCORE_PLUS1 overflow
void test_empty_pop_returns_none(); // pop() on empty queue does not crash
void test_full_no_crash();          // overflow does not corrupt memory
```

### 4.2 Server Unit Tests

Server uses Node.js built-in test runner (`node --test`). All new server functionality (P4, P5) must have corresponding tests in `server/tests/`:

```
server/tests/LedConfigManager.test.js   (extended for per-chipId CRUD)
server/tests/GameState.seq.test.js      (new, for sequence numbers)
```

Run with: `cd server && npm test`

### 4.3 End-to-End Test (manual, with hardware)

After each phase, run the full hardware smoke checklist (§2.2) and display client checklist (§2.3). Document the result in the PR description with the date and device types tested.

---

## 5. Generalisation Checklist

As the refactor progresses, evaluate each piece of new code against this checklist to ensure it is built for long-term maintainability:

| Concern | Question to ask | Phase where it matters |
|---|---|---|
| **SRP** | Does each class/module have exactly one reason to change? | All |
| **OCP** | Can new event types / effects be added without modifying existing code? | P2–P3 |
| **DIP** | Do high-level modules depend on abstractions, not concretions? | P3 |
| **Single source of truth** | Is every event name / config key defined in exactly one place? | P1 |
| **Zero dynamic allocation** | Does new firmware code use only stack and pre-allocated static objects? | P2–P3 |
| **SRAM budget** | Is the SRAM delta for each phase below 10% of the baseline? | P3 |
| **Backward compatibility** | Does the change conform to the WebSocket contract (§1.1)? | P1, P4, P5 |
| **Testability** | Is the new code testable without hardware (native environment or node --test)? | All |
| **Observability** | Does new code emit structured Serial / console log messages for diagnostics? | All |
| **Graceful degradation** | Does the system behave sensibly when a component is absent or reconnects? | P3, P5 |

---

## 6. Separation of Concerns Map (current → target)

| Concern | Currently lives in | Target (after refactor) |
|---|---|---|
| Event name constants (JS) | Inline string literals in 4 files | `clients/shared/js/gameEvents.js` (single source) |
| Event name constants (C++) | `GameEvents.h` | `GameEvents.h` — already correct ✅ |
| Event → effect mapping | `GameEventMapper.h` switch statement | `GameEventMapper.h` registration table (C3) |
| Effect priority / interruption policy | None (last write wins) | Priority gate / `EffectLayer` in `AnimationManager` |
| LED config per device | Type-wide only | Type-wide default + per-chipId override (P4) |
| Device color persistence | Lost on reboot | NVS `"deviceColor"` key (P1) |
| Game lifecycle FSM | Implicit in `GameState.status` string | Same — but document valid transitions explicitly |
| Sequence numbering / dedup | None | `GameState._seq` counter + client-side dedup (P5) |
| Test effect TTL | None (sticky forever) | `durationMs` field + `stop_effect` message (P1) |

---

*Last updated: 2026-04-24*  
*Authors: @copilot, @tamaygz*
