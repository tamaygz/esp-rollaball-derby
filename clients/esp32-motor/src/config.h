#pragma once

// ─── Pin Definitions ──────────────────────────────────────────────────────────
// Status LED (onboard LED on most ESP32 DevKit boards)
#define PIN_STATUS_LED      2     // GPIO2 onboard LED (active-high on most boards)

// LED matrix (WS2812B strip/matrix)
#define PIN_LED_MATRIX      4     // GPIO4 — data pin for WS2812B matrix

// Control buttons (INPUT_PULLUP — press = LOW)
#define PIN_BUTTON_1        34    // GPIO34 — Start/Reset button (input-only pin)
#define PIN_BUTTON_2        35    // GPIO35 — Pause/Resume button (input-only pin)

// ─── Motor Pin Layout ─────────────────────────────────────────────────────────
// 28BYJ-48 stepper via ULN2003: 4 GPIO pins per motor (IN1–IN4)
// Up to 8 motors; default config has 4.
// Pins chosen to avoid: strapping pins (0, 2, 15), UART (1, 3), SPI flash (6–11).

#define MOTOR_MAX_LANES     8

// Each motor: { IN1, IN2, IN3, IN4 }
#define MOTOR_0_PINS  { 16, 17, 18, 19 }
#define MOTOR_1_PINS  { 21, 22, 23, 25 }
#define MOTOR_2_PINS  { 26, 27, 32, 33 }
#define MOTOR_3_PINS  { 12, 13, 14, 15 }
// Motors 4–7 require additional expansion (I2C GPIO expander or dedicated ESP32 board)
#define MOTOR_4_PINS  { 0, 0, 0, 0 }
#define MOTOR_5_PINS  { 0, 0, 0, 0 }
#define MOTOR_6_PINS  { 0, 0, 0, 0 }
#define MOTOR_7_PINS  { 0, 0, 0, 0 }

// Limit switch pins (INPUT_PULLUP), one per lane
// Set to 255 to disable limit switch for a lane (homing will use step count only)
#define LIMIT_SWITCH_PINS { 36, 39, 255, 255, 255, 255, 255, 255 }

// Buzzer fallback pin (PWM tone via ledcWriteTone) — set 255 to disable
#define BT_FALLBACK_PIN    255

// ─── 28BYJ-48 Motor Constants ─────────────────────────────────────────────────
// Half-step mode: 8 steps/cycle × 64 gear ratio × 8 half-steps/mechanical step
// Full mechanical revolution = 4096 half-steps (AccelStepper HALF4WIRE)
#define STEPPER_STEPS_PER_REV   4096

// Default motion parameters (can be overridden per lane via calibration)
#define STEPPER_DEFAULT_MAX_SPEED    800.0f   // half-steps/second (~0.2 rev/s)
#define STEPPER_DEFAULT_ACCELERATION 400.0f   // half-steps/s²

// ─── Buttons ──────────────────────────────────────────────────────────────────
#define BUTTON_DEBOUNCE_MS  200UL
#define BUTTON_COUNT        2

// ─── WebSocket ────────────────────────────────────────────────────────────────
#define WS_PATH            "/"
#define WS_BACKOFF_MIN_MS  1000UL
#define WS_BACKOFF_MAX_MS  30000UL

// ─── HTTP Config Server ───────────────────────────────────────────────────────
#define HTTP_CONFIG_PORT  80

// ─── WiFiManager AP ───────────────────────────────────────────────────────────
#define WIFIMANAGER_AP_PREFIX  "Derby-Motor-"

// ─── LittleFS ─────────────────────────────────────────────────────────────────
#define CONFIG_FILE       "/config.json"
#define STATE_FILE        "/state.json"
#define STATE_TMP         "/state.tmp"
#define CALIB_FILE        "/calib.json"
#define CALIB_TMP         "/calib.tmp"

// ─── Serial ───────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ─── LED Strip Defaults ───────────────────────────────────────────────────────
#define LED_DEFAULT_COUNT       64    // 8×8 WS2812B matrix
#define LED_DEFAULT_PIN         PIN_LED_MATRIX
#define LED_DEFAULT_BRIGHTNESS  64    // 0-255 (25% — safe default for matrix)

