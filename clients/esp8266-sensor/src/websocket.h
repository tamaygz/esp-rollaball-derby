#pragma once
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "config.h"

using namespace websockets;

// WebSocket client wrapper for the Derby sensor firmware.
// Handles:
//   - Initial connection and re-connection with exponential backoff
//   - Registration handshake (sends register, stores assigned playerId)
//   - Score event transmission
//   - Non-blocking operation via poll()
class WSClient {
public:
    // Configure connection parameters. Call once in setup() after WiFi connects.
    void begin(const char* host, uint16_t port, const char* playerName);

    // Drive the WebSocket state machine. Call every loop() iteration.
    void loop();

    // Send a score event. Does nothing if not connected or playerId not yet assigned.
    void sendScore(int points);

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
    unsigned long    _lastAttempt = 0;   // 0 = connect immediately on first loop
    unsigned long    _backoffMs   = WS_BACKOFF_MIN_MS;

    void _connect();
    void _sendRegister();
    void _onMessage(WebsocketsMessage msg);
    void _onEvent(WebsocketsEvent event, String data);
};
