#pragma once
#include <stdint.h>

// Abstract motor interface.
// Concrete implementations: StepperMotor (28BYJ-48 + ULN2003 via AccelStepper),
// ServoMotor (future), StepperNema (NEMA-17 with STEP+DIR driver, future).
class MotorInterface {
public:
    virtual ~MotorInterface() = default;

    // Initialise hardware and set default motion parameters.
    // Returns false if hardware is not present / misconfigured.
    virtual bool begin() = 0;

    // Non-blocking step update — call every loop() iteration.
    virtual void update() = 0;

    // Move to an absolute step position (non-blocking; subsequent update() calls execute it).
    virtual void moveTo(int32_t targetStep) = 0;

    // Move by a relative number of steps (positive = forward, negative = backward).
    virtual void jogSteps(int32_t steps) = 0;

    // Home the motor: drive toward limit switch until triggered, then zero the position.
    // If no limit switch configured, simply set current position to 0.
    virtual void home() = 0;

    // Set motion speed parameters (used for all subsequent moves).
    virtual void setMaxSpeed(float stepsPerSec) = 0;
    virtual void setAcceleration(float stepsPerSec2) = 0;

    // De-energise coils to save power / reduce heat when idle.
    virtual void release() = 0;

    // State queries
    virtual bool    isHomed()    const = 0;
    virtual bool    isMoving()   const = 0;
    virtual int32_t getCurrentStep() const = 0;
    virtual int32_t getTargetStep()  const = 0;
};
