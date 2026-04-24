#pragma once
#include <Arduino.h>
#include "config.h"
#include <leds/GameEvents.h>
#include <leds/EventQueue.h>
#include <ws_client_base.h>

// Maximum number of players whose positions we track simultaneously.
#define WS_MAX_PLAYERS 8

// A player position entry received in the server `state` broadcast.
struct PlayerPosition {
    char    playerId[40];
    float   position;       // normalised [0.0, 1.0] lane position
    uint8_t colorIndex;     // Derby palette index — used to match player to lane
};

// WebSocket client for the ESP32 motor peripheral.
// Extends WSClientBase (shared connection/poll/LED machinery) with:
//   - Motor-specific registration payload (motorCount, motorColors, capabilities)
//   - `state` message handling → player position updates
//   - `sendButton()` method for physical button events
class WSClient : public WSClientBase {
public:
    // Call once in setup() after WiFi connects.
    // motorCount: number of stepper motors on this device
    // motorColors: array of colorIndex values (one per motor/player lane, up to 8)
    // persistedPlayerId: optional saved device UUID
    void begin(const char* host, uint16_t port, const char* playerName,
               uint8_t motorCount, const uint8_t* motorColors,
               const char* persistedPlayerId = nullptr);

    // Send a button event to the server.
    // buttonIdx: hardware button index (0 = BUTTON_1, 1 = BUTTON_2)
    // action: "start" | "reset" | "pause" | "resume"
    void sendButton(uint8_t buttonIdx, const char* action);

    // Poll for updated player positions (from latest `state` message).
    // Returns true and fills out[] with up to WS_MAX_PLAYERS entries if positions changed.
    bool pollPositions(PlayerPosition out[], uint8_t& count);

protected:
    void _sendRegister() override;
    void _onAppMessage(const char* type, JsonDocument& doc) override;
    LedTopology _defaultTopology() const override { return LedTopology::MATRIX_ZIGZAG; }

private:
    uint8_t        _motorCount                  = 0;
    uint8_t        _motorColors[WS_MAX_PLAYERS] = {};
    PlayerPosition _positions[WS_MAX_PLAYERS]   = {};
    uint8_t        _positionCount               = 0;
    bool           _positionsDirty              = false;
};
