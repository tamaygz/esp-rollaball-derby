#pragma once

// ws_client_base.h — shared WebSocket machinery for all Derby firmware clients.
//
// ⚠ INCLUDE ORDER: This header must be included AFTER the project's config.h so
//   that WS_PATH, WS_BACKOFF_MIN_MS, WS_BACKOFF_MAX_MS, and LED_DEFAULT_* are
//   already defined when these inline methods are compiled.
//
// Usage:
//   1.  In the project websocket.h  — include config.h first, then this header.
//       Derive your WSClient from WSClientBase.
//   2.  Implement the two pure-virtual methods:
//         _sendRegister()                     — build and send the device registration
//         _onAppMessage(type, doc)            — handle device-specific message types
//   3.  Call _baseBegin() from your begin() after storing device-specific fields.
//   4.  Call DerbyLogger::setSender(this) from main() once wsClient is ready.

#ifndef NATIVE_TEST

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <leds/GameEvents.h>
#include <leds/EventQueue.h>
#include <color_utils.h>
#include <derby_logger.h>

using namespace websockets;

class WSClientBase : public ILogSender {
public:
    // ─── ILogSender ──────────────────────────────────────────────────────────

    bool isConnected() const override { return _connected; }

    // Send a log line over WebSocket.  Returns false if not connected.
    bool sendLog(const char* message) override {
        if (!_connected) return false;
        // Small stack buffer — message is at most DERBY_LOG_LINE_MAX chars
        char buf[DERBY_LOG_LINE_MAX + 64];
        JsonDocument doc;
        doc["type"] = "log";
        doc["payload"]["message"] = message;
        if (!serializeJson(doc, buf, sizeof(buf))) return false;
        _client.send(buf);
        return true;
    }

    // ─── Common public API ────────────────────────────────────────────────────

    // Drive the WebSocket state machine. Call every loop() iteration.
    void loop() {
        if (_connected) {
            _client.poll();
            return;
        }
        // Connect immediately on first call, then apply exponential backoff.
        const unsigned long now = millis();
        if (_lastAttempt == 0 || (now - _lastAttempt >= _backoffMs)) {
            _lastAttempt = now;
            _connect();
        }
    }

    // Call when WiFi is lost. Resets state so reconnect starts fresh.
    void onWiFiLost() {
        _connected   = false;
        _backoffMs   = WS_BACKOFF_MIN_MS;
        _lastAttempt = 0;
        DERBY_LOG_LN("[WS] WiFi lost — connection reset");
    }

    const String& getPlayerId()  const { return _playerId; }
    void setLedMetadata(uint16_t ledCount) { _ledMetadataCount = ledCount; }

    // ─── Common poll methods ──────────────────────────────────────────────────

    GlobalEventType pollGlobalEvent() {
        GlobalEventType ev = GlobalEventType::NONE;
        _globalQueue.pop(ev);
        return ev;
    }

    bool pollLedConfig(LedConfig& out) {
        if (!_hasPendingLedConfig) return false;
        out                  = _pendingLedConfig;
        _hasPendingLedConfig = false;
        return true;
    }

    bool pollTestEffect(LedTestEffectMessage& out) {
        if (!_hasPendingTestEffect) return false;
        out                   = _pendingTestEffect;
        _hasPendingTestEffect = false;
        return true;
    }

    bool pollStopEffect() {
        if (!_pendingStopEffect) return false;
        _pendingStopEffect = false;
        return true;
    }

protected:
    // ─── Shared state (accessible by derived classes) ─────────────────────────

    WebsocketsClient _client;
    bool             _connected         = false;
    String           _playerId;
    String           _playerName;
    String           _host;
    uint16_t         _port              = 3000;
    unsigned long    _lastAttempt       = 0;
    unsigned long    _backoffMs         = WS_BACKOFF_MIN_MS;
    uint16_t         _ledMetadataCount  = 0;

    // Global event queue — bounded FIFO with priority-based overflow eviction.
    // Capacity of 4 is sufficient: at most one event fires per WS message,
    // and loop() drains it every iteration.
    EventQueue<GlobalEventType, 4> _globalQueue;

    LedConfig             _pendingLedConfig       = {};
    bool                  _hasPendingLedConfig    = false;
    LedTestEffectMessage  _pendingTestEffect      = {};
    bool                  _hasPendingTestEffect   = false;
    bool                  _pendingStopEffect      = false;

