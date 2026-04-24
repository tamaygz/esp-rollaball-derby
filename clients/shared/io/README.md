# Shared IO Library — `clients/shared/io/`

Utility headers shared across all Derby firmware targets (ESP8266 and ESP32). Contains chip identity helpers, colour parsing, and peripheral drivers (status LED, buttons).

## Installation

Add to your `platformio.ini`:

```ini
[env]
build_flags = -I../../shared        ; gives <io/...> and root-level includes
lib_deps =
    symlink://${PROJECT_DIR}/../shared/io
```

## Modules

### `device_info.h` — Chip Identity

Platform-abstracted helpers for reading chip identity at runtime.

```cpp
#include <device_info.h>
```

#### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DERBY_CHIP_ID_HEX_MAX_LEN` | `17` | Max hex string + null (ESP32 canonical size — safe for both targets) |
| `DERBY_CHIP_ID_ESP8266_HEX_LEN` | `9` | 8 hex chars + null (ESP8266 specific) |

#### API

```cpp
const char* derbyChipType()
```

Returns the chip family string: `"ESP8266"`, `"ESP32"`, or `"UNKNOWN"`.

```cpp
uint16_t derbyChipSuffix16()
```

Returns the low 16 bits of the chip ID — useful for short display names or hostname suffixes.

```cpp
void derbyChipIdHex(char* out, size_t outSize)
```

Writes the full chip ID as an uppercase hex string into `out`.

- **ESP8266**: 8-char hex (`"A1B2C3D4"`)
- **ESP32**: 16-char hex (`"A1B2C3D4E5F60708"`)

Always allocate at least `DERBY_CHIP_ID_HEX_MAX_LEN` bytes:

```cpp
char chipId[DERBY_CHIP_ID_HEX_MAX_LEN];
derbyChipIdHex(chipId, sizeof(chipId));
// ESP8266 → "A1B2C3D4", ESP32 → "A1B2C3D4E5F60708"
```

---

### `color_utils.h` — Hex Colour Parsing

Parses `#RRGGBB` colour strings into byte components. Shared by sensor and motor firmware to eliminate duplicate parsing code.

```cpp
#include <color_utils.h>
```

#### API

```cpp
bool derbyParseHexColor(const char* str, uint8_t& r, uint8_t& g, uint8_t& b)
```

Parses a 7-character `#RRGGBB` hex colour string.

- **Returns**: `true` on success; `r`, `g`, `b` are written only on success.
- **Returns**: `false` if `str` is null, missing the `#` prefix, or not exactly 7 chars.

```cpp
uint8_t r, g, b;
if (derbyParseHexColor("#FF8000", r, g, b)) {
    // r=255, g=128, b=0
}
```

---

### `StatusLed.h` / `StatusLed.cpp` — Status LED Driver

Non-blocking status LED controller with blink patterns.

```cpp
#include <io/StatusLed.h>
```

Controls the onboard LED (or external LED) for firmware state indication — boot, WiFi connecting, game events, and errors.

---

### `ButtonManager.h` / `ButtonManager.cpp` — Button Input

Debounced button input with press and hold detection.

```cpp
#include <io/ButtonManager.h>
```

Manages physical buttons with software debounce. Used by the ESP32 motor controller for Start/Reset and Pause/Resume buttons.

---

## `library.json`

PlatformIO library manifest. Fields:

| Field | Value |
|-------|-------|
| `name` | `derby-shared-io` |
| `description` | Shared IO utilities: status LED, buttons, device identity, colour parsing |
| `frameworks` | `arduino` |
| `platforms` | `espressif8266`, `espressif32` |
