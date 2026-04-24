---
applyTo: "clients/esp8266-sensor/**,clients/esp32-motor/**,clients/shared/**,server/**,clients/web/**,clients/display/**"
---

# Logging — How to Add Log Output Across All Stacks

## Architecture at a Glance

```
ESP firmware  ──DERBY_LOG_*──► Serial (always)
                │
                └─► DerbyLogger ──► (queued) ──► WSClient.sendLog()
                                                      │
Node.js server ──console.log/warn/error──► log.js ──►│
                                                      ▼
                                           ConnectionManager.broadcastLog()
                                                      │
                                             web / display clients
                                                      │
                                                      ▼
                                              log-viewer.js (admin/logs page)
```

**Key contract:** native output (Serial / console) is **always called first**. The forwarding layer is additive — it never replaces or suppresses native output.

---

## 1 · ESP8266 Sensor firmware (`clients/esp8266-sensor/`)

Include `<derby_logger.h>` (already transitively included via `websocket.h` in any TU that includes `websocket.h`). For files that don't include `websocket.h`, add it explicitly after the other includes.

### Replace `Serial.printf` → `DERBY_LOG_F`

```cpp
// Before
Serial.printf("[WS] Score sent: playerId=%s points=%d\n", id.c_str(), pts);

// After — Serial is called first, then line is forwarded over WebSocket
DERBY_LOG_F("[WS] Score sent: playerId=%s points=%d\n", id.c_str(), pts);
```

### Replace `Serial.println("literal")` → `DERBY_LOG_LN`

```cpp
// Before
Serial.println("[CFG] No config file — using defaults");

// After
DERBY_LOG_LN("[CFG] No config file — using defaults");
```

### Convert `Serial.println(F("..."))` → `DERBY_LOG_F`

`F()` strings live in PROGMEM (ESP8266 flash) and cannot be passed as `const char*` to `_enqueue`. Convert to a regular format string:

```cpp
// Before (PROGMEM string — cannot be forwarded as-is)
Serial.println(F("[CFG] Serial pre-config window 3 s — send: DERBY_CFG:{...}"));

// After — drop F(), move \n inside the format string
DERBY_LOG_F("[CFG] Serial pre-config window 3 s — send: DERBY_CFG:{...}\n");
```

### Wire the sender in `setup()`

Call `DerbyLogger::setSender(&wsClient)` **once**, after `wsClient.begin(...)`. Do it at every call site where `begin()` is called (initial connect + WiFi reconnect):

```cpp
wsClient.begin(host, port, g_playerName, g_playerId);
DerbyLogger::setSender(&wsClient);   // ← add this line
```

### Do NOT touch

- `Serial.begin(SERIAL_BAUD)` — hardware init, must stay as-is
- `Serial.available()`, `Serial.read()`, `Serial.print()` — I/O methods, not log calls
- Lines inside the serial pre-config input loop

---

## 2 · ESP32 Motor firmware (`clients/esp32-motor/`)

Identical rules to the sensor. The same `derby_logger.h` macros work on both platforms.

```cpp
// motor_calibration.cpp — example
#include <derby_logger.h>

// ...
DERBY_LOG_F("[CALIB] Lane %u: start=%ld end=%ld\n", lane, (long)c.startStep, (long)c.endStep);
DERBY_LOG_LN("[CALIB] Calibration saved");
```

`DerbyLogger::setSender(&wsClient)` must be called after every `wsClient.begin(...)` in `main.cpp` — including the WiFi-reconnect path.

---

## 3 · Shared C++ IO library (`clients/shared/io/`, `clients/shared/leds/`)

Headers in `clients/shared/io/` can include `<derby_logger.h>` directly because the build system adds `clients/shared/io/` to the include path for both firmware projects.

```cpp
// my_feature.h
#pragma once
#include <Arduino.h>
#include <derby_logger.h>

class MyFeature {
public:
    void begin() {
        DERBY_LOG_LN("[MY] Initialised");
    }
};
```

Headers in `clients/shared/leds/` are also compiled by the native (host) unit-test runner which has no Arduino environment. Guard the include with `NATIVE_TEST`:

