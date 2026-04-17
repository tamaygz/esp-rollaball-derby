#pragma once
#include <Arduino.h>

// Callback type: (buttonIndex 1-based, actionString)
using ButtonCallback = void(*)(uint8_t, const char*);

/**
 * ButtonManager — Debounced multi-button controller.
 *
 * Handles up to MAX_BUTTONS physical push-buttons (active-low INPUT_PULLUP).
 * Pin numbers and button count are supplied at begin() time so the class
 * has no dependency on any client's config.h.
 *
 * Action mapping (two-button game-control convention):
 *   Button 1: "start"  when idle / "reset" otherwise
 *   Button 2: "pause"  when running / "resume" when paused
 *
 * Usage:
 *   const uint8_t pins[] = { PIN_BUTTON_1, PIN_BUTTON_2 };
 *   buttons.begin(pins, 2, onPress, BUTTON_DEBOUNCE_MS);
 *   // in loop():
 *   buttons.loop();
 */
class ButtonManager {
public:
    static constexpr uint8_t MAX_BUTTONS = 8;

    // Initialise buttons.
    // pins:       array of GPIO pin numbers (INPUT_PULLUP, active-low)
    // count:      number of entries in pins[] (clamped to MAX_BUTTONS)
    // onPress:    callback fired with (1-based index, action string) on debounced press
    // debounceMs: minimum ms between valid transitions (default 200 ms)
    bool begin(const uint8_t* pins, uint8_t count, ButtonCallback onPress,
               unsigned long debounceMs = 200);

    // Poll button state.  Call every loop() iteration.
    void loop();

    bool isAvailable() const { return _available; }

    // Allow external code to override the game status used for action mapping.
    // Valid values: "idle", "countdown", "running", "paused", "finished"
    void setGameStatus(const char* status);

private:
    uint8_t        _count        = 0;
    bool           _available    = false;
    ButtonCallback _callback     = nullptr;
    unsigned long  _debounceMs   = 200;

    uint8_t        _pins[MAX_BUTTONS]         = {};
    bool           _lastState[MAX_BUTTONS]    = {};
    unsigned long  _lastChangeMs[MAX_BUTTONS] = {};
    char           _gameStatus[16]            = "idle";

    const char* _actionForButton(uint8_t idx) const;
};
