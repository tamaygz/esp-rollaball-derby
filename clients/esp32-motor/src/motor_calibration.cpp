#include "motor_calibration.h"
#include <Arduino.h>
#include <derby_logger.h>

bool MotorCalibration::begin(uint8_t motorCount) {
    _motorCount = min(motorCount, (uint8_t)MOTOR_MAX_LANES);
    for (uint8_t i = 0; i < _motorCount; ++i) {
        _defaults(i);
        _calibrating[i] = false;
    }

    if (!LittleFS.exists(CALIB_FILE)) {
        DERBY_LOG_LN("[CALIB] No calibration file — using defaults");
        return true;
    }

    File f = LittleFS.open(CALIB_FILE, "r");
    if (!f) {
        DERBY_LOG_LN("[CALIB] Failed to open calibration file");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        DERBY_LOG_F("[CALIB] Parse error (%s) — using defaults\n", err.c_str());
        return false;
    }

    JsonArrayConst arr = doc["lanes"].as<JsonArrayConst>();
    for (JsonVariantConst v : arr) {
        uint8_t lane = v["laneId"] | 255;
        if (lane >= _motorCount) continue;

        LaneCalibration& c = _lanes[lane];
        c.startStep        = v["startStep"]       | 0;
        c.endStep          = v["endStep"]         | (int32_t)STEPPER_STEPS_PER_REV;
        c.totalTrackSteps  = v["totalTrackSteps"] | (int32_t)STEPPER_STEPS_PER_REV;
        c.stepsPerMm       = v["stepsPerMm"]      | 0.0f;
        c.directionReversed = v["directionReversed"] | false;
        c.maxSpeed         = v["maxSpeed"]        | STEPPER_DEFAULT_MAX_SPEED;
        c.acceleration     = v["acceleration"]    | STEPPER_DEFAULT_ACCELERATION;
        c.calibrated       = v["calibrated"]      | false;

        DERBY_LOG_F("[CALIB] Lane %u: start=%ld end=%ld total=%ld cal=%d\n",
                      lane, (long)c.startStep, (long)c.endStep,
                      (long)c.totalTrackSteps, (int)c.calibrated);
    }

    return true;
}

