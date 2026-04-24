---
goal: Refactor Event / Effect Engine — Complete remaining PR#25 scope (firmware event queue, priority gate integration, per-device LED admin UI, event sequence numbers)
version: 1.0
date_created: 2026-04-24
last_updated: 2026-04-24
owner: tamaygz
status: 'Planned'
tags: [refactor, firmware, server, web, led, event-engine]
---

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

# Refactor: Event / Effect Engine — PR#25 Remaining Scope

**Feature:** Complete the remaining scope of the Event / Effect Engine refactor on branch `refactor-events` (PR#25). Shared primitives (`EventQueue<T,N>`, `AnimationManager` priority gate, `GameEvents.js`), chiptype-aware LED defaults, per-device server overrides, `stop_effect` / `durationMs`, and LittleFS `deviceColor` persistence are already merged. This plan covers the five remaining tasks (T7–T11) that complete the refactor.

**Branch:** `refactor-events` — PR [#25](https://github.com/tamaygz/esp-rollaball-derby/pull/25)

**Dependencies (verified merged):**
- T3 `clients/shared/js/gameEvents.js` ✅ (exists; referenced by [clients/display/index.html](clients/display/index.html#L24), [clients/display/js/main.js](clients/display/js/main.js#L123-L126), [clients/display/js/effects/ActionEffect.js](clients/display/js/effects/ActionEffect.js#L199-L206))
- T4 Chiptype-aware LED defaults ✅ (verified via `sensor-esp32` entry in `server/data/led-config.json`)
- T5 `clients/shared/leds/EventQueue.h` ✅ (exists + native tests)
- T6 `AnimationManager` priority gate ✅ (`PRIORITY_AMBIENT/GAME/ADMIN` verified in [clients/shared/leds/AnimationManager.h](clients/shared/leds/AnimationManager.h#L72-L74) and [AnimationManager.cpp](clients/shared/leds/AnimationManager.cpp#L11))
- P5 LittleFS `deviceColor` persistence ✅ (verified in [clients/esp8266-sensor/src/main.cpp](clients/esp8266-sensor/src/main.cpp#L127-L261))
- P6 `stop_effect` + `durationMs` ✅ (verified in [server/src/ws/ConnectionManager.js](server/src/ws/ConnectionManager.js#L326-L364) and [server/src/routes/leds.js](server/src/routes/leds.js#L360-L454))

---

## Section 1 — Requirements & Constraints

### Functional Requirements

- **REQ-001**: Firmware `websocket.cpp` (both `esp8266-sensor` and `esp32-motor`) must map all incoming WS event strings using the exact literals defined in `clients/shared/leds/GameEvents.h` and mirrored in `clients/shared/js/gameEvents.js`. No drift permitted.
- **REQ-002**: Firmware must replace single-slot `_pendingLocalEvent` / `_pendingGlobalEvent` with `EventQueue<LocalEventType, 4>` / `EventQueue<GlobalEventType, 4>` and drain one entry per main-loop tick.
- **REQ-003**: Rapid-fire: 3 consecutive `scored` messages within 100 ms must not silently lose the highest-priority event.
- **REQ-004**: Near-simultaneous `scored` + `game_event` messages: the local event runs to completion before the global event fires (or vice versa based on priority), never silently cancelled.
- **REQ-005**: All firmware `playEffect()` call sites must pass an explicit priority constant: `PRIORITY_AMBIENT` for status/ambient, `PRIORITY_GAME` for game events, `PRIORITY_ADMIN` for `test_effect`.
- **REQ-006**: `GameEventMapper::onLocalEvent()` / `onGlobalEvent()` switch statements must be replaced with a table-driven registration pattern (`LOCAL_EFFECTS[]` / `GLOBAL_EFFECTS[]` arrays of `{event, effect*, durationMs, priority}`).
- **REQ-007**: Admin LED page (`clients/web/admin` or equivalent) must expose per-device override UI for each connected `sensor`/`motor`: show effective config, PUT override, DELETE override.
- **REQ-008**: Server `GameState` must expose a monotonically increasing `seq` counter; all WS broadcasts (`scored`, `game_event`, `winner`, `countdown`, `state`) must carry `seq`.
- **REQ-009**: `register` payload must accept an optional `lastSeq` from clients; server logs gap if `lastSeq < currentSeq - 1`.
- **REQ-010**: Display client must deduplicate any broadcast whose `seq <= _lastSeenSeq`.

### Non-Functional Requirements

- **NFR-001**: SRAM usage on ESP8266 must not increase by more than 10% relative to the pre-T8 baseline. Captured via `pio run --target size`.
- **NFR-002**: Native unit tests (`pio test -e native_test`) must continue to pass (current baseline: 6 tests — 4 `EventQueue` + 2 `AnimationManager`).
- **NFR-003**: Server test suite (`cd server && npm test`) must continue to report 0 failures; add ≥2 new tests for sequence numbers (T11) and ≥1 for per-device admin UI round-trip integration.
- **NFR-004**: Existing hardware smoke tests H1–H9 (see [docs/refactor-safety-guide.md](docs/refactor-safety-guide.md)) must all pass after every phase.

### Security Requirements

- **SEC-001**: Per-device admin UI `PUT /api/leds/config/:deviceType/:chipId` body must be validated the same way the type-wide endpoint already is — reject invalid pins via `ledPinIsValid(pin)`, clamp `ledCount` to platform max.
- **SEC-002**: `seq` field is informational only; server must not trust client-supplied `lastSeq` for authoritative state — it is a hint for logging/replay, not a re-auth gate.

### Constraints

- **CON-001**: Firmware `EventQueue<T,N>` already implements priority-based overflow eviction. Do not redesign; integrate as-is.
- **CON-002**: `AnimationManager::playEffect(effect, priority = PRIORITY_GAME)` already exists with default `PRIORITY_GAME`. All existing call sites remain valid without modification; only three categories of calls need explicit priority changes (ambient, admin).
- **CON-003**: Display client uses parallel GSAP/Pixi timelines — the firmware-style priority gate must NOT be ported there (see "Architectural Constraints" in [docs/event-effect-engine-analysis.md](docs/event-effect-engine-analysis.md)).
- **CON-004**: `scored` broadcast format must remain `{ type: 'scored', seq, payload: { playerId, points, events[] } }` — `seq` is additive at the top level, not inside `payload`.
- **CON-005**: No new libraries. Firmware already has `ArduinoJson`, `NeoPixelBus`, `ArduinoWebsockets`, `LittleFS`, `WiFiManager`. Server uses only existing deps.
- **CON-006**: All protocol-affecting changes must ship server + all four client types (firmware sensor, firmware motor, display, web admin) in the same PR/commit series — no orphaned fields.

### Guidelines

- **GUD-001**: Firmware file convention: queues live in `websocket.h` (private members); mapping tables live in `clients/shared/leds/GameEventMapper.h` (`constexpr` arrays, `PROGMEM` where beneficial on ESP8266).
- **GUD-002**: Server: `seq` lives on `GameState` as a private `_seq = 0`; exposed via `getSeq()` and `nextSeq()`.
- **GUD-003**: Admin UI: additive — do not remove the type-wide config panel; add a per-device panel below it. Vanilla JS, no framework (project convention).
- **GUD-004**: Serial log prefix convention: `[WS]`, `[LED]`, `[EVT]`, `[GS]`. Keep it.

### Patterns

- **PAT-001**: Table-driven dispatch — `GameEventMapper` reads `LOCAL_EFFECTS[]` entries of `{LocalEventType event; LedEffect* effect; uint16_t durationMs; uint8_t priority;}` and calls `_animator->playEffect(entry.effect, entry.priority)`.
- **PAT-002**: Queue drain — one event per `loop()` tick to keep WiFi/WS cooperative; drain order is FIFO within same priority.
- **PAT-003**: Sequence attachment — `ConnectionManager._broadcast(type, payload)` helper wraps `_gameState.nextSeq()` into the envelope; all existing broadcast call sites go through it.

---

## Section 2 — Implementation Steps

### Phase T7 — Firmware WebSocket event-string cleanup

**Scope:** `clients/esp8266-sensor/src/websocket.cpp`, `clients/esp32-motor/src/websocket.cpp`  
**Risk:** Low

| Task ID | Description | Files |
|---------|-------------|-------|
| TASK-T7-1 | Audit every `strcmp(type, "...")` and `strcmp(event, "...")` in `esp8266-sensor/src/websocket.cpp`. Cross-reference literal strings against `clients/shared/leds/GameEvents.h` comments and `clients/shared/js/gameEvents.js` values. Correct any drift. | `clients/esp8266-sensor/src/websocket.cpp` |
| TASK-T7-2 | Add a header comment block above the mapping section citing `GameEvents.h` as canonical source. | `clients/esp8266-sensor/src/websocket.cpp` |
| TASK-T7-3 | Repeat TASK-T7-1 and TASK-T7-2 for the motor firmware. | `clients/esp32-motor/src/websocket.cpp` |
| TASK-T7-4 | Verify: `grep -E 'strcmp.*(took_lead\|score_3\|game_started\|streak_three_2x\|streak_zero_3x\|became_last)' clients/*/src/websocket.cpp` — every match corresponds to a `GameEvents.*` constant value. | (verification only) |

**Acceptance:**
- [ ] All firmware `strcmp` strings align with `GameEvents.h` and `gameEvents.js`.
- [ ] Firmware builds (`pio run`) for both sensor and motor targets.
- [ ] `pio test -e native_test` still passes (6/6).
- [ ] Hardware smoke tests H1–H9 pass.

---

### Phase T8 — Firmware `EventQueue` integration

**Scope:** `clients/esp8266-sensor/src/websocket.h/.cpp`, `clients/esp32-motor/src/websocket.h/.cpp`, `clients/esp8266-sensor/src/main.cpp`, `clients/esp32-motor/src/main.cpp`  
**Risk:** Medium  
**Prerequisite:** T7

| Task ID | Description | Files |
|---------|-------------|-------|
| TASK-T8-1 | In `esp8266-sensor/src/websocket.h`: `#include "EventQueue.h"`; replace `LocalEventType _pendingLocalEvent = LocalEventType::NONE;` with `EventQueue<LocalEventType, 4> _localQueue;` and likewise for global. | `clients/esp8266-sensor/src/websocket.h` |
| TASK-T8-2 | Replace all `_pendingLocalEvent = X` assignments with `_localQueue.push(X)`; same for global queue. | `clients/esp8266-sensor/src/websocket.cpp` |
| TASK-T8-3 | Update `pollLocalEvent(LocalEventType& out)` to return `_localQueue.pop(out)`; same for `pollGlobalEvent`. | `clients/esp8266-sensor/src/websocket.cpp`, `websocket.h` |
| TASK-T8-4 | `main.cpp` loop: keep draining pattern (`while (ws.pollLocalEvent(ev)) { ...; break; }`) — one event per tick, break after processing. | `clients/esp8266-sensor/src/main.cpp` |
| TASK-T8-5 | Repeat TASK-T8-1 through TASK-T8-4 for the motor firmware. | `clients/esp32-motor/src/websocket.*`, `clients/esp32-motor/src/main.cpp` |
| TASK-T8-6 | No new test files required — `EventQueue` behaviour is already covered by the 4 native tests in `clients/esp8266-sensor/test/test_event_queue/`. Verify they still pass after integration. | (verification only) |

**Acceptance:**
- [ ] Both firmware builds succeed.
- [ ] `pio test -e native_test` passes (6/6).
- [ ] Bot-driven rapid fire (3 `scored` within 100 ms) — highest-priority event survives. Verify via serial log.
- [ ] `TOOK_LEAD` + 3× `SCORE_PLUS1` simultaneously → `TOOK_LEAD` effect fires.
- [ ] Hardware smoke tests H1–H9 pass.
- [ ] `pio run --target size` — SRAM delta ≤ +10% vs pre-T8 baseline (record pre/post in PR description).

---

### Phase T9 — Priority gate wired into call sites + `GameEventMapper` table

**Scope:** `clients/esp8266-sensor/src/led.cpp`/`led.h`, `clients/esp32-motor/src/led.cpp`/`led.h`, `clients/shared/leds/GameEventMapper.h`  
**Risk:** High — touches every effect dispatch path  
**Prerequisite:** T8

| Task ID | Description | Files |
|---------|-------------|-------|
| TASK-T9-1 | Audit all `_animator.playEffect(&X)` calls in `esp8266-sensor/src/led.cpp`. Classify each by layer: ambient (`applyStatus`, `restoreAmbient`, connection-status blink/pulse) vs game (everything mapped from events). | `clients/esp8266-sensor/src/led.cpp` |
| TASK-T9-2 | In `esp8266-sensor/src/led.cpp`: change every ambient-class call to `_animator.playEffect(&X, AnimationManager::PRIORITY_AMBIENT)`. Leave game-class calls at default (equivalent to `PRIORITY_GAME`). | `clients/esp8266-sensor/src/led.cpp` |
| TASK-T9-3 | In `esp8266-sensor/src/led.cpp`: locate `test_effect` dispatch (in the `handleTestEffect` or equivalent path) and pass `AnimationManager::PRIORITY_ADMIN`. | `clients/esp8266-sensor/src/led.cpp` |
| TASK-T9-4 | Repeat T9-1 through T9-3 for the motor firmware. | `clients/esp32-motor/src/led.cpp` |
| TASK-T9-5 | Refactor `clients/shared/leds/GameEventMapper.h`: replace the `switch (event)` in `onLocalEvent()` with a lookup over `constexpr LocalEffectEntry LOCAL_EFFECTS[] = { {LocalEventType::TOOK_LEAD, &_chaseEffect, 1200, AnimationManager::PRIORITY_GAME}, ... };`. | `clients/shared/leds/GameEventMapper.h` |
| TASK-T9-6 | Same refactor for `onGlobalEvent()` → `GLOBAL_EFFECTS[]`. | `clients/shared/leds/GameEventMapper.h` |
| TASK-T9-7 | Add a native Unity test `test/test_game_event_mapper/test_game_event_mapper.cpp` that registers a mock animator and asserts: (a) every `LocalEventType` value has at least one entry, (b) unknown enum value yields a no-op. | `clients/esp8266-sensor/test/test_game_event_mapper/` |

**Acceptance:**
- [ ] `pio test -e native_test` passes (≥7/7 with new mapper tests).
- [ ] `GAME_PAUSED` amber pulse (ambient, priority 0) is preemptable by `SCORE_PLUS1` (game, priority 1): pause pulse yields to score sparkle, returns after.
- [ ] `test_effect` (admin, priority 2) overrides any game effect; game/ambient resumes on `stop_effect` or after `durationMs`.
- [ ] `WINNER_SELF` indefinite rainbow holds until `GAME_RESET` fires.
- [ ] Adding a new `LocalEventType` requires only a new row in `LOCAL_EFFECTS[]` — no switch modification.
- [ ] Hardware smoke tests H1–H9 all pass.

---

### Phase T10 — LED admin per-device override UI

**Scope:** `clients/web/` (admin page assets), `server/views/admin/` (if server-rendered partials)  
**Risk:** Low — additive UI; server REST endpoints already exist (`GET/PUT/DELETE /api/leds/config/:deviceType/:chipId`)  
**Prerequisite:** None (independent; can run parallel with T7–T9)

| Task ID | Description | Files |
|---------|-------------|-------|
| TASK-T10-1 | Identify current admin LED page entry point. Run `grep -r '/api/leds/config' clients/web server/views`. | (discovery) |
| TASK-T10-2 | Add a "Connected devices" section to the admin LED page that renders one row per connected `sensor` / `motor` client: columns `chipId`, `chipType`, effective `pin`, `count`, `topology`, buttons `[Edit override]` `[Reset to default]`. Source data: combine `GET /api/clients` (or equivalent listing) with `GET /api/leds/config/:deviceType/:chipId`. | `clients/web/js/led-admin.js`, `clients/web/led-admin.html` (or equivalent) |
| TASK-T10-3 | `[Edit override]` opens a form pre-populated with the effective config; submit issues `PUT /api/leds/config/:deviceType/:chipId` with the new body; on success refresh row. | `clients/web/js/led-admin.js` |
| TASK-T10-4 | `[Reset to default]` issues `DELETE /api/leds/config/:deviceType/:chipId`; on success, row re-renders showing type default. | `clients/web/js/led-admin.js` |
| TASK-T10-5 | XSS guard: user-supplied fields (`chipId`, `chipType`) rendered via `textContent` or the existing `_esc()` helper. | `clients/web/js/led-admin.js` |
| TASK-T10-6 | Add a server integration test: PUT override → expect `led_config` WS message delivered only to the matching `chipId`, not to siblings. | `server/tests/ledConfigManager.test.js` or `integration.test.js` |

**Acceptance:**
- [ ] Admin page lists every currently connected `sensor` and `motor` with their effective LED config.
- [ ] PUT override broadcasts `led_config` only to the target `chipId`; other devices unaffected.
- [ ] DELETE override restores type default and sends fresh `led_config` to the device.
- [ ] `npm test` passes with +1 new integration test.
- [ ] Hardware smoke test: set override on one sensor, verify pin/count updates live without reboot; delete, verify reverts.

---

### Phase T11 — Event sequence numbers

**Scope:** `server/src/game/GameState.js`, `server/src/ws/ConnectionManager.js`, `clients/display/js/main.js`, `clients/display/js/connection.js`  
**Risk:** Low — additive optional field  
**Prerequisite:** None (independent)

| Task ID | Description | Files |
|---------|-------------|-------|
| TASK-T11-1 | In `server/src/game/GameState.js`: add private `_seq = 0`; add `nextSeq() { return ++this._seq; }` and `getSeq()`. Reset to 0 on full game reset. | `server/src/game/GameState.js` |
| TASK-T11-2 | In `server/src/ws/ConnectionManager.js`: introduce a helper `_broadcast(type, payload)` that injects `seq: this._gameState.nextSeq()` into the JSON envelope. Refactor every broadcast call (`scored`, `game_event`, `winner`, `countdown`, `state`) to use it. | `server/src/ws/ConnectionManager.js` |
| TASK-T11-3 | Extend `handleRegister` in `ConnectionManager.js` to read optional `payload.lastSeq`. If `lastSeq < _gameState.getSeq() - 1`, log a gap warning (informational; no replay in this phase). | `server/src/ws/ConnectionManager.js` |
| TASK-T11-4 | In `clients/display/js/main.js`: on every incoming message, track `_lastSeenSeq`. If `msg.seq && msg.seq <= _lastSeenSeq`, drop the message (no rendering). | `clients/display/js/main.js` |
| TASK-T11-5 | Add two server tests: (a) `nextSeq()` is monotonic and strictly increasing across all broadcast types; (b) `lastSeq` in `register` is accepted without error. | `server/tests/gameState.test.js`, `server/tests/connectionManager.test.js` |

**Acceptance:**
- [ ] `npm test` — 0 failures, +2 new tests pass.
- [ ] Every outbound WS envelope (`scored`, `game_event`, `winner`, `countdown`, `state`) carries `"seq": <number>`.
- [ ] Manually resending a previously-seen `scored` message (via devtools `ws.send`) does not trigger a duplicate effect in the display client.
- [ ] `register` without `lastSeq` still works (backward compatibility — firmware may not send it yet).

---

## Section 3 — Alternatives Considered

- **ALT-001 (rejected for T9):** Three-layer `EffectLayer` compositor (see analysis §4.1 Option B). Rejected for ESP8266 due to 3× per-layer pixel buffer cost. Priority gate (Option A / T6) achieves required behaviour at zero SRAM cost.
- **ALT-002 (rejected for T8):** Deep queue (N=16). Rejected — `N=4` is sufficient given sensor/motor event rates (≤10 Hz typical) and conserves SRAM on ESP8266.
- **ALT-003 (rejected for T11):** Server-side ring buffer for replay on reconnect. Rejected as out-of-scope for this PR; `seq` is added now as the foundation, replay can be added in a follow-up if needed.
- **ALT-004 (rejected for T10):** Redesign admin UI with a front-end framework. Rejected — project convention is vanilla JS with `window.Derby` namespace; framework adoption is a separate decision.

---

## Section 4 — Dependencies & Risks

### Dependencies

- **DEP-001**: T8 depends on T7 — firmware string-to-enum mapping must be correct before swapping the storage mechanism.
- **DEP-002**: T9 depends on T8 — queue drain semantics are required before priority gate testing is meaningful under load.
- **DEP-003**: T10 and T11 are independent and can proceed in parallel with T7–T9.
- **DEP-004**: `clients/shared/js/gameEvents.js` (T3 ✅) is a runtime dependency of `clients/display/index.html` via `<script src="/shared/js/gameEvents.js">`; the `/shared` static route in `server/src/index.js` must remain.

### Risks

- **RISK-001** (T8): Queue integration errors may cause silent event loss under load. Mitigation: retain the existing 4 native `EventQueue` tests; add serial logging on every `push`/`pop` behind a compile flag `#define DERBY_WS_TRACE`.
- **RISK-002** (T9): Misassigned priorities can make ambient effects invisible (always preempted) or admin overrides sticky. Mitigation: end-to-end hardware smoke H4 + H5 (admin effect start, `stop_effect` ends it).
- **RISK-003** (T9): `GameEventMapper` table refactor may drop an event mapping. Mitigation: TASK-T9-7 unit test asserts every `LocalEventType`/`GlobalEventType` has an entry.
- **RISK-004** (T10): XSS via user-supplied `chipId` in device list. Mitigation: strict `textContent` / `_esc()` rendering per project convention.
- **RISK-005** (T11): Older clients that don't send `lastSeq` should register without error. Mitigation: server treats `lastSeq` as optional; add an explicit test.
- **RISK-006** (all phases): SRAM regression on ESP8266. Mitigation: baseline `pio run --target size` before each firmware phase and gate merge on ≤10% delta.

---

## Section 5 — Files Changed (summary)

| File | Phase | Change |
|------|-------|--------|
| `clients/esp8266-sensor/src/websocket.cpp` | T7 | Align `strcmp` literals with `GameEvents.h` + header comment |
| `clients/esp8266-sensor/src/websocket.h/.cpp` | T8 | Single slot → `EventQueue<T,4>` |
| `clients/esp8266-sensor/src/main.cpp` | T8 | Drain loop — one event per tick |
| `clients/esp8266-sensor/src/led.cpp` | T9 | Explicit `PRIORITY_AMBIENT` / `PRIORITY_ADMIN` at call sites |
| `clients/esp32-motor/src/websocket.cpp` | T7/T8 | Same as sensor |
| `clients/esp32-motor/src/websocket.h` | T8 | Same |
| `clients/esp32-motor/src/main.cpp` | T8 | Same |
| `clients/esp32-motor/src/led.cpp` | T9 | Same |
| `clients/shared/leds/GameEventMapper.h` | T9 | Switch → table-driven `LOCAL_EFFECTS[]` / `GLOBAL_EFFECTS[]` |
| `clients/esp8266-sensor/test/test_game_event_mapper/` | T9 | New native tests |
| `clients/web/js/led-admin.js` | T10 | Per-device override UI |
| `clients/web/led-admin.html` (or admin entrypoint) | T10 | Per-device section markup |
| `server/src/game/GameState.js` | T11 | `_seq` counter, `nextSeq()`, `getSeq()` |
| `server/src/ws/ConnectionManager.js` | T11 | `_broadcast()` helper, `lastSeq` in `register` |
| `server/tests/gameState.test.js` | T11 | Monotonic seq test |
| `server/tests/connectionManager.test.js` | T11 | `lastSeq` accepted in register |
| `server/tests/ledConfigManager.test.js` or `integration.test.js` | T10 | Override routes only target matching `chipId` |
| `clients/display/js/main.js` | T11 | `_lastSeenSeq` dedup |

---

## Section 6 — Test Strategy

### Unit tests (native, `pio test -e native_test`)

- [ ] Existing 4 `EventQueue` tests remain green.
- [ ] Existing 2 `AnimationManager` priority-gate tests remain green.
- [ ] New `GameEventMapper` table coverage test (T9).

### Server tests (`cd server && npm test`)

- [ ] All existing tests remain green.
- [ ] +2 `GameState` / `ConnectionManager` sequence-number tests (T11).
- [ ] +1 per-device override integration test (T10).

### Hardware smoke tests (run after each firmware-touching phase)

Reference checklist in [docs/refactor-safety-guide.md](docs/refactor-safety-guide.md) section 2.2:
- [ ] H1–H9 pass on at least one ESP8266 sensor and one ESP32 motor.

### Load / ordering tests (T8 verification via Bot Manager)

- [ ] Burst of 3 `score` events within 100 ms → highest-priority local event fires.
- [ ] `score` immediately followed by `game_paused` → both effects observed (no silent cancellation).

---

## Section 7 — Rollout & Rollback

Each phase is an independently revertable commit on `refactor-events`. Before merging PR#25:

1. Land T7 → smoke test → commit.
2. Land T8 → native + smoke → commit.
3. Land T9 → native + smoke → commit.
4. Land T10 → server + manual admin test → commit.
5. Land T11 → server tests + display dedup manual test → commit.

### Rollback

```powershell
git revert <phase-commit-sha>
# For firmware phases (T7–T9): re-flash the previous firmware build for both sensor and motor.
# For server/web phases (T10–T11): redeploy server; hard-reload admin/display browsers.
```

LittleFS `state.json` and `server/data/led-config.json` are forward-compatible: reverted firmware and server ignore unknown fields.

---

## Section 8 — Handoff

After completion, hand off to execution agent (e.g., `blueprint-mode` or `Software Engineer Agent`) to implement each phase in order. Update `docs/progress.md` and `docs/isolated-tasks.md` statuses (T7→T11) as each phase closes. Final PR description should reference this plan file and cite the commit SHA for each phase.
