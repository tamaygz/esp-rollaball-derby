#include "websocket.h"
#include <WiFi.h>

void WSClient::begin(const char* host, uint16_t port, const char* playerName,
                     uint8_t motorCount, const uint8_t* motorColors,
                     const char* persistedPlayerId) {
    _host        = host;
    _port        = port;
    _playerName  = playerName;
    _motorCount  = min(motorCount, (uint8_t)WS_MAX_PLAYERS);
    if (motorColors) {
        memcpy(_motorColors, motorColors, _motorCount);
    }
    if (persistedPlayerId && strlen(persistedPlayerId) > 0) {
        _playerId = String(persistedPlayerId);
        Serial.printf("[WS] Restored playerId: %s\n", _playerId.c_str());
    }
    _lastAttempt = 0;    // trigger immediate connect on first loop()

    _client.onMessage([this](WebsocketsMessage msg)          { _onMessage(msg); });
    _client.onEvent ([this](WebsocketsEvent ev, String data)  { _onEvent(ev, data); });
}

void WSClient::onWiFiLost() {
    _connected   = false;
    _backoffMs   = WS_BACKOFF_MIN_MS;
    _lastAttempt = 0;
    Serial.println("[WS] WiFi lost — connection reset");
}

void WSClient::loop() {
    if (_connected) {
        _client.poll();
        return;
    }
    unsigned long now = millis();
    if (_lastAttempt == 0 || (now - _lastAttempt >= _backoffMs)) {
        _lastAttempt = now;
        _connect();
    }
}

void WSClient::_connect() {
    Serial.printf("[WS] Connecting to ws://%s:%u%s\n", _host.c_str(), _port, WS_PATH);
    bool ok = _client.connect(_host, _port, WS_PATH);
    if (!ok) {
        _backoffMs = min(_backoffMs * 2, (unsigned long)WS_BACKOFF_MAX_MS);
        Serial.printf("[WS] Connection failed — retry in %lu ms\n", _backoffMs);
    }
}

void WSClient::_sendRegister() {
    // Buffer: registration JSON for motor client (~512 bytes with full capabilities)
    char buf[512];
    JsonDocument doc;
    doc["type"] = "register";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["type"]       = "motor";
    payload["playerName"] = _playerName;
    if (_playerId.length() > 0) {
        payload["playerId"] = _playerId;
    }

    // Unique hardware identifier — ESP32 MAC as hex string
    char chipIdBuf[17];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(chipIdBuf, sizeof(chipIdBuf), "%04X%08X",
             (uint16_t)(mac >> 32), (uint32_t)(mac & 0xFFFFFFFF));
    payload["chipId"]  = chipIdBuf;
    payload["chipType"] = "ESP32";

    // Motor metadata
    payload["motorCount"] = _motorCount;
    JsonArray motorColors = payload["motorColors"].to<JsonArray>();
    for (uint8_t i = 0; i < _motorCount; ++i) {
        motorColors.add(_motorColors[i]);
    }

    // Device IP for server-side proxy routing
    payload["ip"] = WiFi.localIP().toString();

    // LED metadata
    payload["ledCount"] = _ledMetadataCount;

    // Capability flags
    JsonObject caps = payload["capabilities"].to<JsonObject>();
    caps["motors"]  = (_motorCount > 0);
    caps["leds"]    = (_ledMetadataCount > 0);
    caps["buttons"] = true;

    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    Serial.printf("[WS] Register sent (motorCount=%u, ip=%s)\n",
                  _motorCount, WiFi.localIP().toString().c_str());
}

void WSClient::sendButton(uint8_t buttonIdx, const char* action) {
    if (!_connected) {
        Serial.println("[WS] Cannot send button: not connected");
        return;
    }
    if (_playerId.isEmpty()) {
        Serial.println("[WS] Cannot send button: playerId not assigned");
        return;
    }

    char buf[128];
    JsonDocument doc;
    doc["type"] = "button";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["button"]   = buttonIdx;
    payload["action"]   = action;
    payload["playerId"] = _playerId;

    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    Serial.printf("[WS] Button sent: idx=%u action=%s\n", buttonIdx, action);
}