bool MotorCalibration::save() {
    JsonDocument doc;
    JsonArray arr = doc["lanes"].to<JsonArray>();
    for (uint8_t i = 0; i < _motorCount; ++i) {
        const LaneCalibration& c = _lanes[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["laneId"]            = c.laneId;
        obj["startStep"]         = c.startStep;
        obj["endStep"]           = c.endStep;
        obj["totalTrackSteps"]   = c.totalTrackSteps;
        obj["stepsPerMm"]        = c.stepsPerMm;
        obj["directionReversed"] = c.directionReversed;
        obj["maxSpeed"]          = c.maxSpeed;
        obj["acceleration"]      = c.acceleration;
        obj["calibrated"]        = c.calibrated;
    }

    // Atomic write: write to temp, then rename
    {
        File f = LittleFS.open(CALIB_TMP, "w");
        if (!f) {
            DERBY_LOG_LN("[CALIB] Failed to open calib.tmp for writing");
            return false;
        }
        serializeJson(doc, f);
        f.close();
    }

    if (LittleFS.exists(CALIB_FILE)) LittleFS.remove(CALIB_FILE);
    if (!LittleFS.rename(CALIB_TMP, CALIB_FILE)) {
        DERBY_LOG_LN("[CALIB] Rename calib.tmp → calib.json failed");
        return false;
    }

    DERBY_LOG_LN("[CALIB] Calibration saved");
    return true;
}

void MotorCalibration::beginCalibration(uint8_t lane) {
    if (lane >= _motorCount) return;
    _backup[lane]    = _lanes[lane];     // save current state for cancel
    _calibrating[lane] = true;
    DERBY_LOG_F("[CALIB] Lane %u: calibration started\n", lane);
}

void MotorCalibration::setStartPosition(uint8_t lane, int32_t currentStep) {
    if (lane >= _motorCount) return;
    _lanes[lane].startStep = currentStep;
    DERBY_LOG_F("[CALIB] Lane %u: start=%ld\n", lane, (long)currentStep);
}

void MotorCalibration::setEndPosition(uint8_t lane, int32_t currentStep) {
    if (lane >= _motorCount) return;
    _lanes[lane].endStep = currentStep;
    _lanes[lane].totalTrackSteps = abs(currentStep - _lanes[lane].startStep);
    DERBY_LOG_F("[CALIB] Lane %u: end=%ld total=%ld\n",
                  lane, (long)currentStep, (long)_lanes[lane].totalTrackSteps);
}

bool MotorCalibration::finishCalibration(uint8_t lane) {
    if (lane >= _motorCount) return false;
    LaneCalibration& c = _lanes[lane];
    if (c.totalTrackSteps == 0 || c.startStep == c.endStep) {
        DERBY_LOG_F("[CALIB] Lane %u: invalid calibration (start == end)\n", lane);
        cancelCalibration(lane);
        return false;
    }
    c.calibrated = true;
    _calibrating[lane] = false;
    DERBY_LOG_F("[CALIB] Lane %u: calibration complete\n", lane);
    save();
    return true;
}

void MotorCalibration::cancelCalibration(uint8_t lane) {
    if (lane >= _motorCount) return;
    _lanes[lane]     = _backup[lane];
    _calibrating[lane] = false;
    DERBY_LOG_F("[CALIB] Lane %u: calibration cancelled\n", lane);
}

void MotorCalibration::resetCalibration(uint8_t lane) {
    if (lane == 255) {
        for (uint8_t i = 0; i < _motorCount; ++i) _defaults(i);
        save();
        return;
    }
    if (lane >= _motorCount) return;
    _defaults(lane);
    save();
    DERBY_LOG_F("[CALIB] Lane %u: reset\n", lane);
}

void MotorCalibration::setDirection(uint8_t lane, bool reversed) {
    if (lane >= _motorCount) return;
    _lanes[lane].directionReversed = reversed;
}

void MotorCalibration::setSpeed(uint8_t lane, float maxSpeed, float acceleration) {
    if (lane >= _motorCount) return;
    if (maxSpeed > 0)     _lanes[lane].maxSpeed     = maxSpeed;
    if (acceleration > 0) _lanes[lane].acceleration = acceleration;
}

void MotorCalibration::setStepsPerMm(uint8_t lane, float stepsPerMm) {
    if (lane >= _motorCount) return;
    _lanes[lane].stepsPerMm = stepsPerMm;
}

void MotorCalibration::setFromJson(uint8_t lane, JsonObjectConst obj) {
    if (lane >= _motorCount) return;
    LaneCalibration& c = _lanes[lane];
    if (obj["directionReversed"].is<bool>()) c.directionReversed = obj["directionReversed"];
    if (obj["maxSpeed"].is<float>())         c.maxSpeed          = obj["maxSpeed"];
    if (obj["acceleration"].is<float>())     c.acceleration      = obj["acceleration"];
    if (obj["stepsPerMm"].is<float>())       c.stepsPerMm        = obj["stepsPerMm"];
}

const LaneCalibration& MotorCalibration::getLane(uint8_t lane) const {
    static LaneCalibration empty = laneCalibrationDefaults(255);
    if (lane >= _motorCount) return empty;
    return _lanes[lane];
}

bool MotorCalibration::isCalibrated(uint8_t lane) const {
    if (lane >= _motorCount) return false;
    return _lanes[lane].calibrated;
}

void MotorCalibration::toJson(JsonArray& arr) const {
    for (uint8_t i = 0; i < _motorCount; ++i) {
        const LaneCalibration& c = _lanes[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["laneId"]            = c.laneId;
        obj["startStep"]         = c.startStep;
        obj["endStep"]           = c.endStep;
        obj["totalTrackSteps"]   = c.totalTrackSteps;
        obj["stepsPerMm"]        = c.stepsPerMm;
        obj["directionReversed"] = c.directionReversed;
        obj["maxSpeed"]          = c.maxSpeed;
        obj["acceleration"]      = c.acceleration;
        obj["calibrated"]        = c.calibrated;
    }
}

void MotorCalibration::_defaults(uint8_t lane) {
    _lanes[lane] = laneCalibrationDefaults(lane);
}
