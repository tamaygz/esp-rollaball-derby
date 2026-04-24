'use strict';

const fs = require('fs/promises');
const fsSync = require('fs');
const path = require('path');
const { EventEmitter } = require('events');

const GameEvents = require(path.join(__dirname, '..', '..', '..', 'clients', 'shared', 'js', 'gameEvents'));

/**
 * Canonical list of all audio-playable event names. Mirrors the server-side
 * SoundManager.EVENT_FILE_MAP so the admin UI can display a complete, stable
 * list regardless of which files currently exist on disk.
 *
 *   <eventName>: <default filename under server/sounds/>
 */
const EVENT_FILE_MAP = {
  [GameEvents.ZERO_ROLL]:       'score_0.wav',
  [GameEvents.SCORE_1]:         'score_1.wav',
  [GameEvents.SCORE_2]:         'score_2.wav',
  [GameEvents.SCORE_3]:         'score_3.wav',
  [GameEvents.STREAK_ZERO_3X]:  'streak_zero.wav',
  [GameEvents.STREAK_THREE_2X]: 'streak_three.wav',
  [GameEvents.TOOK_LEAD]:       'took_lead.wav',
  [GameEvents.BECAME_LAST]:     'became_last.wav',
  [GameEvents.GAME_STARTED]:    'game_started.wav',
  [GameEvents.GAME_PAUSED]:     'game_paused.wav',
  [GameEvents.GAME_RESUMED]:    'game_resumed.wav',
  [GameEvents.GAME_RESET]:      'game_reset.wav',
  [GameEvents.COUNTDOWN_TICK]:  'countdown_tick.wav',
  [GameEvents.WINNER]:          'winner.wav',
  countdown_go:                 'countdown_go.wav',
};

/**
 * Reject anything that isn't plainly an http(s) URL or a site-relative path.
 * Blocks `javascript:`/`data:`/etc. — OWASP-style URL validation.
 * @param {string} url
 * @returns {boolean}
 */
function isSafeUrl(url) {
  if (typeof url !== 'string' || url.length === 0) return false;
  if (url.length > 2048) return false;
  if (url.startsWith('/')) return true;                 // site-relative
  try {
    const parsed = new URL(url);
    return parsed.protocol === 'http:' || parsed.protocol === 'https:';
  } catch (e) {
    return false;
  }
}

/**
 * SoundConfigManager
 * Persists admin-configurable URL overrides for each audio event.
 * Resolution rule: override URL wins; otherwise `/sounds/<default-filename>`
 * if the file exists on disk; otherwise no URL (event stays silent).
 */
class SoundConfigManager extends EventEmitter {
  constructor(options = {}) {
    super();
    this.configFilePath = options.configFilePath
      || path.join(__dirname, '..', '..', 'data', 'sound-config.json');
    this.soundsDir = options.soundsDir
      || path.join(__dirname, '..', '..', 'sounds');
    /** @type {{urls: Object<string,string>}} */
    this.config = { urls: {} };
    this._loaded = false;
  }

  /** Load config from disk. Missing file → empty overrides (not an error). */
  async loadConfig() {
    try {
      const raw = await fs.readFile(this.configFilePath, 'utf8');
      const parsed = JSON.parse(raw);
      this.config = {
        urls: (parsed && typeof parsed.urls === 'object' && parsed.urls) ? parsed.urls : {},
      };
      console.log('[SoundConfig] loaded from', this.configFilePath);
    } catch (err) {
      if (err.code !== 'ENOENT') {
        console.error('[SoundConfig] load failed, using empty overrides:', err.message);
      }
      this.config = { urls: {} };
    }
    this._loaded = true;
    return this.config;
  }

  /** Atomic write to disk. Validates URL shape before persisting. */
  async saveConfig(newUrls) {
    const urls = {};
    if (newUrls && typeof newUrls === 'object') {
      for (const [event, url] of Object.entries(newUrls)) {
        if (!(event in EVENT_FILE_MAP)) continue;       // unknown event — drop
        if (url === '' || url === null || url === undefined) continue; // cleared
        if (!isSafeUrl(url)) {
          throw new Error(`Invalid URL for event '${event}'`);
        }
        urls[event] = url;
      }
    }

    const next = { urls };
    const tmp  = this.configFilePath + '.tmp';
    await fs.mkdir(path.dirname(this.configFilePath), { recursive: true });
    await fs.writeFile(tmp, JSON.stringify(next, null, 2), 'utf8');
    await fs.rename(tmp, this.configFilePath);
    this.config = next;
    this.emit('configChanged', next);
    return next;
  }

  /**
   * Build the full resolved map the browser uses.
   *   { events: [...], urls: { event: url }, overrides: { event: url }, defaults: { event: url|null } }
   * `urls` is the authoritative map each client should consume.
   */
  getClientConfig() {
    const events    = Object.keys(EVENT_FILE_MAP);
    const overrides = { ...(this.config.urls || {}) };
    const defaults  = {};
    const urls      = {};
    for (const event of events) {
      const filename = EVENT_FILE_MAP[event];
      const filePath = path.join(this.soundsDir, filename);
      const hasFile  = fsSync.existsSync(filePath);
      defaults[event] = hasFile ? `/sounds/${encodeURIComponent(filename)}` : null;
      if (overrides[event]) {
        urls[event] = overrides[event];
      } else if (hasFile) {
        urls[event] = defaults[event];
      }
    }
    return { events, urls, overrides, defaults };
  }
}

module.exports = SoundConfigManager;
module.exports.EVENT_FILE_MAP = EVENT_FILE_MAP;
module.exports.isSafeUrl = isSafeUrl;
