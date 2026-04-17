# LED Effects Creation Guide

Complete checklist for adding a new LED animation across the entire stack without missing pieces. This is the central source of truth after lessons learned from `ballroll` and `camelchew`.

## Overview

A new LED effect requires **changes in 4 independent layers**:

1. **Effect manifest** (`clients/shared/led-effects-manifest.json`) — registered as single source of truth
2. **JavaScript simulator** (`clients/web/js/led-effects.js`) — for admin preview panel
3. **C++ firmware** (`clients/shared/leds/MatrixDisplay.cpp`/`.h` + dispatch in `main.cpp`) — hardware animation
4. **Server validation** (optional, only if using `/api/leds/effects/test` endpoint)

**Common mistake: Implementing only 2–3 layers and wondering why "it doesn't work on device" or "preview is all gray."** Each layer is independent; skipping one breaks the chain.

---

## Layer 1: Effect Manifest

**File:** `clients/shared/led-effects-manifest.json`

**Purpose:** Single source of truth for all platforms (web, C++).

### Example Entry (Camel Chew)

```json
{
  "name": "camelchew",
  "label": "Camel Chew",
  "category": "matrix",
  "platforms": ["motor"],
  "params": {
    "color": { "type": "color", "default": "#7AC520" },
    "speed": { "type": "ms", "default": 2400, "min": 800, "max": 6000 }
  }
}
```

### Manifest Fields

- **`name`** — kebab-case unique identifier, used in code dispatch (`strcmp(name, "camelchew")`)
- **`label`** — human-readable name shown in UI dropdown
- **`category`** — `"strip"` or `"matrix"` (determines UI grouping; matrix effects only usable on motor devices)
- **`platforms`** — array of `["sensor"]`, `["motor"]`, or both (filters availability by device type)
- **`params`** — object of configurable parameters:
  - `color`: user-choosable hex colour (shows colour picker in UI)
  - `speed`: duration in milliseconds (shows slider: range `min` to `max`)
  - **no parameters** — use empty `{}` for effects like "countdown", "winner"

### Checklist for Manifest

- [ ] Effect name is unique (not already in manifest)
- [ ] `platforms` array includes the device where you'll test (usually `["motor"]` for matrix animations)
- [ ] `params` match what the JS simulator and C++ firmware expect
- [ ] Sensible default values and min/max ranges

---

## Layer 2: JavaScript Simulator

**File:** `clients/web/js/led-effects.js`

**Purpose:** Canvas-based animation preview in admin panel at `http://localhost:3000/admin` (LED Configuration > LED Preview section).

### Registration in Effect Registry

At the top of `led-effects.js`, add to the `_effects` object:

```javascript
var _effects = {
  solid:     SolidEffect,
  blink:     BlinkEffect,
  // ... other effects ...
  ballroll:  BallRollEffect,
  camelchew: CamelChewEffect,  // ← NEW
};
```

### Effect Constructor + Prototype

Create a constructor function + `update()` prototype method:

```javascript
function CamelChewEffect(params, api) {
  // params = { color: "#7AC520", speed: 2400 }
  // api = { setPixel, setAll, ledCount, matrixRows, matrixCols, topology }
  
  this.api = api;
  this.rows = api.matrixRows || 8;
  this.cols = api.matrixCols || 8;
  this.topology = api.topology || 'matrix_zigzag';
  
  // Colour palette
  this.C_FUR = { r: 200, g: 140, b: 55 };
  this.C_HAY = hexToRgb(params.color || '#7AC520');  // user-configurable
  
  // Animation state
  this.chewCycle = Math.max(800, params.speed || 2400);
  this.chewTime = 0;
  this.blinkTimer = 2500 + Math.random() * 2500;
  this.blinkElapsed = 0;
}

CamelChewEffect.prototype.update = function (deltaTime) {
  this.chewTime = (this.chewTime + deltaTime) % this.chewCycle;
  
  // Compute animation frame based on elapsed time
  var rawT = this.chewTime / this.chewCycle;
  var mouthOpen = this._computeMouthCurve(rawT);
  
  // Draw frame using api.setPixel(index, {r, g, b})
  this.api.setAll({ r: 0, g: 0, b: 0 });  // clear
  this.api.setPixel(0, { r: 255, g: 255, b: 255 });  // example: light first LED
};

// Helper: matrix pixel index conversion
CamelChewEffect.prototype._pixelIndex = function (row, col) {
  return _matrixPixelIdx(Math.round(row), Math.round(col), this.rows, this.cols, this.topology);
};

// Helper: set one matrix cell
CamelChewEffect.prototype._setMPixel = function (row, col, color) {
  var idx = this._pixelIndex(row, col);
  if (idx >= 0 && idx < this.api.ledCount) {
    this.api.setPixel(idx, color);
  }
};
```

