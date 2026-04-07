#include "websocket.h"

void WSClient::begin(const char* host, uint16_t port, const char* playerName) {
    _host       = host;
    _port       = port;
    _playerName = playerName;
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
    // Buffer covers: type(10) + payload.type(10) + playerName(≤21) + playerId(≤40 UUID) + JSON overhead ≈ 150 bytes; 256 gives ample headroom.
    char buf[256];
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
            _pendingEvent = GameEvent::COUNTDOWN_TICK;
        }
        return;
    }

    if (strcmp(type, "winner") == 0) {
        const char* winnerId = doc["payload"]["playerId"];
        if (winnerId) {
            _pendingEvent = (_playerId == winnerId)
                            ? GameEvent::WINNER_SELF
                            : GameEvent::WINNER_OTHER;
        }
        return;
    }

    // Silently ignore other broadcast messages (state, scored, positions).
}

GameEvent WSClient::pollEvent() {
    GameEvent ev  = _pendingEvent;
    _pendingEvent = GameEvent::NONE;
    return ev;
}

void WSClient::_onEvent(WebsocketsEvent event, String data) {

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