    // ─── Called from derived begin() ─────────────────────────────────────────

    void _baseBegin(const char* host, uint16_t port, const char* playerName,
                    const char* persistedPlayerId) {
        _host       = host;
        _port       = port;
        _playerName = playerName;
        if (persistedPlayerId && strlen(persistedPlayerId) > 0) {
            _playerId = String(persistedPlayerId);
            DERBY_LOG_F("[WS] Restored persisted playerId: %s\n", _playerId.c_str());
        }
        _lastAttempt = 0;   // trigger immediate connect on first loop()

        _client.onMessage([this](WebsocketsMessage msg)          { _onMessage(msg); });
        _client.onEvent ([this](WebsocketsEvent ev, String data)  { _onEvent(ev, data); });
    }

    // ─── Device-specific overrides ────────────────────────────────────────────

    // Build and send the JSON registration payload for this device type.
    virtual void _sendRegister() = 0;

    // Called for any message type not handled by the base class.
    // Default is a no-op; derived classes override to handle device-specific types.
    virtual void _onAppMessage(const char* type, JsonDocument& doc) {
        (void)type; (void)doc;
    }

    // Default LedTopology when the server omits the "topology" field in led_config.
    // Sensor default is STRIP; motor overrides this to MATRIX_ZIGZAG.
    virtual LedTopology _defaultTopology() const { return LedTopology::STRIP; }

private:
    void _connect() {
        DERBY_LOG_F("[WS] Connecting to ws://%s:%u%s\n", _host.c_str(), _port, WS_PATH);
        const bool ok = _client.connect(_host, _port, WS_PATH);
        if (!ok) {
            _backoffMs = min(_backoffMs * 2, (unsigned long)WS_BACKOFF_MAX_MS);
            DERBY_LOG_F("[WS] Connection failed — retry in %lu ms\n", _backoffMs);
        }
        // On success, ConnectionOpened event fires and resets backoff + registers.
    }

    void _onEvent(WebsocketsEvent event, String data) {
        if (event == WebsocketsEvent::ConnectionOpened) {
            DERBY_LOG_LN("[WS] Connected");
            _connected = true;
            _backoffMs = WS_BACKOFF_MIN_MS;
            _sendRegister();
            // Drain any lines buffered before this connection was available.
            DerbyLogger::flushQueue();
        } else if (event == WebsocketsEvent::ConnectionClosed) {
            DERBY_LOG_LN("[WS] Disconnected");
            _connected   = false;
            _lastAttempt = millis();
        } else if (event == WebsocketsEvent::GotPing) {
            _client.pong();
        }
    }

