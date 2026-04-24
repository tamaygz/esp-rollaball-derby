# Refactor Safety Guide — Event / Effect Engine

> **Purpose:** Companion to [event-effect-engine-analysis.md](./event-effect-engine-analysis.md).
> Covers everything needed to execute the refactor safely and reach a clean, modern codebase:
> modernisation policy, regression tests, rollback plans, and per-phase acceptance criteria.

---

## 1. Modernisation Policy

**Goal:** A clean, top-modern state. Legacy patterns, outdated APIs, and redundant code should be removed or upgraded — not preserved for backward compatibility. Every phase should leave the codebase strictly better than it found it.

### 1.1 WebSocket Protocol

The server–client WebSocket protocol is the primary integration boundary. All clients are maintained in this repo, so coordinated protocol changes are acceptable — all sides are updated together in the same PR or phase.

| Change type | Policy | Examples |
|---|---|---|
| Add optional field to existing payload | ✅ Fine — add freely | `+seq`, `+durationMs`, `+layer` |
| Add new `type` value | ✅ Fine | New `stop_effect`, `ack` messages |
| Rename a `type` value | ✅ Acceptable — update all clients in same PR | `scored` → `score_event` if cleaner |
| Remove a payload field | ✅ Acceptable — remove dead code across all clients | Remove unused fields from `scored` |
| Clean up string values inside `events[]` | ✅ Acceptable — update `GameEvents.h`, `GameEvents.js`, firmware, display, admin together | Rename for clarity |
| Change encoding | ✅ If a better approach exists, migrate it | — |

**Rule:** Changes to the protocol must touch all four layers (server, firmware, display client, web admin) in the same PR. Leave no orphaned field or dead message type.

---

### 1.2 REST API

| Endpoint | Policy |
|---|---|
| `PUT /api/leds/config/:deviceType` | Modernise as needed. Remove deprecated overloads. |
| `POST /api/game/start` etc. | Clean up response shapes for consistency. |
| `GET /api/health` | Keep stable — external monitoring may depend on it. |

---

### 1.3 Firmware Shared Library (`clients/shared/leds/`)

