/* eslint-disable no-var */
/**
 * SoundDecision — shared logic for picking *which* sound to play for a
 * Derby game event (or bundle of events). Single source of truth used by:
 *   - Node server   (server/src/ws/ConnectionManager.js → SoundManager.play)
 *   - Display SPA   (clients/display/js/main.js → DerbyAudio.play)
 *   - Web admin     (if it ever plays previews)
 *   - Any future client (e.g. esp32-motor companion page)
 *
 * Rules (in order of priority within a `scored` bundle):
 *   1. `took_lead` / `became_last`    — rank-change trumps score
 *   2. `streak_three_2x` / `streak_zero_3x`
 *   3. `score_N` / `zero_roll`        — base score event
 *
 * For lifecycle events (game_started, game_paused, …), winner, and
 * countdown_tick, the event IS the sound event — no priority resolution
 * needed; callers pass the event name directly to the player.
 */
(function (root) {
  'use strict';

  // Avoid hard dependency on GameEvents global: use string literals that
  // match clients/shared/js/gameEvents.js verbatim.
  var PRIORITY = [
    'took_lead',
    'became_last',
    'streak_three_2x',
    'streak_zero_3x',
  ];

  /**
   * Pick the single sound-event name to play from a `scored` payload.
   *
   * @param {{events?: string[], points?: number}} payload
   * @returns {string|null} event name, or null if no sound applies.
   */
  function pickScoredSound(payload) {
    if (!payload) return null;
    var events = Array.isArray(payload.events) ? payload.events : [];
    for (var i = 0; i < PRIORITY.length; i++) {
      if (events.indexOf(PRIORITY[i]) !== -1) return PRIORITY[i];
    }
    var p = payload.points;
    if (p === 0) return 'zero_roll';
    if (p === 1 || p === 2 || p === 3) return 'score_' + p;
    return null;
  }

  var api = { pickScoredSound: pickScoredSound, PRIORITY: PRIORITY };

  if (typeof module !== 'undefined' && module.exports) {
    module.exports = api;
  } else {
    root.SoundDecision = api;
  }
}(typeof window !== 'undefined' ? window : this));