```cpp
// clients/shared/leds/SomeFeature.h
#ifndef NATIVE_TEST
#  include <derby_logger.h>
#else
#  ifndef DERBY_LOG_F
#    define DERBY_LOG_F(...) ((void)0)
#  endif
#  ifndef DERBY_LOG_LN
#    define DERBY_LOG_LN(...) ((void)0)
#  endif
#endif

void begin() {
    DERBY_LOG_LN("[FEATURE] Initialized");  // no-op in native tests; logs on device
}
```

`GameEventMapper.h` follows this pattern and is the canonical example.

> **Never** include `<derby_logger.h>` from `clients/shared/leds/` headers without the `NATIVE_TEST` guard — the native test runner will fail to compile.

---

## 4 · Node.js server (`server/`)

`server/src/log.js` is required **once** at the top of `server/src/index.js` and intercepts `console.log`, `console.warn`, and `console.error` globally.

**No changes needed** in any other server file. Just use the standard Node.js console API as normal:

```js
// Any server file — works automatically
console.log('[ConnectionManager] Device %s registered', clientId);
console.warn('[LedConfig] Missing topology field — using default');
console.error('[GameState] Unexpected state:', err.message);
```

Lines emitted before the ConnectionManager is wired (startup messages) are buffered in a 200-entry ring and flushed to the admin page once `serverLog.setConnectionManager(cm)` is called.

### Adding a new structured log source in the server

If a module needs to emit log entries with a custom `senderName` (e.g. a future hardware bridge), call `broadcastLog` directly instead of `console.log`:

```js
// inside a method that has access to connectionManager
connectionManager.broadcastLog({
  source:     'my-bridge',
  senderName: 'Bridge',
  senderType: 'server',
  message:    'Custom structured entry',
  ts:         Date.now(),
});
```

---

## 5 · Web admin client (`clients/web/`)

The admin logs page (`/admin/logs`) is driven by `clients/web/js/log-viewer.js`. It connects via `Derby.Connection` and listens for `log_line` WebSocket messages.

**You don't need to write any log-emission code in web JS** — the web client is a consumer only.

If you add a new admin page that also needs to show logs, include `log-viewer.js` after `connection.js`:

```html
<script src="/js/connection.js"></script>
<script src="/js/log-viewer.js"></script>
```

`Derby.LogViewer` initialises automatically on `DOMContentLoaded` when `#log-output` and `#log-filter` are present.

---

## 6 · Display client (`clients/display/`)

The display client connects as type `'display'` and will also receive `log_line` messages (the server broadcasts to both `web` and `display` clients). The display does not currently render logs — this is intentional.

If you add log rendering to the display, handle the `log_line` message type in `clients/display/js/connection.js` — do **not** duplicate `log-viewer.js` into the display bundle.

---

## Rules & Anti-patterns

| ❌ Don't | ✅ Do instead |
|----------|--------------|
| Remove `Serial.printf` / `Serial.println` calls | Replace with the equivalent `DERBY_LOG_*` macro |
| Call `DerbyLogger::_enqueue` or `_fmtEnqueue` directly | Use the `DERBY_LOG_*` macros |
| Guard `DERBY_LOG_*` with `if (_connected)` | The macro handles disconnected state internally (queues the line) |
| Use `Serial.printf` for new log lines in firmware | Write new log lines with `DERBY_LOG_F` from the start |
| Pass an F()-string to `DERBY_LOG_LN` | Convert to a format string and use `DERBY_LOG_F("...\n")` |
| Suppress `console.log` calls in server code | Leave them as-is — log.js intercepts transparently |
| Send raw log strings as custom WS message types | Use the standard `{ type: "log", payload: { message } }` format |

## Queue behaviour (firmware)

When no WebSocket connection is available, `DerbyLogger` keeps the **last 16 lines** in a ring buffer (oldest entry overwritten when full). On connect, `flushQueue()` is called automatically by `WSClientBase::_onEvent(ConnectionOpened)` — you never need to call it manually.

The queue size is tunable per-project by defining `DERBY_LOG_QUEUE_SIZE` before including `derby_logger.h`:

```cpp
// In a project's config.h, before any include of derby_logger.h:
#define DERBY_LOG_QUEUE_SIZE 32
```
