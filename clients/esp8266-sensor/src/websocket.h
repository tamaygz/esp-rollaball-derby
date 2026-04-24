#pragma once
#include <Arduino.h>
#include "config.h"
#include <leds/GameEvents.h>
#include <leds/EventQueue.h>
#include <ws_client_base.h>

// WebSocket client for the Derby sensor firmware.
// Extends WSClientBase (shared connection/poll/LED machinery) with:
//   - Sensor registration payload (ledCapabilities, chipId, chipType)
//   - Score event transmission
//   - Local event handling (scored messages → priority queue of LocalEventType)
class WSClient : public WSClientBase {
public:
    // Configure connection parameters. Call once in setup() after WiFi connects.
    // persistedPlayerId is optional — pass the saved UUID from LittleFS so the
    // device can re-register with the server after a power cycle.
    void begin(const char* host, uint16_t port, const char* playerName,
               const char* persistedPlayerId = nullptr);

    // Send a score event. Does nothing if not connected or playerId not yet assigned.
    void sendScore(int points);

    // Return and clear the oldest pending device-local event (NONE if nothing queued).
    LocalEventType pollLocalEvent();

protected:
    void _sendRegister() override;
    void _onAppMessage(const char* type, JsonDocument& doc) override;

private:
    // Local event queue — bounded FIFO with priority-based overflow eviction.
    // Sensor-specific: only the owning device reacts to local scoring events.
    EventQueue<LocalEventType, 4> _localQueue;
};