### Simulator API Contract

The `api` object passed to the effect provides:

```javascript
{
  setPixel: function(index, {r, g, b}),  // set single LED by 0-indexed position
  setAll: function({r, g, b}),           // fill all LEDs with one colour
  ledCount: number,                       // total LED count
  matrixRows: number,                     // grid height (for matrix effects)
  matrixCols: number,                     // grid width (for matrix effects)
  topology: string,                       // 'matrix_zigzag' | 'matrix_progressive' | 'strip' | 'ring'
}
```

### Helper: `_matrixPixelIdx(row, col, rows, cols, topology)`

Already defined in `led-effects.js`. Use this to convert `(row, col)` → linear LED index, accounting for zigzag wiring:

```javascript
// Example:
var ledIndex = _matrixPixelIdx(2, 3, 8, 8, 'matrix_zigzag');
api.setPixel(ledIndex, { r: 255, g: 0, b: 0 });
```

### Checklist for JS Simulator

- [ ] Effect registered in `_effects` object
- [ ] Constructor accepts `params` (from manifest) and `api` (simulator context)
- [ ] `update(deltaTime)` called ~60 Hz, animates based on elapsed time
- [ ] Uses `this.api.setPixel()` or `api.setAll()` to draw (never direct array access)
- [ ] Loops smoothly: animation time wraps with modulo (`this.time % this.cycleDuration`)
- [ ] Respects matrix topology when converting `(row, col)` → linear index
- [ ] Colours use RGB values (0–255), handle alpha blending if needed

---

## Layer 3: C++ Firmware (Hardware Animation)

**Files:**
- `clients/shared/leds/MatrixDisplay.h` — declarations + state variables
- `clients/shared/leds/MatrixDisplay.cpp` — implementations
- `clients/esp32-motor/src/main.cpp` — WebSocket dispatch

### 3a. Header Declarations

In `MatrixDisplay.h`, add:

```cpp
public:
  // New effect method
  void showCamelChew(uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs = 2400);

private:
  // New step method for animation loop
  void _stepCamelChew();

  // State variables (initialization: `= {}`或 `= 0` etc.)
  uint8_t       _ccHayR        = 120;
  uint8_t       _ccHayG        = 197;
  uint8_t       _ccHayB        = 32;
  uint16_t      _ccChewCycleMs = 2400;
  unsigned long _ccChewStartMs = 0;
  unsigned long _ccBlinkTimer  = 0;
  unsigned long _ccEarTimer    = 0;
  uint8_t       _ccEarSide     = 0;
```

Also update the `Mode` enum to include the new effect:

```cpp
enum class Mode { IDLE, STATIC, NUMBER, SCROLLING, WINNER, BLINK, PULSE, CHASE, SPARKLE, BALLROLL, CAMELCHEW };
```

### 3b. Mode Loop Dispatch

In `MatrixDisplay.cpp`, in the `loop()` method's switch statement, add:

```cpp
case Mode::CAMELCHEW:
  _stepCamelChew();
  break;
```

### 3c. Show Method Implementation

Implement `showCamelChew()` in `.cpp`:

```cpp
void MatrixDisplay::showCamelChew(
    uint8_t r, uint8_t g, uint8_t b, uint16_t speedMs)
{
    if (!_available || !_strip) return;

    _ccHayR        = r;
    _ccHayG        = g;
    _ccHayB        = b;
    _ccChewCycleMs = speedMs > 0 ? speedMs : 2400;
    _ccChewStartMs = millis();

    // Initialize random timers for blink & ear flicks
    _ccBlinkTimer = millis() + 2500 + random(0, 2500);
    _ccEarTimer   = millis() + 3500 + random(0, 3500);

    _strip->ClearTo(RgbColor(0));
    _strip->Show();
    _mode = Mode::CAMELCHEW;
}
```