    void _onMessage(WebsocketsMessage msg) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, msg.data());
        if (err) {
            DERBY_LOG_F("[WS] JSON parse error: %s\n", err.c_str());
            return;
        }

        const char* type = doc["type"];
        if (!type) return;

        if (strcmp(type, "registered") == 0) {
            const char* id = doc["payload"]["id"];
            if (id && strlen(id) > 0) {
                _playerId = String(id);
                DERBY_LOG_F("[WS] Registered: playerId=%s\n", _playerId.c_str());
            }
            return;
        }

        if (strcmp(type, "error") == 0) {
            const char* errMsg = doc["payload"]["message"];
            DERBY_LOG_F("[WS] Server error: %s\n", errMsg ? errMsg : "unknown");
            return;
        }

        if (strcmp(type, "countdown") == 0) {
            const int count = doc["payload"]["count"] | 0;
            if (count >= 1) _globalQueue.push(GlobalEventType::COUNTDOWN_TICK);
            return;
        }

        if (strcmp(type, "winner") == 0) {
            const char* winnerId = doc["payload"]["playerId"];
            if (winnerId) {
                _globalQueue.push((_playerId == winnerId)
                                ? GlobalEventType::WINNER_SELF
                                : GlobalEventType::WINNER_OTHER);
            }
            return;
        }

        if (strcmp(type, "game_event") == 0) {
            const char* event = doc["payload"]["event"];
            if (!event) return;
            if      (strcmp(event, "game_started") == 0) _globalQueue.push(GlobalEventType::GAME_STARTED);
            else if (strcmp(event, "game_paused")  == 0) _globalQueue.push(GlobalEventType::GAME_PAUSED);
            else if (strcmp(event, "game_resumed") == 0) _globalQueue.push(GlobalEventType::GAME_RESUMED);
            else if (strcmp(event, "game_reset")   == 0) _globalQueue.push(GlobalEventType::GAME_RESET);
            return;
        }

        if (strcmp(type, "led_config") == 0) {
            LedConfig cfg = {};
            cfg.ledCount   = (uint16_t)(doc["payload"]["ledCount"]   | LED_DEFAULT_COUNT);
            cfg.pin        = (uint8_t) (doc["payload"]["gpioPin"]    | LED_DEFAULT_PIN);
            cfg.brightness = (uint8_t) (doc["payload"]["brightness"] | LED_DEFAULT_BRIGHTNESS);
            cfg.topology   = _defaultTopology();
            cfg.matrixRows = (uint8_t)(doc["payload"]["matrixRows"]  | 8);
            cfg.matrixCols = (uint8_t)(doc["payload"]["matrixCols"]  | 8);
            if (cfg.matrixRows < 1) cfg.matrixRows = 8;
            if (cfg.matrixCols < 1) cfg.matrixCols = 8;
            cfg.mirrorH    = doc["payload"]["mirrorH"] | false;
            cfg.mirrorV    = doc["payload"]["mirrorV"] | false;

            // Parse topology string; empty/unknown falls back to _defaultTopology().
            const char* topStr = doc["payload"]["topology"] | "";
            if      (strcmp(topStr, "strip")               == 0) cfg.topology = LedTopology::STRIP;
            else if (strcmp(topStr, "ring")                == 0) cfg.topology = LedTopology::RING;
            else if (strcmp(topStr, "matrix_zigzag")       == 0) cfg.topology = LedTopology::MATRIX_ZIGZAG;
            else if (strcmp(topStr, "matrix_progressive")  == 0) cfg.topology = LedTopology::MATRIX_PROGRESSIVE;

            cfg.hasDeviceColor = derbyParseHexColor(
                doc["payload"]["deviceColor"] | "",
                cfg.deviceColorR, cfg.deviceColorG, cfg.deviceColorB);
            if (cfg.hasDeviceColor) {
                DERBY_LOG_F("[WS] deviceColor: #%02X%02X%02X\n",
                            cfg.deviceColorR, cfg.deviceColorG, cfg.deviceColorB);
            }
            _pendingLedConfig    = cfg;
            _hasPendingLedConfig = true;
            DERBY_LOG_F("[WS] led_config: %u LEDs, pin=%u, brightness=%u\n",
                        cfg.ledCount, cfg.pin, cfg.brightness);
            return;
        }

        if (strcmp(type, "test_effect") == 0) {
            LedTestEffectMessage msg = {};
            const char* name = doc["payload"]["effectName"] | "solid";
            strlcpy(msg.effectName, name, sizeof(msg.effectName));
            const char* hexColor = doc["payload"]["params"]["color"] | "";
            if (!derbyParseHexColor(hexColor, msg.r, msg.g, msg.b)) {
                msg.r = 255; msg.g = 255; msg.b = 255;
            }
            msg.speedMs    = (uint16_t) (doc["payload"]["params"]["speed"]      | 1000);
            msg.brightness = (uint8_t)  (doc["payload"]["params"]["brightness"] | 255);
            msg.durationMs = (uint32_t) (doc["payload"]["durationMs"]           | 0);
            const char* text = doc["payload"]["params"]["text"] | "";
            strlcpy(msg.text, text, sizeof(msg.text));
            _pendingTestEffect    = msg;
            _hasPendingTestEffect = true;
            DERBY_LOG_F("[WS] test_effect: effect=%s rgb=(%u,%u,%u) speed=%u duration=%u\n",
                        msg.effectName, msg.r, msg.g, msg.b, msg.speedMs, msg.durationMs);
            return;
        }

        if (strcmp(type, "stop_effect") == 0) {
            _pendingStopEffect = true;
            DERBY_LOG_LN("[WS] stop_effect received");
            return;
        }

        // Unhandled types go to the device-specific override.
        _onAppMessage(type, doc);
    }
};

#endif  // NATIVE_TEST
