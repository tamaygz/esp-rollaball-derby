#pragma once
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include "config.h"

using namespace websockets;

// Device-local events: only the owning device reacts (from 'scored' messages).
// Ordered by priority (higher enum value = higher priority for event selection).
enum class LocalEvent {
    NONE,
    BECAME_LAST,       // this player dropped to last place
    ZERO_ROLL,         // this player scored 0
    STREAK_ZERO,       // 3 consecutive zero rolls
    SCORE_PLUS1,       // this player scored +1
    SCORE_PLUS2,       // this player scored +2
    SCORE_PLUS3,       // this player scored +3
    STREAK_THREE,      // 2 consecutive +3 rolls
    TOOK_LEAD,         // this player just took the lead
};

// Game-global events: all devices react (from 'game_event', 'countdown', 'winner').
enum class GlobalEvent {
    NONE,
    COUNTDOWN_TICK,    // countdown tick (count >= 1)
    GAME_STARTED,      // game transitioned to running
    GAME_PAUSED,       // game paused
    GAME_RESUMED,      // game resumed from pause
    GAME_RESET,        // game reset to idle
    WINNER_SELF,       // this sensor's player won
    WINNER_OTHER,      // another player won
};

// LED test-effect command received from the server via the admin web UI.
// Color is stored as raw RGB bytes to avoid a NeoPixelBus dependency in this header.
struct LedTestEffectMessage {
    char    effectName[16]; // "solid", "blink", "pulse", "rainbow", "chase", "sparkle"
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t speedMs;       // Period / cycle speed in milliseconds
    uint8_t  brightness;    // 0–255
};

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
    LocalEvent pollLocalEvent();

    // Return and clear the pending game-global event (NONE if nothing queued).
    GlobalEvent pollGlobalEvent();

    // Poll for a pending led_config message. Returns true and fills `out` if one
    // has arrived since the last call; false otherwise.
    bool pollLedConfig(LedConfig& out);

    // Poll for a pending test_effect message. Returns true and fills `out` if one
    // has arrived since the last call; false otherwise.
    bool pollTestEffect(LedTestEffectMessage& out);

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

    LocalEvent       _pendingLocalEvent  = LocalEvent::NONE;
    GlobalEvent      _pendingGlobalEvent = GlobalEvent::NONE;

    // Pending LED config message (from server led_config broadcast)
    LedConfig        _pendingLedConfig      = {};
    bool             _hasPendingLedConfig   = false;

    // Pending LED test-effect message (from server test_effect command)
    LedTestEffectMessage _pendingTestEffect    = {};
    bool                 _hasPendingTestEffect = false;

    // LED metadata sent in registration
    uint16_t         _ledMetadataCount = LED_DEFAULT_COUNT;

    void _connect();
    void _sendRegister();
    void _onMessage(WebsocketsMessage msg);
    void _onEvent(WebsocketsEvent event, String data);
};
