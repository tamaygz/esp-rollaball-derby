#pragma once
#include "motor_interface.h"
#include <AccelStepper.h>
#include <Arduino.h>

// 28BYJ-48 stepper motor driven by a ULN2003 darlington transistor array.
// Uses AccelStepper in HALF4WIRE mode (8 microsteps per electrical cycle).
// One full mechanical revolution = 4096 half-steps (8 half-steps × 512 cycles × gear ratio).
//
// Wiring: 4 GPIO pins connected to IN1–IN4 on the ULN2003 board.
// Pin order for AccelStepper HALF4WIRE: motorPin1, motorPin2, motorPin3, motorPin4
// (must match the IN1-IN3-IN2-IN4 interleaved ULN2003 wiring for correct rotation).

class StepperMotor : public MotorInterface {
public:
    // Construct with the four GPIO pins (IN1–IN4) for the ULN2003 driver board.
    // limitSwitchPin: INPUT_PULLUP pin that reads LOW when motor reaches home position.
    //                 Pass 255 to disable limit-switch homing.
    StepperMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4,
                 uint8_t limitSwitchPin = 255);

    bool    begin()           override;
    void    update()          override;
    void    moveTo(int32_t targetStep)   override;
    void    jogSteps(int32_t steps)      override;
    void    home()            override;
    void    setMaxSpeed(float stepsPerSec) override;
    void    setAcceleration(float stepsPerSec2) override;
    void    release()         override;

    bool    isHomed()    const override { return _homed; }
    bool    isMoving()   const override;
    int32_t getCurrentStep() const override;
    int32_t getTargetStep()  const override;

private:
    mutable AccelStepper _stepper;  // mutable: AccelStepper query methods are not const-qualified
    uint8_t      _limitPin;
    bool         _homed           = false;
    bool         _homing          = false;
    bool         _idleSinceMs0    = false;
    unsigned long _idleStartMs    = 0;
    static constexpr unsigned long IDLE_RELEASE_MS = 500; // ms before releasing coils

    void _releaseIfIdle();
};
