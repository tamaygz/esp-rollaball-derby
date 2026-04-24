#include "websocket.h"
#include <device_info.h>

void WSClient::begin(const char* host, uint16_t port, const char* playerName,
                     const char* persistedPlayerId) {
    _baseBegin(host, port, playerName, persistedPlayerId);
}

void WSClient::sendScore(int points) {
    if (!_connected) {
        DERBY_LOG_LN("[WS] Cannot send score: not connected");
        return;
    }
    if (_playerId.isEmpty()) {
        DERBY_LOG_LN("[WS] Cannot send score: playerId not assigned yet");
        return;
    }

    char buf[128];
    JsonDocument doc;
    doc["type"] = "score";
    doc["payload"]["playerId"] = _playerId;
    doc["payload"]["points"]   = points;
    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    DERBY_LOG_F("[WS] Score sent: playerId=%s points=%d\n", _playerId.c_str(), points);
}

LocalEventType WSClient::pollLocalEvent() {
    LocalEventType ev = LocalEventType::NONE;
    _localQueue.pop(ev);
    return ev;
}

void WSClient::_sendRegister() {
    char buf[384];
    JsonDocument doc;
    doc["type"] = "register";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["type"] = "sensor";
    if (_playerName.length() > 0) payload["playerName"] = _playerName;
    if (_playerId.length()   > 0) payload["playerId"]   = _playerId;
    payload["ledCount"]  = _ledMetadataCount;
    payload["chipType"]  = derbyChipType();
    char chipIdBuf[DERBY_CHIP_ID_HEX_MAX_LEN];
    derbyChipIdHex(chipIdBuf, sizeof(chipIdBuf));
    payload["chipId"] = chipIdBuf;
    JsonObject ledCaps = payload["ledCapabilities"].to<JsonObject>();
    ledCaps["maxLeds"] = LED_MAX_COUNT;
    ledCaps["method"]  = LED_CAPABILITIES_METHOD;
    ledCaps["pin"]     = PIN_LED;
    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    DERBY_LOG_LN("[WS] Register sent");
}

void WSClient::_onAppMessage(const char* type, JsonDocument& doc) {
    if (strcmp(type, "scored") != 0) return;

    // Only react to scoring events for our own player (device-local).
    const char* scoredId = doc["payload"]["playerId"];
    if (!scoredId || _playerId.isEmpty() || _playerId != scoredId) return;

    // Pick the highest-priority event from the events array, then push it to the
    // local queue (priority-based eviction protects high-value events on overflow).
    // Event strings are canonical per clients/shared/leds/GameEvents.h.
    LocalEventType best = LocalEventType::NONE;
    JsonArrayConst events = doc["payload"]["events"].as<JsonArrayConst>();
    for (JsonVariantConst ev : events) {
        const char* evStr = ev.as<const char*>();
        if (!evStr) continue;
        LocalEventType candidate = LocalEventType::NONE;
        if      (strcmp(evStr, "took_lead")       == 0) candidate = LocalEventType::TOOK_LEAD;
        else if (strcmp(evStr, "streak_three_2x") == 0) candidate = LocalEventType::STREAK_THREE;
        else if (strcmp(evStr, "score_3")         == 0) candidate = LocalEventType::SCORE_PLUS3;
        else if (strcmp(evStr, "score_2")         == 0) candidate = LocalEventType::SCORE_PLUS2;
        else if (strcmp(evStr, "score_1")         == 0) candidate = LocalEventType::SCORE_PLUS1;
        else if (strcmp(evStr, "streak_zero_3x")  == 0) candidate = LocalEventType::STREAK_ZERO;
        else if (strcmp(evStr, "zero_roll")        == 0) candidate = LocalEventType::ZERO_ROLL;
        else if (strcmp(evStr, "became_last")      == 0) candidate = LocalEventType::BECAME_LAST;
        if (static_cast<int>(candidate) > static_cast<int>(best)) best = candidate;
    }
    if (best != LocalEventType::NONE) {
        _localQueue.push(best);
    }
}
