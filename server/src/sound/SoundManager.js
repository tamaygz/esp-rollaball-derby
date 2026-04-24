'use strict';

const path = require('path');
const fs   = require('fs');

const GameEvents = require(path.join(__dirname, '..', '..', '..', 'clients', 'shared', 'js', 'gameEvents'));

// play-sound wraps OS audio players:
//   macOS   → afplay
//   Linux   → aplay, mpg123, or mplayer
//   Windows → PowerShell (Windows Media Player COM)
let createPlayer;
try {
  createPlayer = require('play-sound');
} catch {
  // play-sound not installed — all playback calls will be silent no-ops.
  createPlayer = null;
}

// Maps logical event names → WAV filenames (relative to soundsDir).
//
// All keys use canonical GameEvents string values where possible so they can
// be looked up directly from the scored.events[] array.
//
// Sound-system-internal keys (no WS event equivalent):
//   countdown_go — played when countdown reaches 0; not a WS event name.
const EVENT_FILE_MAP = {
  // ── Per-player score events (GameEvents.SCORE_* / ZERO_ROLL) ─────────────
  [GameEvents.ZERO_ROLL]:      'score_0.wav',
  [GameEvents.SCORE_1]:        'score_1.wav',
  [GameEvents.SCORE_2]:        'score_2.wav',
  [GameEvents.SCORE_3]:        'score_3.wav',
  // ── Streak / rank events ──────────────────────────────────────────────────
  [GameEvents.STREAK_ZERO_3X]:  'streak_zero.wav',
  [GameEvents.STREAK_THREE_2X]: 'streak_three.wav',
  [GameEvents.TOOK_LEAD]:       'took_lead.wav',
  [GameEvents.BECAME_LAST]:     'became_last.wav',
  // ── Global lifecycle events ───────────────────────────────────────────────
  [GameEvents.GAME_STARTED]:    'game_started.wav',
  [GameEvents.GAME_PAUSED]:     'game_paused.wav',
  [GameEvents.GAME_RESUMED]:    'game_resumed.wav',
  [GameEvents.GAME_RESET]:      'game_reset.wav',
  [GameEvents.COUNTDOWN_TICK]:  'countdown_tick.wav',
  [GameEvents.WINNER]:          'winner.wav',
  // ── Sound-only key (no WS event equivalent) ───────────────────────────────
  countdown_go:                 'countdown_go.wav',
};

class SoundManager {
  /**
   * @param {string} soundsDir - Absolute path to the directory containing WAV files.
   * @param {object} [options]
   * @param {boolean} [options.enabled] - Defaults to SOUND_ENABLED env var (default true).
   * @param {string}  [options.player]  - Override OS audio player binary (e.g. 'aplay').
   */
  constructor(soundsDir, options = {}) {
    this._soundsDir = soundsDir;
    this._enabled   = options.enabled !== undefined
      ? Boolean(options.enabled)
      : (process.env.SOUND_ENABLED || 'true').toLowerCase() !== 'false';

    const playerOpt = options.player || process.env.SOUND_PLAYER || undefined;
    this._player = (createPlayer && this._enabled)
      ? createPlayer(playerOpt ? { player: playerOpt } : {})
      : null;

    if (this._enabled && !createPlayer) {
      console.warn('[Sound] play-sound is not installed — audio playback disabled. Run: npm install play-sound');
    }

    // Validate that every GameEvents value has a sound mapping.
    // 'countdown_go' is intentionally absent from GameEvents: it is a sound-system
    // internal event (played when the countdown ends) with no corresponding WS
    // message type — firmware handles the countdown animation independently.
    // Any GameEvents value that IS missing from EVENT_FILE_MAP is a bug to fix.
    const gameEventValues = new Set(Object.values(GameEvents));
    const unmapped = [...gameEventValues].filter((v) => !(v in EVENT_FILE_MAP));
    if (unmapped.length > 0) {
      console.warn('[Sound] GameEvents values with no EVENT_FILE_MAP entry:', unmapped);
    }
  }

  /**
   * Play the WAV file that corresponds to the given event name.
   * No-ops silently if: sound is disabled, file is missing, or player unavailable.
   * @param {string} eventName - Logical event name from EVENT_FILE_MAP.
   */
  play(eventName) {
    if (!this._enabled || !this._player) return;

    const filename = EVENT_FILE_MAP[eventName];
    if (!filename) return; // unknown event → ignore

    const filePath = path.join(this._soundsDir, filename);
    if (!fs.existsSync(filePath)) {
      // Expected during setup — don't spam the log, just skip.
      return;
    }

    this._player.play(filePath, (err) => {
      if (err) {
        console.warn(`[Sound] Failed to play '${filename}': ${err.message}`);
      }
    });
  }
}

module.exports = SoundManager;
