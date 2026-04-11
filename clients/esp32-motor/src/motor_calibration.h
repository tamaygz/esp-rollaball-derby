#pragma once
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Arduino.h>

// Per-lane calibration state machine.
// Workflow:
//   beginCalibration(lane) → jog motor to start → setStartPosition(lane)
//   → jog motor to end → setEndPosition(lane) → finishCalibration(lane)
//
// Configuration is persisted to LittleFS (CALIB_FILE) so it survives reboots.
// The motor_manager reads calibrations on boot and applies them before game play.

class MotorCalibration {
public:
    // Initialise — load persisted calibration data from LittleFS.
    bool begin(uint8_t motorCount);

    // Save all calibrations to flash (atomic write via temp file).
    bool save();

    // Per-lane calibration workflow ─────────────────────────────────────────

    // Enter calibration mode for a lane.
    // While in calibration mode, game position updates are suppressed for that lane.
    void beginCalibration(uint8_t lane);

    // Capture the current motor position as the start (home) step for this lane.
    // currentStep is read from MotorManager.
    void setStartPosition(uint8_t lane, int32_t currentStep);

    // Capture the current motor position as the end step; calculate totalTrackSteps.
    void setEndPosition(uint8_t lane, int32_t currentStep);

    // Validate and finalise calibration. Returns false if start == end.
    bool finishCalibration(uint8_t lane);

    // Discard in-progress calibration, restore previous values.
    void cancelCalibration(uint8_t lane);

    // Reset calibration for one lane (or all lanes if lane == 255).
    void resetCalibration(uint8_t lane);

    // Per-lane parameter updates ────────────────────────────────────────────

    void setDirection(uint8_t lane, bool reversed);
    void setSpeed(uint8_t lane, float maxSpeed, float acceleration);
    void setStepsPerMm(uint8_t lane, float stepsPerMm);

    // Full calibration struct update from REST API payload
    void setFromJson(uint8_t lane, JsonObjectConst obj);

    // Getters ───────────────────────────────────────────────────────────────

    // Returns a reference to the calibration for a given lane.
    const LaneCalibration& getLane(uint8_t lane) const;

    bool isCalibrating(uint8_t lane) const { return lane < _motorCount && _calibrating[lane]; }
    bool isCalibrated(uint8_t lane)  const;
    uint8_t motorCount() const { return _motorCount; }

    // Serialize all calibrations to a JsonArray for REST responses
    void toJson(JsonArray& arr) const;

private:
    LaneCalibration _lanes[MOTOR_MAX_LANES];
    LaneCalibration _backup[MOTOR_MAX_LANES]; // pre-calibration backup
    bool            _calibrating[MOTOR_MAX_LANES] = {};
    uint8_t         _motorCount = 0;

    void _defaults(uint8_t lane);
};