### 3d. Step Method (Animation Loop)

Implement `_stepCamelChew()` in `.cpp`:

```cpp
void MatrixDisplay::_stepCamelChew() {
    if (!_strip) return;

    unsigned long now = millis();
    if (now - _animLastStepMs < 16) return;  // ~60 fps cap
    _animLastStepMs = now;

    // Compute chew phase (0.0 → 1.0)
    unsigned long cycleElapsed = (now - _ccChewStartMs) % _ccChewCycleMs;
    float rawT = (float)cycleElapsed / (float)_ccChewCycleMs;

    // Phase curve: 4-phase chew cycle
    float mouthOpen;
    if (rawT < 0.38f) {
        mouthOpen = 0.0f;                                    // closed
    } else if (rawT < 0.56f) {
        float t0 = (rawT - 0.38f) / 0.18f;
        mouthOpen = t0 * t0;                                 // ease-in opening
    } else if (rawT < 0.84f) {
        float t1 = (rawT - 0.56f) / 0.28f;
        mouthOpen = 0.85f + sinf(t1 * M_PI * 3.0f) * 0.15f;  // oscillate (chewing)
    } else {
        float t2 = (rawT - 0.84f) / 0.16f;
        mouthOpen = (1.0f - t2) * (1.0f - t2) * 0.85f;       // ease-out closing
    }

    // Blink logic
    bool blinking = false;
    if (now >= _ccBlinkTimer) {
        unsigned long blinkAge = now - _ccBlinkTimer;
        if (blinkAge < 120) blinking = true;
        else _ccBlinkTimer = now + 2500 + random(0, 3000);
    }

    // Ear flick logic
    uint8_t earFlickSide = 3;  // 3 = no flick
    if (now >= _ccEarTimer) {
        unsigned long earAge = now - _ccEarTimer;
        if (earAge < 220) {
            earFlickSide = _ccEarSide;
        } else {
            _ccEarSide = (uint8_t)(random(0, 3));
            _ccEarTimer = now + 3500 + random(0, 4000);
        }
    }

    // Clear and begin drawing
    _strip->ClearTo(RgbColor(200, 140, 55));  // base fur

    // Draw eyes, ears, jaw, etc. using _setPixel(row, col, r, g, b)
    // ...
    
    _strip->Show();
}
```

### 3e. WebSocket Dispatch in main.cpp

In `main.cpp`, in the WebSocket test effect handler, add:

```cpp
} else if (strcmp(pendingEffect.effectName, "camelchew") == 0) {
    matrixDisplay.showCamelChew(pendingEffect.r, pendingEffect.g, pendingEffect.b,
        pendingEffect.speedMs > 0 ? pendingEffect.speedMs : 2400);
```

### C++ Key Patterns

- **Fixed-point math**: Use `int32_t x << 8` (shift left 8 bits) instead of `float` for smoother interpolation on embedded systems.
- **Phase wrapping**: `(now - startTime) % cycleDuration` resets phase cleanly.
- **Colour interpolation**: `(uint8_t)lerp(a, b, t)` for smooth transitions.
- **Matrix topology**: Use `_setPixel(row, col, r, g, b)` helper; it handles zigzag internally.
- **Frame rate capping**: Check `now - _animLastStepMs < 16` before updating (prevents CPU waste, ensures smooth 60 fps).

### Checklist for C++ Firmware

- [ ] New state variables added to `.h` with sensible defaults
- [ ] `Mode` enum includes the new effect
- [ ] `loop()` switch statement has new case calling `_stepXXX()`
- [ ] `showXXX()` method initializes state and sets `_mode = Mode::XXX`
- [ ] `_stepXXX()` method animates and calls `_strip->Show()` sparingly (not every 1ms)
- [ ] WebSocket dispatch in `main.cpp` calls `matrixDisplay.showXXX()` with right params
- [ ] No hardcoded LED indices — use `_setPixel()` + topology awareness
- [ ] Effect loops seamlessly (re-initializes phase when complete)

