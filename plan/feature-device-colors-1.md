# feature-device-colors-1

## 1. Requirements

| ID | Description | Priority |
|----|-------------|----------|
| REQ-001 | Server assigns every player a `colorIndex` (0-15) from the shared 16-color palette | Must |
| REQ-002 | Hardware devices (sensor/motor) get sticky color assignments persisted in `led-config.json` keyed by ESP chipId | Must |
| REQ-003 | On reconnect, a device with a known chipId gets its previously assigned color | Must |
| REQ-004 | Non-device players (web clients) get colors from the pool not occupied by device mappings | Must |
| REQ-005 | Device color is sent to ESP in `led_config` WS message as `deviceColor` hex string | Must |
| REQ-006 | ESP uses device color as idle/ambient effect color and as default for test effects (replacing hardcoded green/orange/red with tinted variants) | Must |
| REQ-007 | ESP generates a random color on first boot if no server config is present yet | Must |
| REQ-008 | Admin LED page shows device color assignment per-device with a color picker dropdown | Must |
| REQ-009 | Admin LED page effect test color dropdown has "Device Color" as first/default option | Must |
| REQ-010 | Display client and web admin use `colorIndex` from player state instead of array position | Should |
| REQ-011 | ESP sends `chipId` (uint32 hex string) in registration payload for device identification | Must |

## 2. Phases

### Phase 1 — Server color assignment engine

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-001 | Add `colorIndex` field to player object in `GameState.addPlayer()` | | `server/src/game/GameState.js` |
| TASK-002 | Add `deviceColorMap` section to `led-config.json` schema: `{ "<chipId>": colorIndex }` | | `server/data/led-config.json`, `server/src/config/LedConfigManager.js` |
| TASK-003 | Implement `assignColor(chipId, type)` in `LedConfigManager`: lookup chipId → mapped color, else pick lowest unused index, persist mapping for devices | | `server/src/config/LedConfigManager.js` |
| TASK-004 | On device register in `ConnectionManager._handleRegister()`: extract `chipId` from payload, call `assignColor()`, set `player.colorIndex`, include in `registered` response | | `server/src/ws/ConnectionManager.js` |
| TASK-005 | For web client register: assign first color not in `deviceColorMap` values | | `server/src/ws/ConnectionManager.js` |
| TASK-006 | Include `colorIndex` in `GameState.toJSON()` player serialization and `state` broadcasts | | `server/src/game/GameState.js` |
| TASK-007 | Include `deviceColor` (hex string) in `led_config` WS payload sent to devices | | `server/src/ws/ConnectionManager.js` |

### Phase 2 — Server API routes

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-008 | `GET /api/leds/device-colors` — returns `deviceColorMap` and full palette | | `server/src/routes/leds.js` |
| TASK-009 | `PUT /api/leds/device-colors/:chipId` — admin changes a device's color assignment, persists, re-broadcasts `led_config` to that device | | `server/src/routes/leds.js` |

### Phase 3 — ESP firmware

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-010 | Send `chipId` (hex string of `ESP.getChipId()`) in registration payload | | `clients/esp8266-sensor/src/websocket.cpp` |
| TASK-011 | Add `deviceColor` (RgbColor) field to `LedConfig` struct, parse from `led_config` message | | `clients/esp8266-sensor/src/config.h`, `clients/esp8266-sensor/src/websocket.cpp` |
| TASK-012 | Generate random startup color when no server config received yet | | `clients/esp8266-sensor/src/led.cpp` |
| TASK-013 | `_playAmbient()`: use `_config.deviceColor` for WIFI_ONLY and WS_CONNECTED states (NO_WIFI stays red for visibility) | | `clients/esp8266-sensor/src/led.cpp` |
| TASK-014 | `playTestEffect()`: use `_config.deviceColor` as default when test effect has no explicit color | | `clients/esp8266-sensor/src/led.cpp` |

