#pragma once
#include <stdint.h>

/**
 * GameEvents — canonical game-event types and LED test-effect message.
 *
 * Shared between all firmware clients and the GameEventMapper so there is a
 * single definition rather than parallel enums that must be translated.
 *
 * Previously each client's websocket.h redeclared GlobalEvent / LocalEvent
 * as client-local enums; the shared GameEventMapper used GlobalEventType /
 * LocalEventType; callers had to convert between them.  These canonical types
 * replace both sets.
 */

// Device-local events: only the owning device reacts (from 'scored' messages).
// Ordered by priority — higher enum value = higher priority for event selection.
enum class LocalEventType {
    NONE,
    BECAME_LAST,    // this player dropped to last place
    ZERO_ROLL,      // this player scored 0
    STREAK_ZERO,    // 3 consecutive zero rolls
    SCORE_PLUS1,    // this player scored +1
    SCORE_PLUS2,    // this player scored +2
    SCORE_PLUS3,    // this player scored +3
    STREAK_THREE,   // 2 consecutive +3 rolls
    TOOK_LEAD,      // this player just took the lead
};

// Game-global events: all devices react (from 'game_event', 'countdown', 'winner').
enum class GlobalEventType {
    NONE,
    COUNTDOWN_TICK,  // countdown tick (count >= 1)
    GAME_STARTED,    // game transitioned to running
    GAME_PAUSED,     // game paused
    GAME_RESUMED,    // game resumed from pause
    GAME_RESET,      // game reset to idle
    WINNER_SELF,     // this device's player won
    WINNER_OTHER,    // another player won
};

// LED test-effect command received from server via the admin web UI.
// Color is stored as raw RGB bytes to avoid a NeoPixelBus dependency here.
struct LedTestEffectMessage {
    char     effectName[16]; // "solid", "blink", "pulse", "rainbow", "chase", "sparkle"
    uint8_t  r;
    uint8_t  g;
    uint8_t  b;
    uint16_t speedMs;        // Period / cycle speed in milliseconds
    uint8_t  brightness;     // 0–255
    uint16_t durationMs;     // Auto-stop duration in ms; 0 = indefinite
    char     text[32];       // Optional text payload (used by "text" effect)
};