void WSClient::_onMessage(WebsocketsMessage msg) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg.data());
    if (err) {
        Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* type = doc["type"];
    if (!type) return;

    // ─── registered ─────────────────────────────────────────────────────────
    if (strcmp(type, "registered") == 0) {
        const char* id = doc["payload"]["id"];
        if (id && strlen(id) > 0) {
            _playerId = String(id);
            Serial.printf("[WS] Registered: playerId=%s\n", _playerId.c_str());
        }
        return;
    }

    // ─── error ──────────────────────────────────────────────────────────────
    if (strcmp(type, "error") == 0) {
        const char* errMsg = doc["payload"]["message"];
        Serial.printf("[WS] Server error: %s\n", errMsg ? errMsg : "unknown");
        return;
    }

    // ─── countdown ──────────────────────────────────────────────────────────
    if (strcmp(type, "countdown") == 0) {
        int count = doc["payload"]["count"] | 0;
        if (count >= 1) _pendingGlobalEvent = GlobalEventType::COUNTDOWN_TICK;
        return;
    }

    // ─── winner ─────────────────────────────────────────────────────────────
    if (strcmp(type, "winner") == 0) {
        const char* winnerId = doc["payload"]["playerId"];
        if (winnerId) {
            _pendingGlobalEvent = (_playerId == winnerId)
                                ? GlobalEventType::WINNER_SELF
                                : GlobalEventType::WINNER_OTHER;
        }
        return;
    }

    // ─── game_event ─────────────────────────────────────────────────────────
    if (strcmp(type, "game_event") == 0) {
        const char* event = doc["payload"]["event"];
        if (!event) return;
        if      (strcmp(event, "game_started") == 0) _pendingGlobalEvent = GlobalEventType::GAME_STARTED;
        else if (strcmp(event, "game_paused")  == 0) _pendingGlobalEvent = GlobalEventType::GAME_PAUSED;
        else if (strcmp(event, "game_resumed") == 0) _pendingGlobalEvent = GlobalEventType::GAME_RESUMED;
        else if (strcmp(event, "game_reset")   == 0) _pendingGlobalEvent = GlobalEventType::GAME_RESET;
        return;
    }

    // ─── state (player positions for motor control) ──────────────────────────
    if (strcmp(type, "state") == 0) {
        JsonArrayConst players = doc["payload"]["state"]["players"].as<JsonArrayConst>();
        uint8_t count = 0;
        for (JsonVariantConst p : players) {
            if (count >= WS_MAX_PLAYERS) break;
            const char* id  = p["id"]       | "";
            float pos       = p["position"] | 0.0f;
            uint8_t ci      = p["colorIndex"] | 0;
            strlcpy(_positions[count].playerId, id, sizeof(_positions[count].playerId));
            _positions[count].position   = pos;
            _positions[count].colorIndex = ci;
            count++;
        }
        _positionCount = count;
        _positionsDirty = (count > 0);
        return;
    }

    // ─── led_config ─────────────────────────────────────────────────────────
    if (strcmp(type, "led_config") == 0) {
        LedConfig cfg      = {};
        cfg.ledCount       = (uint16_t)(doc["payload"]["ledCount"]   | 0);
        cfg.pin            = (uint8_t) (doc["payload"]["gpioPin"]    | PIN_LED_MATRIX);
        cfg.brightness     = (uint8_t) (doc["payload"]["brightness"] | LED_DEFAULT_BRIGHTNESS);
        cfg.topology       = LedTopology::MATRIX_ZIGZAG;

        const char* topStr = doc["payload"]["topology"] | "matrix_zigzag";
        if      (strcmp(topStr, "strip")               == 0) cfg.topology = LedTopology::STRIP;
        else if (strcmp(topStr, "ring")                == 0) cfg.topology = LedTopology::RING;
        else if (strcmp(topStr, "matrix_progressive")  == 0) cfg.topology = LedTopology::MATRIX_PROGRESSIVE;

        cfg.matrixRows = (uint8_t)(doc["payload"]["matrixRows"] | 8);
        cfg.matrixCols = (uint8_t)(doc["payload"]["matrixCols"] | 8);
        cfg.mirrorH    = doc["payload"]["mirrorH"] | false;
        cfg.mirrorV    = doc["payload"]["mirrorV"] | false;

        const char* devColor = doc["payload"]["deviceColor"] | "";
        cfg.hasDeviceColor = false;
        if (devColor[0] == '#' && strlen(devColor) == 7) {
            char hex[3] = {0};
            hex[0] = devColor[1]; hex[1] = devColor[2];
            cfg.deviceColorR = (uint8_t)strtoul(hex, nullptr, 16);
            hex[0] = devColor[3]; hex[1] = devColor[4];
            cfg.deviceColorG = (uint8_t)strtoul(hex, nullptr, 16);
            hex[0] = devColor[5]; hex[1] = devColor[6];
            cfg.deviceColorB = (uint8_t)strtoul(hex, nullptr, 16);
            cfg.hasDeviceColor = true;
        }
        _pendingLedConfig    = cfg;
        _hasPendingLedConfig = true;
        return;
    }

    // ─── test_effect ────────────────────────────────────────────────────────
    if (strcmp(type, "test_effect") == 0) {
        LedTestEffectMessage msg = {};
        const char* name  = doc["payload"]["effectName"] | "solid";
        strlcpy(msg.effectName, name, sizeof(msg.effectName));
        const char* hexColor = doc["payload"]["params"]["color"] | "";
        if (hexColor[0] == '#' && strlen(hexColor) == 7) {
            char hex[3] = {0};
            hex[0] = hexColor[1]; hex[1] = hexColor[2]; msg.r = (uint8_t)strtoul(hex, nullptr, 16);
            hex[0] = hexColor[3]; hex[1] = hexColor[4]; msg.g = (uint8_t)strtoul(hex, nullptr, 16);
            hex[0] = hexColor[5]; hex[1] = hexColor[6]; msg.b = (uint8_t)strtoul(hex, nullptr, 16);
        } else {
            msg.r = 255; msg.g = 255; msg.b = 255;
        }
        msg.speedMs    = (uint16_t)(doc["payload"]["params"]["speed"]      | 1000);
        msg.brightness = (uint8_t) (doc["payload"]["params"]["brightness"] | 255);
        msg.durationMs = (uint16_t)(doc["payload"]["durationMs"]           | 0);
        const char* text = doc["payload"]["params"]["text"] | "";
        strlcpy(msg.text, text, sizeof(msg.text));
        _pendingTestEffect    = msg;
        _hasPendingTestEffect = true;
        return;
    }

    if (strcmp(type, "stop_effect") == 0) {
        _pendingStopEffect = true;
        Serial.println("[WS] stop_effect received");
        return;
    }

    // All other message types silently ignored.
}

