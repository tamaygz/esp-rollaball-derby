#pragma once
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "config.h"
#include <leds/GameEvents.h>
#include <leds/EventQueue.h>

using namespace websockets;

// WebSocket client wrapper for the Derby sensor firmware.
// Handles:
//   - Initial connection and re-connection with exponential backoff
//   - Registration handshake (sends register, stores assigned playerId)
//   - Score event transmission
//   - LED config and test-effect message reception
//   - Non-blocking operation via poll()
class WSClient {
public:
    // Configure connection parameters. Call once in setup() after WiFi connects.
    // persistedPlayerId is optional — pass the saved UUID from LittleFS so the
    // device can re-register with the server after a power cycle.
    void begin(const char* host, uint16_t port, const char* playerName,
               const char* persistedPlayerId = nullptr);

    // Drive the WebSocket state machine. Call every loop() iteration.
    void loop();

    // Send a score event. Does nothing if not connected or playerId not yet assigned.
    void sendScore(int points);

    // Return and clear the pending device-local event (NONE if nothing queued).
    LocalEventType pollLocalEvent();

    // Return and clear the pending game-global event (NONE if nothing queued).
    GlobalEventType pollGlobalEvent();

    // Poll for a pending led_config message. Returns true and fills `out` if one
    // has arrived since the last call; false otherwise.
    bool pollLedConfig(LedConfig& out);

    // Poll for a pending test_effect message. Returns true and fills `out` if one
    // has arrived since the last call; false otherwise.
    bool pollTestEffect(LedTestEffectMessage& out);

    // Returns true (once) if a stop_effect message arrived since the last call.
    bool pollStopEffect();

    // Inform the client of the current LED count to include in the registration
    // message (and in re-registrations after reconnect).
    void setLedMetadata(uint16_t ledCount);

    bool           isConnected()  const { return _connected; }
    const String&  getPlayerId()  const { return _playerId;  }

    // Call when WiFi is lost. Resets connection state so reconnect starts fresh
    // when WiFi recovers.
    void onWiFiLost();

private:
    WebsocketsClient _client;
    bool             _connected   = false;
    String           _playerId;
    String           _playerName;
    String           _host;
    uint16_t         _port        = 3000;

    // Reconnect backoff
    unsigned long    _lastAttempt  = 0;
    unsigned long    _backoffMs    = WS_BACKOFF_MIN_MS;

    EventQueue<LocalEventType,  4> _localQueue;
    EventQueue<GlobalEventType, 4> _globalQueue;

    // Pending LED config message (from server led_config broadcast)
    LedConfig        _pendingLedConfig      = {};
    bool             _hasPendingLedConfig   = false;

    // Pending LED test-effect message (from server test_effect command)
    LedTestEffectMessage _pendingTestEffect    = {};
    bool                 _hasPendingTestEffect = false;
    bool                 _pendingStopEffect    = false;

    // LED metadata sent in registration
    uint16_t         _ledMetadataCount = LED_DEFAULT_COUNT;

    void _connect();
    void _sendRegister();
    void _onMessage(WebsocketsMessage msg);
    void _onEvent(WebsocketsEvent event, String data);
};
