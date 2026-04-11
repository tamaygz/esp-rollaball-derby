#include "websocket.h"

void WSClient::begin(const char* host, uint16_t port, const char* playerName,
                     const char* persistedPlayerId) {
    _host       = host;
    _port       = port;
    _playerName = playerName;
    if (persistedPlayerId && strlen(persistedPlayerId) > 0) {
        _playerId = String(persistedPlayerId);
        Serial.printf("[WS] Restored persisted playerId: %s\n", _playerId.c_str());
    }
    _lastAttempt = 0;  // trigger immediate connect on first loop()

    _client.onMessage([this](WebsocketsMessage msg)         { _onMessage(msg); });
    _client.onEvent ([this](WebsocketsEvent ev, String data) { _onEvent(ev, data); });
}

void WSClient::onWiFiLost() {
    _connected   = false;
    _backoffMs   = WS_BACKOFF_MIN_MS;
    _lastAttempt = 0;  // reconnect immediately once WiFi recovers
    Serial.println("[WS] WiFi lost — connection reset");
}

void WSClient::loop() {
    if (_connected) {
        _client.poll();
        return;
    }

    // Connect immediately on first call (_lastAttempt == 0), then apply backoff.
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
        // Double backoff, clamp to maximum
        _backoffMs = min(_backoffMs * 2, (unsigned long)WS_BACKOFF_MAX_MS);
        Serial.printf("[WS] Connection failed — retry in %lu ms\n", _backoffMs);
    }
    // On success the ConnectionOpened event fires and resets backoff.
}

void WSClient::_sendRegister() {
    // Buffer covers: type + payload fields + LED capabilities ≈ 300 bytes; 384 gives ample headroom.
    char buf[384];
    JsonDocument doc;
    doc["type"] = "register";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["type"] = "sensor";
    if (_playerName.length() > 0) {
        payload["playerName"] = _playerName;
    }
    // Include stored playerId so the server can reconnect to the same player entry.
    if (_playerId.length() > 0) {
        payload["playerId"] = _playerId;
    }
    // LED metadata — lets the server validate LED count and log device capabilities.
    payload["ledCount"] = _ledMetadataCount;
    payload["chipType"] = "ESP8266";
    // Unique hardware identifier for persistent color assignment
    char chipIdBuf[9];
    snprintf(chipIdBuf, sizeof(chipIdBuf), "%08X", ESP.getChipId());
    payload["chipId"] = chipIdBuf;
    JsonObject ledCaps = payload["ledCapabilities"].to<JsonObject>();
    ledCaps["maxLeds"] = 300;
    ledCaps["method"]  = "DMA";
    ledCaps["pin"]     = PIN_LED;

    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    Serial.println("[WS] Register sent");
}

void WSClient::sendScore(int points) {
    if (!_connected) {
        Serial.println("[WS] Cannot send score: not connected");
        return;
    }
    if (_playerId.isEmpty()) {
        Serial.println("[WS] Cannot send score: playerId not assigned yet");
        return;
    }

    // Buffer covers: type(7) + playerId(≤40 UUID) + points(1 digit) + JSON overhead ≈ 80 bytes; 128 gives ample headroom.
    char buf[128];
    JsonDocument doc;
    doc["type"] = "score";
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["playerId"] = _playerId;
    payload["points"]   = points;

    serializeJson(doc, buf, sizeof(buf));
    _client.send(buf);
    Serial.printf("[WS] Score sent: playerId=%s points=%d\n", _playerId.c_str(), points);
}

