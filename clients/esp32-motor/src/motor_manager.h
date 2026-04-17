#pragma once
#include "motor_interface.h"
#include "motor_calibration.h"
#include "stepper_motor.h"
#include "config.h"
#include <Arduino.h>

// Motor status snapshot for REST API / registration capabilities
struct MotorStatus {
    uint8_t  lane;
    int32_t  currentStep;
    int32_t  targetStep;
    bool     isMoving;
    bool     isHomed;
};

// Manages 1–MOTOR_MAX_LANES stepper motors:
//  - Owns a MotorCalibration that persists lane configs to LittleFS
//  - Maps game position [0.0–1.0] to calibrated step range per lane
//  - Orchestrates homing sequence on boot / game reset
//  - Delegates jog / moveTo / home REST commands
class MotorManager {
public:
    // Call once in setup(). motorCount = number of active motor lanes.
    bool begin(uint8_t motorCount = MOTOR_MAX_LANES);

    // Non-blocking update — call every loop()
    void loop();

    // Map normalised game positions [0.0–1.0] per player to motor target steps.
    // positions[i] is the normalised position for lane i.
    void setPositions(const float* positions, uint8_t count);

    // Home operations
    void homeAll();
    void homeLane(uint8_t lane);

    // Jog a lane by N relative steps (admin calibration use).
    void jogLane(uint8_t lane, int32_t steps);

    // Move a lane to an absolute step position.
    void moveLaneTo(uint8_t lane, int32_t step);

    // Move a lane to a normalised [0.0, 1.0] position.
    void moveLaneToNormalized(uint8_t lane, float normalizedPos);

    // ── Queries ──────────────────────────────────────────────────────────────
    MotorStatus getStatus(uint8_t lane) const;

    // Current step position for a lane (used by calibration workflow)
    int32_t currentStep(uint8_t lane) const;

    float   getLaneNormalisedPosition(uint8_t lane) const;
    bool    isLaneCalibrated(uint8_t lane) const;
    bool    isLaneHomed(uint8_t lane)      const;
    bool    isLaneMoving(uint8_t lane)     const;

    uint8_t laneCount()   const { return _count; }
    uint8_t motorCount()  const { return _count; }  // alias
    bool    isAvailable() const { return _available; }
    bool    allHomed()    const;

    // Access the shared MotorCalibration object for all lanes.
    MotorCalibration& calibration() { return _calibration; }

private:
    StepperMotor*    _motors[MOTOR_MAX_LANES] = {};
    MotorCalibration _calibration;             // owned; persists to LittleFS
    uint8_t          _count     = 0;
    bool             _available = false;

    int32_t _mapPosition(uint8_t lane, float position) const;
};
