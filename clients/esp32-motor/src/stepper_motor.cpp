#include "stepper_motor.h"
#include <Arduino.h>
#include <derby_logger.h>

// AccelStepper HALF4WIRE constructor expects pins in the order:
// motorPin1=IN1, motorPin3=IN2, motorPin2=IN3, motorPin4=IN4
// (interleaved sequence required for correct half-step rotation of 28BYJ-48)
StepperMotor::StepperMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4,
                            uint8_t limitSwitchPin)
    : _stepper(AccelStepper::HALF4WIRE, in1, in3, in2, in4, true)
    , _limitPin(limitSwitchPin)
{}

bool StepperMotor::begin() {
    _stepper.setMaxSpeed(800.0f);
    _stepper.setAcceleration(400.0f);
    _stepper.setCurrentPosition(0);

    if (_limitPin != 255) {
        pinMode(_limitPin, INPUT_PULLUP);
    }

    DERBY_LOG_F("[MOTOR] StepperMotor ready, lim_pin=%u\n", (unsigned)_limitPin);
    return true;
}

void StepperMotor::update() {
    if (_homing) {
        // Drive slowly toward the limit switch
        if (_limitPin != 255 && digitalRead(_limitPin) == LOW) {
            // Triggered: zero position here
            _stepper.stop();
            _stepper.setCurrentPosition(0);
            _homing = false;
            _homed  = true;
            DERBY_LOG_LN("[MOTOR] Homed via limit switch");
            release();
        } else {
            _stepper.runSpeed();   // constant-speed run toward negative direction
        }
        return;
    }

    _stepper.run();
    _releaseIfIdle();
}

void StepperMotor::moveTo(int32_t targetStep) {
    _homing = false;
    _stepper.moveTo(targetStep);
    _idleSinceMs0 = false;
}

void StepperMotor::jogSteps(int32_t steps) {
    _homing = false;
    _stepper.move(steps);
    _idleSinceMs0 = false;
}

void StepperMotor::home() {
    if (_limitPin != 255) {
        // Jog slowly in negative direction until limit switch triggers
        _homing = true;
        _homed  = false;
        _stepper.setSpeed(-300.0f);  // slow crawl toward home
        DERBY_LOG_LN("[MOTOR] Homing via limit switch");
    } else {
        // No limit switch — simply call current position 0
        _stepper.stop();
        _stepper.setCurrentPosition(0);
        _homed = true;
        DERBY_LOG_LN("[MOTOR] Homed (no limit switch — zeroed position)");
        release();
    }
}

void StepperMotor::setMaxSpeed(float stepsPerSec) {
    _stepper.setMaxSpeed(stepsPerSec);
}

void StepperMotor::setAcceleration(float stepsPerSec2) {
    _stepper.setAcceleration(stepsPerSec2);
}

void StepperMotor::release() {
    // De-energise all coils to reduce heat when holding position
    _stepper.disableOutputs();
    _idleSinceMs0 = false;
}

bool StepperMotor::isMoving() const {
    return _homing || (_stepper.distanceToGo() != 0);
}

int32_t StepperMotor::getCurrentStep() const {
    return static_cast<int32_t>(_stepper.currentPosition());
}

int32_t StepperMotor::getTargetStep() const {
    return static_cast<int32_t>(_stepper.targetPosition());
}

void StepperMotor::_releaseIfIdle() {
    if (_stepper.distanceToGo() != 0) {
        // Motor is still moving — keep coils energised
        _idleSinceMs0 = false;
        _stepper.enableOutputs();
        return;
    }

    // Motor has reached its target; start idle timer
    if (!_idleSinceMs0) {
        _idleSinceMs0  = true;
        _idleStartMs   = millis();
    } else if (millis() - _idleStartMs >= IDLE_RELEASE_MS) {
        _stepper.disableOutputs();
    }
}