bool WSClient::pollPositions(PlayerPosition out[], uint8_t& count) {
    if (!_positionsDirty) return false;
    memcpy(out, _positions, _positionCount * sizeof(PlayerPosition));
    count = _positionCount;
    _positionsDirty = false;
    return true;
}

GlobalEventType WSClient::pollGlobalEvent() {
    GlobalEventType ev   = _pendingGlobalEvent;
    _pendingGlobalEvent  = GlobalEventType::NONE;
    return ev;
}

bool WSClient::pollLedConfig(LedConfig& out) {
    if (!_hasPendingLedConfig) return false;
    out                  = _pendingLedConfig;
    _hasPendingLedConfig = false;
    return true;
}

bool WSClient::pollTestEffect(LedTestEffectMessage& out) {
    if (!_hasPendingTestEffect) return false;
    out                   = _pendingTestEffect;
    _hasPendingTestEffect = false;
    return true;
}

bool WSClient::pollStopEffect() {
    if (!_pendingStopEffect) return false;
    _pendingStopEffect = false;
    return true;
}

void WSClient::setLedMetadata(uint16_t ledCount) {
    _ledMetadataCount = ledCount;
}

void WSClient::_onEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("[WS] Connected");
        _connected = true;
        _backoffMs = WS_BACKOFF_MIN_MS;
        _sendRegister();
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("[WS] Disconnected");
        _connected   = false;
        _lastAttempt = millis();
    } else if (event == WebsocketsEvent::GotPing) {
        _client.pong();
    }
}