### Phase 4 — Admin UI

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-015 | Add color swatch + dropdown to device cards showing/changing assigned color | | `clients/web/leds.html`, `clients/web/js/led-admin.js` |
| TASK-016 | Populate dropdown from player-colors.json palette | | `clients/web/js/led-admin.js` |
| TASK-017 | On change: `PUT /api/leds/device-colors/:chipId` → update swatch | | `clients/web/js/led-admin.js` |
| TASK-018 | Effect test color dropdown: add "Device Color" as first option (value = device's assigned hex), make it default | | `clients/web/js/led-admin.js` |
| TASK-019 | Include `chipId` in device list from server (already in `_getDeviceList()`) | | `server/src/ws/ConnectionManager.js` |

### Phase 5 — Display & web state sync

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-020 | Display client: use `player.colorIndex` from state instead of array position | | `clients/display/js/main.js`, `clients/display/js/scene/Lane.js` |
| TASK-021 | Web admin state.js: use `player.colorIndex` from state instead of array position | | `clients/web/js/state.js` |

### Phase 6 — Tests

| ID | Task | Status | File(s) |
|----|------|--------|---------|
| TASK-022 | GameState tests: verify colorIndex assignment, toJSON inclusion | | `server/tests/gameState.test.js` |
| TASK-023 | Existing tests pass (no regressions) | | all test files |

## 3. Alternatives

| ID | Option | Pros | Cons | Decision |
|----|--------|------|------|----------|
| ALT-001 | Store color per-player in GameState vs per-device in LedConfig | Simpler | Doesn't survive server restart for devices | **Both**: device mapping persisted in LedConfig, runtime colorIndex on player object |
| ALT-002 | Send full RGB to ESP vs colorIndex | ESP doesn't need palette file | Larger payload, harder to keep in sync | **Hex RGB string** — ESP just needs the color, not the index |

## 4. Dependencies

| ID | Dependency | Required By |
|----|-----------|-------------|
| DEP-001 | `player-colors.json` palette (already exists) | TASK-003, TASK-016 |
| DEP-002 | `LedConfigManager` persistence (already exists) | TASK-002, TASK-003 |
| DEP-003 | `ESP.getChipId()` (ESP8266 SDK) | TASK-010 |

## 5. Files

| ID | File | Action |
|----|------|--------|
| FILE-001 | `server/src/game/GameState.js` | Modify — add colorIndex to player, toJSON |
| FILE-002 | `server/src/config/LedConfigManager.js` | Modify — deviceColorMap, assignColor() |
| FILE-003 | `server/src/ws/ConnectionManager.js` | Modify — chipId handling, color in registered/led_config |
| FILE-004 | `server/src/routes/leds.js` | Modify — device-colors endpoints |
| FILE-005 | `server/data/led-config.json` | Modify — add deviceColorMap |
| FILE-006 | `clients/esp8266-sensor/src/websocket.cpp` | Modify — send chipId, parse deviceColor |
| FILE-007 | `clients/esp8266-sensor/src/config.h` | Modify — deviceColor in LedConfig |
| FILE-008 | `clients/esp8266-sensor/src/led.cpp` | Modify — ambient/default colors |
| FILE-009 | `clients/web/js/led-admin.js` | Modify — color UI in device cards, test dropdown |
| FILE-010 | `clients/web/leds.html` | Modify — color picker markup |
| FILE-011 | `clients/web/js/state.js` | Modify — use colorIndex from state |
| FILE-012 | `clients/display/js/main.js` | Modify — use colorIndex from state |
| FILE-013 | `clients/display/js/scene/Lane.js` | Modify — accept colorIndex from player |
| FILE-014 | `server/tests/gameState.test.js` | Modify — colorIndex tests |

## 6. Testing

| ID | Test | Validates |
|----|------|-----------|
| TEST-001 | `addPlayer()` returns player with `colorIndex` 0-15 | TASK-001 |
| TEST-002 | Device with known chipId gets same color on reconnect | TASK-003 |
| TEST-003 | Web clients get colors not in deviceColorMap | TASK-005 |
| TEST-004 | `GET /api/leds/device-colors` returns map + palette | TASK-008 |
| TEST-005 | `PUT /api/leds/device-colors/:chipId` persists and re-broadcasts | TASK-009 |
| TEST-006 | ESP registration includes chipId | TASK-010 |
| TEST-007 | ESP idle effect uses device color | TASK-013 |
| TEST-008 | All existing 111 tests pass | TASK-023 |

## 7. Risks

| ID | Risk | Mitigation |
|----|------|------------|
| RISK-001 | chipId collision between ESP devices | Extremely unlikely (32-bit unique ID), log warning if detected |
| RISK-002 | All 16 colors exhausted with many devices | Wrap around with modulo, warn in admin UI |
| RISK-003 | Breaking existing display client color rendering | Display change is additive — falls back to index 0 if colorIndex missing |

## 8. References

- `clients/assets/themes/shared/player-colors.json` — 16-color shared palette
- `server/README.md` — WebSocket protocol reference
- `clients/shared/leds/README.md` — LED effect system docs
