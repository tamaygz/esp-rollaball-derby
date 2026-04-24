#include "motor_manager.h"
#include <Arduino.h>
#include <derby_logger.h>

// Pin layout: MOTOR_N_PINS macros expand to uint8_t[4] { in1, in2, in3, in4 }
static const uint8_t MOTOR_PINS[MOTOR_MAX_LANES][4] = {
    MOTOR_0_PINS,
    MOTOR_1_PINS,
    MOTOR_2_PINS,
    MOTOR_3_PINS,
    MOTOR_4_PINS,
    MOTOR_5_PINS,
    MOTOR_6_PINS,
    MOTOR_7_PINS,
};

static const uint8_t LIMIT_PINS[MOTOR_MAX_LANES] = LIMIT_SWITCH_PINS;

bool MotorManager::begin(uint8_t motorCount) {
    _count = min(motorCount, (uint8_t)MOTOR_MAX_LANES);

    if (_count == 0) {
        DERBY_LOG_LN("[MOTOR_MGR] motorCount=0 — motor subsystem inactive");
        _available = false;
        return false;
    }

    // Load calibration data from LittleFS
    _calibration.begin(_count);

    for (uint8_t i = 0; i < _count; ++i) {
        if (MOTOR_PINS[i][0] == 0) {
            DERBY_LOG_F("[MOTOR_MGR] Lane %u: no pins defined, skipping\n", i);
            continue;
        }

        _motors[i] = new StepperMotor(
            MOTOR_PINS[i][0], MOTOR_PINS[i][1],
            MOTOR_PINS[i][2], MOTOR_PINS[i][3],
            LIMIT_PINS[i]
        );

        if (!_motors[i]->begin()) {
            DERBY_LOG_F("[MOTOR_MGR] Lane %u: begin() failed\n", i);
            delete _motors[i];
            _motors[i] = nullptr;
            continue;
        }

        // Apply calibrated speed/accel
        const LaneCalibration& cal = _calibration.getLane(i);
        _motors[i]->setMaxSpeed(cal.maxSpeed);
        _motors[i]->setAcceleration(cal.acceleration);
    }

    _available = true;
    DERBY_LOG_F("[MOTOR_MGR] Initialised: %u motors\n", _count);
    homeAll();
    return true;
}

void MotorManager::loop() {
    if (!_available) return;
    for (uint8_t i = 0; i < _count; ++i) {
        if (_motors[i]) _motors[i]->update();
    }
}

void MotorManager::setPositions(const float* positions, uint8_t count) {
    if (!_available) return;
    uint8_t n = min(count, _count);
    for (uint8_t i = 0; i < n; ++i) {
        if (!_motors[i]) continue;
        if (_calibration.isCalibrating(i)) continue;
        float pos = constrain(positions[i], 0.0f, 1.0f);
        _motors[i]->moveTo(_mapPosition(i, pos));
    }
}

void MotorManager::moveLaneToNormalized(uint8_t lane, float normalizedPos) {
    if (lane < _count && _motors[lane]) {
        float pos = constrain(normalizedPos, 0.0f, 1.0f);
        _motors[lane]->moveTo(_mapPosition(lane, pos));
    }
}

void MotorManager::homeAll() {
    for (uint8_t i = 0; i < _count; ++i) {
        if (_motors[i]) _motors[i]->home();
    }
}

void MotorManager::homeLane(uint8_t lane) {
    if (lane < _count && _motors[lane]) _motors[lane]->home();
}

void MotorManager::jogLane(uint8_t lane, int32_t steps) {
    if (lane < _count && _motors[lane]) {
        const LaneCalibration& cal = _calibration.getLane(lane);
        if (cal.directionReversed) steps = -steps;
        _motors[lane]->jogSteps(steps);
    }
}

void MotorManager::moveLaneTo(uint8_t lane, int32_t step) {
    if (lane < _count && _motors[lane]) _motors[lane]->moveTo(step);
}

MotorStatus MotorManager::getStatus(uint8_t lane) const {
    MotorStatus s = { lane, 0, 0, false, false };
    if (lane < _count && _motors[lane]) {
        s.currentStep = _motors[lane]->getCurrentStep();
        s.targetStep  = _motors[lane]->getTargetStep();
        s.isMoving    = _motors[lane]->isMoving();
        s.isHomed     = _motors[lane]->isHomed();
    }
    return s;
}

bool MotorManager::allHomed() const {
    for (uint8_t i = 0; i < _count; ++i) {
        if (_motors[i] && !_motors[i]->isHomed()) return false;
    }
    return true;
}

int32_t MotorManager::currentStep(uint8_t lane) const {
    if (lane < _count && _motors[lane]) return _motors[lane]->getCurrentStep();
    return 0;
}

float MotorManager::getLaneNormalisedPosition(uint8_t lane) const {
    if (lane >= _count || !_motors[lane]) return 0.0f;
    const LaneCalibration& cal = _calibration.getLane(lane);
    int32_t range = abs(cal.endStep - cal.startStep);
    if (range == 0) return 0.0f;
    float raw = (float)(_motors[lane]->getCurrentStep() - cal.startStep) / (float)range;
    return constrain(raw, 0.0f, 1.0f);
}

bool MotorManager::isLaneCalibrated(uint8_t lane) const {
    return _calibration.isCalibrated(lane);
}

bool MotorManager::isLaneHomed(uint8_t lane) const {
    return (lane < _count && _motors[lane]) ? _motors[lane]->isHomed() : false;
}

bool MotorManager::isLaneMoving(uint8_t lane) const {
    return (lane < _count && _motors[lane]) ? _motors[lane]->isMoving() : false;
}

int32_t MotorManager::_mapPosition(uint8_t lane, float position) const {
    const LaneCalibration& cal = _calibration.getLane(lane);
    int32_t start = cal.startStep;
    int32_t end   = cal.endStep;
    if (cal.directionReversed) { int32_t tmp = start; start = end; end = tmp; }
    return start + static_cast<int32_t>(position * (end - start));
}
