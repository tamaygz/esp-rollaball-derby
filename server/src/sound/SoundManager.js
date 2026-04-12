'use strict';

const path = require('path');
const fs   = require('fs');

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
// Names match the SoundEvent enum from the removed ESP32 sound.h.
const EVENT_FILE_MAP = {
  score_0:        'score_0.wav',
  score_1:        'score_1.wav',
  score_2:        'score_2.wav',
  score_3:        'score_3.wav',
  game_started:   'game_started.wav',
  game_paused:    'game_paused.wav',
  game_resumed:   'game_resumed.wav',
  game_reset:     'game_reset.wav',
  countdown_tick: 'countdown_tick.wav',
  countdown_go:   'countdown_go.wav',
  winner:         'winner.wav',
  took_lead:      'took_lead.wav',
  became_last:    'became_last.wav',
  streak_zero:    'streak_zero.wav',
  streak_three:   'streak_three.wav',
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