// ─── LED Config ──────────────────────────────────────────────────────────────
// LedTopology, LedConfig and ledConfigDefaults() live in the shared leds lib.
// LED_DEFAULT_* are defined above so the shared header picks them up.
// Define override guard BEFORE including to suppress the shared default.
#define LEDCONFIG_DEFAULTS_OVERRIDE
#include <leds/LedConfig.h>

// ESP32 motor default topology is MATRIX_ZIGZAG — override the shared default.
inline LedConfig ledConfigDefaults() {
    LedConfig cfg;
    cfg.ledCount       = LED_DEFAULT_COUNT;
    cfg.pin            = LED_DEFAULT_PIN;
    cfg.brightness     = LED_DEFAULT_BRIGHTNESS;
    cfg.topology       = LedTopology::MATRIX_ZIGZAG;
    cfg.matrixRows     = 8;
    cfg.matrixCols     = 8;
    cfg.deviceColorR   = 0;
    cfg.deviceColorG   = 0;
    cfg.deviceColorB   = 0;
    cfg.hasDeviceColor = false;
    return cfg;
}

// ─── Motor Lane Calibration Config ────────────────────────────────────────────
struct LaneCalibration {
    uint8_t  laneId;
    int32_t  startStep;             // Absolute step at track start
    int32_t  endStep;               // Absolute step at track end
    int32_t  totalTrackSteps;       // abs(endStep - startStep)
    float    stepsPerMm;            // 0 = unconfigured
    bool     directionReversed;     // true = invert motor direction
    float    maxSpeed;              // steps/s
    float    acceleration;          // steps/s²
    bool     calibrated;            // true once start+end saved & validated
};

inline LaneCalibration laneCalibrationDefaults(uint8_t laneId) {
    LaneCalibration c;
    c.laneId           = laneId;
    c.startStep        = 0;
    c.endStep          = STEPPER_STEPS_PER_REV;  // 1 full revolution as safe default
    c.totalTrackSteps  = STEPPER_STEPS_PER_REV;
    c.stepsPerMm       = 0.0f;
    c.directionReversed = false;
    c.maxSpeed         = STEPPER_DEFAULT_MAX_SPEED;
    c.acceleration     = STEPPER_DEFAULT_ACCELERATION;
    c.calibrated       = false;
    return c;
}

// ─── Sound Config ─────────────────────────────────────────────────────────────
// Sound file names hosted on the game server under /assets/sounds/
#define SOUND_FILE_COUNTDOWN_TICK   "countdown-tick.wav"
#define SOUND_FILE_COUNTDOWN_GO     "countdown-go.wav"
#define SOUND_FILE_SCORE_1          "score-1.wav"
#define SOUND_FILE_SCORE_2          "score-2.wav"
#define SOUND_FILE_SCORE_3          "score-3.wav"
#define SOUND_FILE_SCORE_0          "score-0.wav"
#define SOUND_FILE_WINNER           "winner.wav"
#define SOUND_FILE_BUTTON_CLICK     "button-click.wav"
#define SOUND_FILE_BECAME_LAST      "became-last.wav"
#define SOUND_FILE_STREAK_ZERO      "streak-zero.wav"
#define SOUND_FILE_STREAK_THREE     "streak-three.wav"
#define SOUND_FILE_TOOK_LEAD        "took-lead.wav"
#define SOUND_FILE_GAME_STARTED     "game-started.wav"
#define SOUND_FILE_GAME_PAUSED      "game-paused.wav"
#define SOUND_FILE_GAME_RESUMED     "game-resumed.wav"
#define SOUND_FILE_GAME_RESET       "game-reset.wav"
#define SOUND_FILE_DRAW             "draw.wav"

// ─── Motor lane color indices ──────────────────────────────────────────────────
// Map lane index → Derby color palette index (0–15).
// Edit to match physical hardware wiring/paint.
// Default: lanes 0–3 match first 4 palette colors.
#define DEFAULT_MOTOR_COLORS { 0, 1, 2, 3, 4, 5, 6, 7 }