| Symbol | Policy |
|---|---|
| `LedEffect::begin()`, `update()`, `isComplete()`, `reset()` | Public API — change only if a strictly better interface is adopted consistently across all effects. |
| `EffectParams` struct fields | Remove unused fields; add new ones as needed. Keep the struct lean. |
| `GameEvents.h` enum names and values | Rename for clarity if the new name is cleaner — update all call sites. |
| `GameEventMapper` switch statement | **Replace with table-driven registration (C3 in analysis §9).** The switch is a known OCP violation. |
| `AnimationManager::playEffect()`, `transitionTo()`, `stop()` | Upgrade to priority-gate signature (C1 / Option A). Old signature is not preserved. |

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
# As of 2026-04-24 (post-PR#27 / T1–T6): 168 pass, 0 fail
```

All pre-existing failures were resolved. Each subsequent phase must maintain or improve on this count — never regress it.

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

> **Verification method:** At the end of each phase, acceptance criteria are verified using GitHub MCP tools (CI workflow status, test run logs) and the hardware smoke checklists below. Phase sign-off is not manual — it is confirmed against actual CI output.

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

**Scope:** Add `clients/shared/js/gameEvents.js`; replace hardcoded string literals; add `durationMs`/`stop_effect`; `deviceColor` LittleFS persistence; chiptype-aware LED config defaults.

**Risk:** Low for JS changes. Medium for firmware NVS write (if NVS key collides with existing Preferences keys).

**Acceptance criteria:**
- [x] `npm test` passes. ✅ 168/168 (post-PR#27)
- [x] `ActionEffect.js` uses `GameEvents.TOOK_LEAD` (etc.) — no raw string literals remain. ✅ (T3)
- [x] `SoundManager.js` uses `GameEvents` constants — `EVENT_FILE_MAP` keys are verified against the constants at startup. ✅ (T3)
- [ ] `websocket.cpp` firmware string maps updated to use `GameEvents.h` enum values (remaining — see T7 in `isolated-tasks.md`).
- [x] `test_effect` with `durationMs: 3000` auto-stops on firmware after 3 s. ✅ (P6)
- [x] `stop_effect` from admin UI cancels an indefinite `test_effect`. ✅ (P6)
- [x] `deviceColor` hex persists to LittleFS after `led_config`. After reboot, device shows correct color before WiFi connects. ✅ (P5)
- [x] ESP32 sensors receive `gpioPin: 4`; ESP8266 sensors receive `gpioPin: 2`. ✅ (T4)
- [ ] All hardware smoke tests H1–H9 pass (pending hardware verification).

**Rollback:**
```bash
git revert <phase-1-commit-sha>   # JS changes, no server state affected
```
For firmware: re-flash previous `.bin`. NVS key `deviceColor` is benign if left in flash — it will be ignored by the reverted firmware.

---

### Phase 2 — Event queue (firmware only)

**Scope:** Integrate `EventQueue<T, N>` from `clients/shared/leds/EventQueue.h` into both firmware projects; replace `_pendingLocalEvent` / `_pendingGlobalEvent` single-slot variables.

**Risk:** Medium — changes the core event-dispatch path in firmware. A bug here will cause effects to not fire or fire in wrong order.

**Pre-condition:** Phase 1 complete. ✅ `EventQueue.h` exists and all 4 native unit tests pass (T5, PR#27).

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

### Phase 3 — Priority gate integration (firmware call sites)

**Scope:** Wire `AnimationManager` priority constants (`PRIORITY_AMBIENT/GAME/ADMIN`) into all firmware call sites in both firmware projects; replace ad-hoc `restoreAmbient()` calls; move admin `test_effect` to `PRIORITY_ADMIN`; refactor `GameEventMapper` from switch to table-driven (C3/OCP fix).

**Risk:** High — touches every effect dispatch path. A wrong priority assignment can make effects invisible or permanently override the ambient state.

**Pre-condition:** Phase 2 complete. ✅ Priority gate exists in `AnimationManager` and both native tests pass (T6, PR#27). OQ1 is resolved: Option A (single animator, priority gate).

**Acceptance criteria:**
- [x] Unit test: `playEffect(lowPriority)` while a higher-priority effect is active → request is dropped (not applied). ✅ (T6, `test_low_priority_dropped_while_high_active`)
- [x] Unit test: high-priority effect completes → `_activePriority` resets to `PRIORITY_AMBIENT`. ✅ (T6, `test_priority_resets_after_effect_completes`)
- [ ] `GAME_PAUSED` amber pulse persists through subsequent `SCORE_PLUS1` events (OQ2 resolved: `GAME_PAUSED` is ambient layer, priority 0; scoring fires at priority 1 — gate must NOT suppress higher-priority game event over lower-priority ambient, only suppress lower over higher). Verify on hardware.
- [ ] `test_effect` (admin layer, priority 2) overrides any game effect; when admin effect ends, game/ambient resumes correctly.
- [ ] On `WINNER_SELF` (indefinite rainbow): device holds rainbow until `game_reset` fires, then returns to ambient — not to black.
- [ ] `GameEventMapper` switch replaced with `LOCAL_EFFECTS[]` / `GLOBAL_EFFECTS[]` registration tables (C3).
- [ ] All hardware smoke tests H1–H9 pass.
- [ ] SRAM budget: build with `pio run --target size`. SRAM usage must not increase by more than 10% compared to Phase 2 baseline.

**Rollback:**
```bash
git revert <phase-3-commit-sha>
# Reflash both firmware projects
```

---

### Phase 4 — Per-device LED config (server)

**Scope:** Add LED admin page per-device override UI. Server-side per-device config is already done.

**Risk:** Low — additive UI change. The existing type-wide path and REST endpoints are complete.

**Acceptance criteria:**
- [x] `npm test` passes (7 new chiptype + per-device CRUD tests added in T4). ✅ 168/168
- [x] `PUT /api/leds/config/sensor` (type-wide) still works and broadcasts to all sensors. ✅
- [x] `PUT /api/leds/config/sensor/<chipId>` broadcasts only to the matching device. ✅
- [x] After setting a per-device override, the override persists in `led-config.json` and survives server restart. ✅
- [x] `DELETE /api/leds/config/sensor/<chipId>` removes the override and device falls back to type default. ✅
- [ ] LED admin page exposes per-device override UI (T11 in `isolated-tasks.md`).

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

> **Philosophy:** Moderate — cover the highest-risk paths without building a heavyweight test harness. Focus on pure-logic units (EventQueue, priority gate) that are easy to run natively and hard to debug on hardware. Do not replicate integration tests that are better done via hardware smoke tests.

### 4.1 Firmware Unit Tests (PlatformIO native)

✅ **Native test environment is already set up** (T5/T6, PR#27). `clients/esp8266-sensor/platformio.ini` has `[env:native_test]` with Unity framework.

Current test coverage (6 tests, all passing):

```
clients/esp8266-sensor/test/test_event_queue/test_event_queue.cpp     — 4 tests
  test_push_pop_fifo              basic FIFO ordering
  test_priority_eviction          TOOK_LEAD survives 3× SCORE_PLUS1 overflow
  test_empty_pop_returns_false    pop on empty, does not crash
  test_full_no_crash              overflow does not corrupt memory

clients/esp8266-sensor/test/test_animation_manager/test_animation_manager.cpp  — 2 tests
  test_low_priority_dropped_while_high_active
  test_priority_resets_after_effect_completes