void WSClient::_onMessage(WebsocketsMessage msg) {
    Serial.printf("[WS] Received: %s\n", msg.data().c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg.data());
    if (err) {
        Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* type = doc["type"];
    if (!type) return;

    if (strcmp(type, "registered") == 0) {
        // Server confirmed registration — extract our assigned player ID.
        const char* id = doc["payload"]["id"];
        if (id && strlen(id) > 0) {
            _playerId = String(id);
            Serial.printf("[WS] Registered: playerId=%s\n", _playerId.c_str());
        }
        return;
    }

    if (strcmp(type, "error") == 0) {
        const char* errMsg = doc["payload"]["message"];
        Serial.printf("[WS] Server error: %s\n", errMsg ? errMsg : "unknown");
        return;
    }

    if (strcmp(type, "countdown") == 0) {
        int count = doc["payload"]["count"] | 0;
        if (count >= 1) {
            _pendingGlobalEvent = GlobalEventType::COUNTDOWN_TICK;
        }
        return;
    }

    if (strcmp(type, "winner") == 0) {
        const char* winnerId = doc["payload"]["playerId"];
        if (winnerId) {
            _pendingGlobalEvent = (_playerId == winnerId)
                            ? GlobalEventType::WINNER_SELF
                            : GlobalEventType::WINNER_OTHER;
        }
        return;
    }

    if (strcmp(type, "game_event") == 0) {
        const char* event = doc["payload"]["event"];
        if (!event) return;
        if      (strcmp(event, "game_started") == 0) _pendingGlobalEvent = GlobalEventType::GAME_STARTED;
        else if (strcmp(event, "game_paused")  == 0) _pendingGlobalEvent = GlobalEventType::GAME_PAUSED;
        else if (strcmp(event, "game_resumed") == 0) _pendingGlobalEvent = GlobalEventType::GAME_RESUMED;
        else if (strcmp(event, "game_reset")   == 0) _pendingGlobalEvent = GlobalEventType::GAME_RESET;
        return;
    }

    if (strcmp(type, "scored") == 0) {
        // Only react to scoring events for our own player (device-local).
        const char* scoredId = doc["payload"]["playerId"];
        if (!scoredId || _playerId.isEmpty() || _playerId != scoredId) return;

        // Pick the highest-priority event from the events array.
        // Priority (high→low): took_lead > streak_three > score_3 > score_2
        //   > score_1 > streak_zero > zero_roll > became_last
        LocalEventType best = LocalEventType::NONE;
        JsonArrayConst events = doc["payload"]["events"].as<JsonArrayConst>();
        for (JsonVariantConst ev : events) {
            const char* evStr = ev.as<const char*>();
            if (!evStr) continue;
            LocalEventType candidate = LocalEventType::NONE;
            if      (strcmp(evStr, "took_lead")      == 0) candidate = LocalEventType::TOOK_LEAD;
            else if (strcmp(evStr, "streak_three_2x") == 0) candidate = LocalEventType::STREAK_THREE;
            else if (strcmp(evStr, "score_3")         == 0) candidate = LocalEventType::SCORE_PLUS3;
            else if (strcmp(evStr, "score_2")         == 0) candidate = LocalEventType::SCORE_PLUS2;
            else if (strcmp(evStr, "score_1")         == 0) candidate = LocalEventType::SCORE_PLUS1;
            else if (strcmp(evStr, "streak_zero_3x")  == 0) candidate = LocalEventType::STREAK_ZERO;
            else if (strcmp(evStr, "zero_roll")        == 0) candidate = LocalEventType::ZERO_ROLL;
            else if (strcmp(evStr, "became_last")      == 0) candidate = LocalEventType::BECAME_LAST;
            // Higher enum value = higher priority in our ordering
            if (static_cast<int>(candidate) > static_cast<int>(best)) {
                best = candidate;
            }
        }
        if (best != LocalEventType::NONE) {
            _pendingLocalEvent = best;
        }
        return;
    }

    if (strcmp(type, "led_config") == 0) {
        // Parse the LED configuration sent by the server after registration or
        // when an admin updates it via the web UI.
        LedConfig cfg = {};
        cfg.ledCount   = static_cast<uint16_t>(doc["payload"]["ledCount"] | LED_DEFAULT_COUNT);
        cfg.pin        = static_cast<uint8_t> (doc["payload"]["gpioPin"]  | LED_DEFAULT_PIN);
        cfg.brightness = static_cast<uint8_t> (doc["payload"]["brightness"] | LED_DEFAULT_BRIGHTNESS);
        cfg.topology   = LedTopology::STRIP; // default
        cfg.matrixRows = 8;
        cfg.matrixCols = 8;

        const char* topStr = doc["payload"]["topology"] | "strip";
        if      (strcmp(topStr, "ring")               == 0) cfg.topology = LedTopology::RING;
        else if (strcmp(topStr, "matrix_zigzag")      == 0) cfg.topology = LedTopology::MATRIX_ZIGZAG;
        else if (strcmp(topStr, "matrix_progressive") == 0) cfg.topology = LedTopology::MATRIX_PROGRESSIVE;

        // Parse device color: "#RRGGBB" hex string from server
        cfg.hasDeviceColor = false;
        const char* devColor = doc["payload"]["deviceColor"] | "";
        if (devColor[0] == '#' && strlen(devColor) == 7) {
            char hex[3] = {0};
            hex[0] = devColor[1]; hex[1] = devColor[2];
            cfg.deviceColorR = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
            hex[0] = devColor[3]; hex[1] = devColor[4];
            cfg.deviceColorG = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
            hex[0] = devColor[5]; hex[1] = devColor[6];
            cfg.deviceColorB = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
            cfg.hasDeviceColor = true;
            Serial.printf("[WS] deviceColor: #%02X%02X%02X\n", cfg.deviceColorR, cfg.deviceColorG, cfg.deviceColorB);
        }

        _pendingLedConfig    = cfg;
        _hasPendingLedConfig = true;
        Serial.printf("[WS] led_config: %u LEDs, pin=%u, brightness=%u\n",
                      cfg.ledCount, cfg.pin, cfg.brightness);
        return;
    }

    if (strcmp(type, "test_effect") == 0) {
        // Parse the test-effect command sent to this specific device by the admin.
        LedTestEffectMessage msg = {};

        const char* name = doc["payload"]["effectName"] | "solid";
        strlcpy(msg.effectName, name, sizeof(msg.effectName));

        // Color is sent as a hex string "#RRGGBB" or as separate r/g/b fields.
        const char* hexColor = doc["payload"]["params"]["color"] | "";
        if (hexColor[0] == '#' && strlen(hexColor) == 7) {
            // Parse "#RRGGBB"
            char hex[3] = {0};
            hex[0] = hexColor[1]; hex[1] = hexColor[2];
            msg.r = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
            hex[0] = hexColor[3]; hex[1] = hexColor[4];
            msg.g = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
            hex[0] = hexColor[5]; hex[1] = hexColor[6];
            msg.b = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
        } else {
            // Fallback: white
            msg.r = 255; msg.g = 255; msg.b = 255;
        }

        msg.speedMs   = static_cast<uint16_t>(doc["payload"]["params"]["speed"]      | 1000);
        msg.brightness = static_cast<uint8_t>(doc["payload"]["params"]["brightness"] | 255);

        _pendingTestEffect    = msg;
        _hasPendingTestEffect = true;
        Serial.printf("[WS] test_effect: effect=%s rgb=(%u,%u,%u) speed=%u\n",
                      msg.effectName, msg.r, msg.g, msg.b, msg.speedMs);
        return;
    }

    // Silently ignore other broadcast messages (state, positions).
}

LocalEventType WSClient::pollLocalEvent() {
    LocalEvent ev       = _pendingLocalEvent;
    _pendingLocalEvent  = LocalEventType::NONE;
    return ev;
}

GlobalEventType WSClient::pollGlobalEvent() {
    GlobalEvent ev       = _pendingGlobalEvent;
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

void WSClient::setLedMetadata(uint16_t ledCount) {
    _ledMetadataCount = ledCount;
}

void WSClient::_onEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("[WS] Connected");
        _connected = true;
        _backoffMs = WS_BACKOFF_MIN_MS;  // reset backoff on successful connection
        _sendRegister();
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("[WS] Disconnected");
        _connected   = false;
        _lastAttempt = millis();  // start backoff timer from now
    } else if (event == WebsocketsEvent::GotPing) {
        _client.pong();
    }
}
