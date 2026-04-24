#include "websocket.h"
#include <device_info.h>
#include <WiFi.h>

void WSClient::begin(const char* host, uint16_t port, const char* playerName,
                     uint8_t motorCount, const uint8_t* motorColors,
                     const char* persistedPlayerId) {
    _motorCount = min(motorCount, (uint8_t)WS_MAX_PLAYERS);
    if (motorColors) memcpy(_motorColors, motorColors, _motorCount);
    _baseBegin(host, port, playerName, persistedPlayerId);
}

void WSClient::sendButton(uint8_t buttonIdx, const char* action) {
    if (!_connected) {
        DERBY_LOG_LN("[WS] Cannot send button: not connected");
        return;
    }
    if (_playerId.isEmpty()) {
        DERBY_LOG_LN("[WS] Cannot send button: playerId not assigned");
        return;
    }
    char buf[128];
    JsonDocument doc;
    doc["type"] = "button";
    doc["payload"]["button"]   = buttonIdx;
    doc["payload"]["action"]   = action;
    doc["payload"]["playerId"] = _playerId;
    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    DERBY_LOG_F("[WS] Button sent: idx=%u action=%s\n", buttonIdx, action);
}

bool WSClient::pollPositions(PlayerPosition out[], uint8_t& count) {
    if (!_positionsDirty) return false;
    memcpy(out, _positions, _positionCount * sizeof(PlayerPosition));
    count = _positionCount;
    _positionsDirty = false;
    return true;
}

void WSClient::_sendRegister() {
    char buf[512];
    JsonDocument doc;
    doc["type"] = "register";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["type"]       = "motor";
    payload["playerName"] = _playerName;
    if (_playerId.length() > 0) payload["playerId"] = _playerId;
    char chipIdBuf[DERBY_CHIP_ID_HEX_MAX_LEN];
    derbyChipIdHex(chipIdBuf, sizeof(chipIdBuf));
    payload["chipId"]     = chipIdBuf;
    payload["chipType"]   = derbyChipType();
    payload["motorCount"] = _motorCount;
    JsonArray colors = payload["motorColors"].to<JsonArray>();
    for (uint8_t i = 0; i < _motorCount; ++i) colors.add(_motorColors[i]);
    payload["ip"]       = WiFi.localIP().toString();
    payload["ledCount"] = _ledMetadataCount;
    JsonObject caps = payload["deviceCapabilities"].to<JsonObject>();
    caps["motors"]  = (_motorCount > 0);
    caps["leds"]    = (_ledMetadataCount > 0);
    caps["buttons"] = true;
    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    DERBY_LOG_F("[WS] Register sent (motorCount=%u, ip=%s)\n",
                _motorCount, WiFi.localIP().toString().c_str());
}

void WSClient::_onAppMessage(const char* type, JsonDocument& doc) {
    if (strcmp(type, "state") != 0) return;

    JsonArrayConst players = doc["payload"]["state"]["players"].as<JsonArrayConst>();
    uint8_t count = 0;
    for (JsonVariantConst p : players) {
        if (count >= WS_MAX_PLAYERS) break;
        const char* id = p["id"]         | "";
        float pos      = p["position"]   | 0.0f;
        uint8_t ci     = p["colorIndex"] | 0;
        strlcpy(_positions[count].playerId, id, sizeof(_positions[count].playerId));
        _positions[count].position   = pos;
        _positions[count].colorIndex = ci;
        count++;
    }
    _positionCount  = count;
    _positionsDirty = (count > 0);
}