```

Run with: `pio test -e native_test` (from `clients/esp8266-sensor/`)

As Phase 3 integration (T9) progresses, extend the AnimationManager test file with:
- `test_admin_priority_overrides_game` — PRIORITY_ADMIN wins over PRIORITY_GAME
- `test_ambient_resumes_after_game_effect` — ambient returns after game effect completes

### 4.2 Server Unit Tests

Server uses Node.js built-in test runner (`node --test`). Current baseline: **168/168 pass** (post-PR#27).

For Phase 5 (sequence numbers), add:

```
server/tests/gameState.seq.test.js   (new — monotonic counter, dedup on replay)
```

Run with: `cd server && npm test`

### 4.3 End-to-End Test (manual, with hardware)

After each phase, run the full hardware smoke checklist (§2.2) and display client checklist (§2.3). Document the result in the PR description with the date and device types tested.

---

## 5. Generalisation Checklist

> **Reference project:** [`tamaygz/esp-buzzwire` (feat-web branch)](https://github.com/tamaygz/esp-buzzwire/tree/feat-web) will need the same shared LED/effect components. Use it as a north star for generalisation decisions — any shared module extracted from this project should be immediately usable in buzzwire without modification.
>
> Key patterns already proven in buzzwire that apply here:
> - **Single `config.h`** — one file for all pins and tuning constants; runtime config in a separate `cfg` struct
> - **Pure-function LED API** (`ledsIdle()`, `ledsFail()`, etc.) backed by non-blocking millis() logic — same as our `LedEffect` hierarchy
> - **FSM with `enterState()`** — explicit transitions, single state variable, logged on Serial — mirrors what we want in `GameState` FSM
> - **Small, named modules** (`game.cpp`, `leds.cpp`, `matrix.cpp`, `sensors.cpp`, `promode.cpp`) — one concern per file, each under ~200 lines

As the refactor progresses, evaluate each piece of new code against this checklist to ensure it is built for long-term maintainability:

| Concern | Question to ask | Phase where it matters |
|---|---|---|
| **SRP** | Does each class/module have exactly one reason to change? Keep files small (target < 200 lines). | All |
| **OCP** | Can new event types / effects be added without modifying existing code? (Table-driven mapper, not switch.) | P2–P3 |
| **DIP** | Do high-level modules depend on abstractions, not concretions? | P3 |
| **Single source of truth** | Is every event name / config key defined in exactly one place? | P1 |
| **Zero dynamic allocation** | Does new firmware code use only stack and pre-allocated static objects? | P2–P3 |
| **SRAM budget** | Is the SRAM delta for each phase below 10% of the baseline? | P3 |
| **Reusability** | Can this module be dropped into esp-buzzwire without changes? | P2–P3 |
| **Testability** | Is the new code testable without hardware (native environment or node --test)? | All |
| **Observability** | Does new code emit structured Serial / console log messages for diagnostics? | All |
| **Graceful degradation** | Does the system behave sensibly when a component is absent or reconnects? | P3, P5 |

---

## 6. Separation of Concerns Map (current → target)

> **Design principles:** Scope each concern separately. Keep source files small (target < 200 lines). Name files and symbols clearly so intent is obvious without reading implementation. Make every module reusable and extendable by design — if it cannot be dropped into esp-buzzwire as-is, it is not yet sufficiently separated.

| Concern | Currently lives in | Target (after refactor) |
|---|---|---|
| Event name constants (JS) | Inline string literals in 4 files | `clients/shared/js/gameEvents.js` (single source) |
| Event name constants (C++) | `GameEvents.h` | `GameEvents.h` — already correct ✅ |
| Event → effect mapping | `GameEventMapper.h` switch statement | `GameEventMapper.h` **registration table** (C3 — replaces switch entirely) |
| Effect priority / interruption policy | None (last write wins) | **Priority gate** in `AnimationManager` (C1 Option A) |
| LED config per device | Type-wide only | Type-wide default + per-chipId override (P4 ✅ done) |
| Device color persistence | Lost on reboot | LittleFS `saveState()` in both firmware (P5 ✅ done) |
| Game lifecycle FSM | Implicit in `GameState.status` string | Explicit transitions with `enterState()`-style logging |
| Sequence numbering / dedup | None | `GameState._seq` counter + client-side dedup (P5) |
| Test effect TTL | None (sticky forever) | `durationMs` field + `stop_effect` message (P6 ✅ done) |
| Shared LED effects (C++) | `clients/shared/leds/` | Same — keep as reusable library, usable by esp-buzzwire |
| `EventQueue<T,N>` template | ✅ `clients/shared/leds/EventQueue.h` (T5 — PR#27) | Integration into firmware pending (T8) |
| Priority gate for effects | ✅ `AnimationManager::playEffect(effect, priority)` with `PRIORITY_AMBIENT/GAME/ADMIN` (T6 — PR#27) | `EffectLayer.h` not needed — priority gate is in `AnimationManager` (Option A) |
| Firmware call-site priority wiring | Not yet done — all calls use default `PRIORITY_GAME` | Wire correct priority at each call site (T9) |
| `GameEventMapper` switch → table | Still a switch statement — OCP violation | Replace with `LOCAL_EFFECTS[]` / `GLOBAL_EFFECTS[]` registration tables (T9) |

---

*Last updated: 2026-04-24 (post-PR#27: T1–T6 complete, T7–T11 scoped)*  
*Authors: @copilot, @tamaygz*