---

## Layer 4: Server Validation (Optional)

**File:** `server/src/routes/leds.js`

If you want the server's `/api/leds/effects/test` endpoint to validate the effect before sending to device, add an entry to `_Effects` object:

```javascript
const _Effects = {
  solid: { validate: true, category: 'strip' },
  ballroll: { validate: true, category: 'matrix' },
  camelchew: { validate: true, category: 'matrix' },  // ← NEW
};
```

The validation ensures the effect name exists in the manifest before dispatching to hardware.

### Checklist for Server

- [ ] Effect name added to server's `_Effects` object (if using test endpoint)
- [ ] Category matches manifest

---

## Complete Checklist: Creating a New LED Effect

Use this to verify all pieces are in place **before** testing:

### Manifest
- [ ] Entry added to `clients/shared/led-effects-manifest.json`
- [ ] `name`, `label`, `category`, `platforms` filled correctly
- [ ] `params` object (colour, speed, etc.) matches JS + C++ expectations

### JavaScript Simulator
- [ ] Constructor function created
- [ ] Registered in `_effects` object
- [ ] `update(deltaTime)` method implemented
- [ ] Uses `_matrixPixelIdx()` for matrix topology conversion
- [ ] Loops smoothly (time wraps with modulo)
- [ ] Color parameters respected from manifest

### C++ Firmware
- [ ] State variables declared in `.h`
- [ ] `Mode` enum includes new effect
- [ ] `show()` public method in `.h` and `.cpp`
- [ ] `_step()` private method in `.h` and `.cpp`
- [ ] Animation loop in `loop()` switch calls `_step()`
- [ ] WebSocket dispatcher in `main.cpp` calls `show()`
- [ ] Builds without linker errors (all headers included)

### Server (Optional)
- [ ] Effect added to `_Effects` validation object

### Integration Testing

**Step 1: Test simulator in browser**
- Open `http://localhost:3000/admin`
- Select motor device
- Choose effect from dropdown
- Verify preview animates smoothly

**Step 2: Test on hardware**
- Click "Test on Device"
- ESP32 should run the animation
- Device should reflect all parameters (colour, speed)

**Step 3: Verify deployment**
- Commit all 4 layers together
- Test again after pull request

---

## Troubleshooting

| Symptom | Root Cause | Fix |
|---------|-----------|-----|
| Dropdown doesn't show effect | Missing manifest entry | Add effect to `led-effects-manifest.json` |
| Preview shows solid gray/white | JS effect using white as fallback | Set proper default colour in `params.color` |
| Preview never renders | JS constructor crash or not registered | Check `_effects` registry; ensure `update()` is called |
| Device doesn't animate | Missing C++ dispatch or `show()` not called | Verify `main.cpp` has strcmp + call to `show()` |
| Animation freezes | `_step()` method has infinite loop or blocks | Use frame-rate cap (`now - _lastMs < 16`) |
| Colors inverted/wrong | Manifest params don't match C++ expectations | Verify colour field name is `color` (manifest) → `params.color` (JS/C++) |
| Linker errors ("undefined reference to...") | Shared library includes not resolved | Ensure shim headers use **non-namespaced** includes (`#include <StatusLed.h>` not `#include <leds/StatusLed.h>`) |

---

## Reference: Ballroll Effect (Complete Example)

See `clients/web/js/led-effects.js` (`BallRollEffect`) and `clients/shared/leds/MatrixDisplay.cpp` (`showBallRoll()`, `_stepBallRoll()`) for a fully-implemented 4-layer effect.

---

## Key Lessons

1. **Manifest is the contract**: All teams (web JS, C++ firmware) read the same source of truth.
2. **Simulator must exist**: Without JS preview, you won't know if the animation is any good until it's on hardware.
3. **Include paths matter**: Shim headers must use direct includes (`#include <HeaderName.h>`) for PlatformIO's LDF to find libraries.
4. **Test in order**: (1) browser preview, (2) hardware test, (3) production.
5. **Defaults are critical**: If device has no assigned colour, fallback to first player colour or a sensible brand colour (not white).
6. **Loop smoothly**: Use modulo wrapping and re-initialize phase when complete to avoid glitches.

