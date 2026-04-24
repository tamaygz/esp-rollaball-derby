/* eslint-disable no-var */
/**
 * GameEvents — canonical game event string constants.
 *
 * Single source of truth for all event names used across the Derby system.
 * Matches the values emitted by the server in the `scored.events[]` array,
 * `game_event.event`, and related WS messages.
 *
 * Usage:
 *   Browser  — <script src="/shared/js/gameEvents.js">
 *               → window.GameEvents available to all subsequent scripts.
 *   Node.js  — const GameEvents = require('.../clients/shared/js/gameEvents');
 *
 * C++ counterpart: clients/shared/leds/GameEvents.h  (LocalEventType / GlobalEventType enums)
 */
var GameEvents = Object.freeze({

  // ── Per-player score events (inside scored.events[]) ──────────────────────
  ZERO_ROLL:       'zero_roll',
  SCORE_1:         'score_1',
  SCORE_2:         'score_2',
  SCORE_3:         'score_3',

  // ── Per-player streak events (inside scored.events[]) ─────────────────────
  STREAK_ZERO_3X:  'streak_zero_3x',
  STREAK_THREE_2X: 'streak_three_2x',

  // ── Per-player rank-change events (inside scored.events[]) ────────────────
  TOOK_LEAD:       'took_lead',
  BECAME_LAST:     'became_last',

  // ── Global lifecycle events (game_event.event) ─────────────────────────────
  GAME_STARTED:    'game_started',
  GAME_PAUSED:     'game_paused',
  GAME_RESUMED:    'game_resumed',
  GAME_RESET:      'game_reset',

  // ── Other WS message types ─────────────────────────────────────────────────
  COUNTDOWN_TICK:  'countdown_tick',
  WINNER:          'winner',
});

// CommonJS export for Node.js (server-side SoundManager, tests).
// When loaded as a plain <script> tag the global `GameEvents` is used instead.
/* global module */
if (typeof module !== 'undefined' && module.exports) {
  module.exports = GameEvents;
}
