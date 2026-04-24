#pragma once
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "config.h"
#include <leds/GameEvents.h>

// Maximum number of players whose positions we track simultaneously.
#define WS_MAX_PLAYERS 8

using namespace websockets;

// A player position entry received in the server `state` broadcast.
struct PlayerPosition {
    char  playerId[40];
    float position;       // normalised [0.0, 1.0] lane position
    uint8_t colorIndex;   // Derby palette index — used to match player to lane
};

// WebSocket client for the ESP32 motor peripheral.
// Extends the sensor pattern with:
//   - Motor-specific registration payload (motorCount, motorColors, capabilities)
//   - `state` message handling → player position updates
//   - `sendButton()` method for physical button events
class WSClient {
public:
    // Call once in setup() after WiFi connects.
    // motorCount: number of stepper motors on this device
    // motorColors: array of colorIndex values (one per motor/player lane, up to 8)
    // persistedPlayerId: optional saved device UUID
    void begin(const char* host, uint16_t port, const char* playerName,
               uint8_t motorCount, const uint8_t* motorColors,
               const char* persistedPlayerId = nullptr);

    // Drive the connection state machine. Call every loop().
    void loop();

    // Send a button event to the server.
    // buttonIdx: hardware button index (0 = BUTTON_1, 1 = BUTTON_2)
    // action: "start" | "reset" | "pause" | "resume"
    void sendButton(uint8_t buttonIdx, const char* action);

    // Poll for updated player positions (from latest `state` message).
    // Returns true and fills out[] with up to WS_MAX_PLAYERS entries if positions changed.
    bool pollPositions(PlayerPosition out[], uint8_t& count);

    // Return and clear the pending game-global event.
    GlobalEventType pollGlobalEvent();

    // Poll for a pending led_config message.
    bool pollLedConfig(LedConfig& out);

    // Poll for a pending test_effect message.
    bool pollTestEffect(LedTestEffectMessage& out);

    // Update LED metadata for re-registrations (call if LED config changes).
    void setLedMetadata(uint16_t ledCount);

    // Notify the WS client that WiFi was lost.
    void onWiFiLost();

    bool         isConnected() const { return _connected; }
    const String& getPlayerId() const { return _playerId; }

private:
    WebsocketsClient _client;
    bool             _connected   = false;
    String           _playerId;
    String           _playerName;
    String           _host;
    uint16_t         _port        = 3000;

    uint8_t  _motorCount         = 0;
    uint8_t  _motorColors[WS_MAX_PLAYERS] = {};
    uint16_t _ledMetadataCount   = 0;

    unsigned long _lastAttempt   = 0;
    unsigned long _backoffMs     = WS_BACKOFF_MIN_MS;

    GlobalEventType  _pendingGlobalEvent     = GlobalEventType::NONE;

    LedConfig    _pendingLedConfig       = {};
    bool         _hasPendingLedConfig    = false;

    LedTestEffectMessage _pendingTestEffect     = {};
    bool                 _hasPendingTestEffect  = false;

    PlayerPosition _positions[WS_MAX_PLAYERS] = {};
    uint8_t        _positionCount = 0;
    bool           _positionsDirty = false;

    void _connect();
    void _sendRegister();
    void _onMessage(WebsocketsMessage msg);
    void _onEvent(WebsocketsEvent event, String data);
};
