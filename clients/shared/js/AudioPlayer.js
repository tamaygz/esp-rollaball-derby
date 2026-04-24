/* eslint-disable no-var */
/**
 * AudioPlayer — generalized browser audio playback for the Derby system.
 *
 * Cross-platform notes (desktop + iOS/Android/macOS/Windows):
 *   - Uses HTMLAudioElement (not Web Audio) for maximum compatibility and
 *     simplest MP3/WAV/OGG support across mobile browsers.
 *   - iOS/Safari and some Android browsers require the first playback to
 *     originate from a user gesture. We "unlock" on first click/touch/key
 *     by kicking a silent play() on a sentinel element; after that, any
 *     subsequent programmatic play() works.
 *   - Each event gets a small pool of preloaded Audio elements so rapid
 *     consecutive events (e.g. score + took_lead) can overlap without the
 *     previous one being cut off.
 *   - The enabled state is persisted per-browser in localStorage.
 *
 * Usage (browser):
 *   <script src="/shared/js/gameEvents.js"></script>
 *   <script src="/shared/js/AudioPlayer.js"></script>
 *   <script>
 *     DerbyAudio.init({ enabled: true }).then(() => {
 *       DerbyAudio.play(GameEvents.SCORE_3);
 *     });
 *   </script>
 *
 * Config is loaded from GET /api/sounds/config — an object of the form:
 *   { urls: { "<event>": "<absolute or relative URL>", ... } }
 * Unknown events silently no-op.
 */
(function (root) {
  'use strict';

  var STORAGE_KEY = 'derby.audio.enabled';
  var POOL_SIZE   = 3; // simultaneous overlapping plays per event

  // Tiny silent WAV (44 bytes, 8-bit PCM, 1 sample) — used to unlock audio
  // playback on iOS/Safari on first user gesture.
  var SILENT_WAV =
    'data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA=';

  function _loadEnabled(defaultEnabled) {
    try {
      var raw = root.localStorage && root.localStorage.getItem(STORAGE_KEY);
      if (raw === 'true')  return true;
      if (raw === 'false') return false;
    } catch (e) { /* private mode / disabled storage */ }
    return defaultEnabled !== false;
  }

  function _saveEnabled(enabled) {
    try {
      if (root.localStorage) root.localStorage.setItem(STORAGE_KEY, enabled ? 'true' : 'false');
    } catch (e) { /* ignore */ }
  }

  function AudioPlayer() {
    this._urls      = {};     // eventName → url
    this._pools     = {};     // eventName → [HTMLAudioElement]
    this._poolIdx   = {};     // eventName → next index
    this._enabled   = true;
    this._unlocked  = false;
    this._listeners = [];     // change listeners (enabled/config)
    this._configUrl = '/api/sounds/config';
  }

  AudioPlayer.prototype = {

    /**
     * Initialise the player. Fetches config from the server, builds audio
     * element pools, and installs the first-gesture unlock hook.
     * Safe to call multiple times — later calls re-fetch config.
     *
     * @param {object} [options]
     * @param {boolean} [options.enabled]   Initial enabled state (falls back to localStorage, then true).
     * @param {string}  [options.configUrl] Override the config endpoint URL.
     * @returns {Promise<void>}
     */
    init: function (options) {
      options = options || {};
      if (options.configUrl) this._configUrl = options.configUrl;
      this._enabled = _loadEnabled(options.enabled !== undefined ? options.enabled : true);

      var self = this;
      this._installUnlockHandler();

      return this._fetchConfig().then(function (cfg) {
        self._applyConfig(cfg);
      }).catch(function (err) {
        // Config fetch failed — still usable, just with no URLs wired.
        console.warn('[AudioPlayer] config fetch failed:', err && err.message);
      });
    },

    /** @returns {boolean} */
    isEnabled: function () { return this._enabled; },

    /**
     * Enable or disable audio playback. Persists per-browser.
     * Disabled: play() is a no-op.
     */
    setEnabled: function (enabled) {
      enabled = Boolean(enabled);
      if (enabled === this._enabled) return;
      this._enabled = enabled;
      _saveEnabled(enabled);
      this._notify({ enabled: enabled });
    },

    toggle: function () { this.setEnabled(!this._enabled); return this._enabled; },

    /**
     * Play the sound mapped to the given event name. No-op if disabled,
     * unknown, or the underlying URL fails to load/play.
     * @param {string} eventName
     */
    play: function (eventName) {
      if (!this._enabled || !eventName) return;
      var url = this._urls[eventName];
      if (!url) return;

      var pool = this._pools[eventName];
      if (!pool) {
        pool = this._buildPool(url);
        this._pools[eventName]   = pool;
        this._poolIdx[eventName] = 0;
      }

      var idx = this._poolIdx[eventName] % pool.length;
      this._poolIdx[eventName] = idx + 1;
      var audio = pool[idx];

      try {
        audio.currentTime = 0;
      } catch (e) { /* some browsers throw if not yet loaded — safe to ignore */ }

      var p = audio.play();
      if (p && typeof p.catch === 'function') {
        p.catch(function (err) {
          // Autoplay policy can still reject before first gesture. Swallow.
          if (err && err.name !== 'NotAllowedError' && err.name !== 'AbortError') {
            console.warn('[AudioPlayer] play failed for', eventName, err.message);
          }
        });
      }
    },

    /**
     * Register a callback for state changes (enabled, config).
     * @param {function({enabled?: boolean, urls?: object}): void} fn
     * @returns {function(): void} unsubscribe
     */
    onChange: function (fn) {
      if (typeof fn !== 'function') return function () {};
      this._listeners.push(fn);
      var self = this;
      return function () {
        self._listeners = self._listeners.filter(function (l) { return l !== fn; });
      };
    },

    /** Re-fetch config from the server (e.g. after admin saves new URLs). */
    refresh: function () {
      var self = this;
      return this._fetchConfig().then(function (cfg) { self._applyConfig(cfg); });
    },

    // ── internals ─────────────────────────────────────────────────────────────

    _fetchConfig: function () {
      return fetch(this._configUrl, { cache: 'no-cache' }).then(function (r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      });
    },

    _applyConfig: function (cfg) {
      var urls = (cfg && cfg.urls) || {};
      this._urls   = urls;
      this._pools  = {}; // drop old pools so new URLs take effect
      this._poolIdx = {};
      this._notify({ urls: urls });
    },

    _buildPool: function (url) {
      var pool = [];
      for (var i = 0; i < POOL_SIZE; i++) {
        var a = new Audio();
        a.src      = url;
        a.preload  = 'auto';
        // `playsInline` is only meaningful on <video> but harmless here;
        // `crossOrigin` is omitted — HTMLAudioElement doesn't need CORS
        // to play cross-origin audio, only to route it through Web Audio.
        pool.push(a);
      }
      return pool;
    },

    _installUnlockHandler: function () {
      if (this._unlocked) return;
      var self = this;
      var unlock = function () {
        if (self._unlocked) return;
        self._unlocked = true;
        try {
          var a = new Audio(SILENT_WAV);
          a.volume = 0;
          var p = a.play();
          if (p && typeof p.catch === 'function') p.catch(function () { /* ignore */ });
        } catch (e) { /* ignore */ }
        root.removeEventListener('pointerdown', unlock, true);
        root.removeEventListener('keydown',     unlock, true);
        root.removeEventListener('touchstart',  unlock, true);
      };
      root.addEventListener('pointerdown', unlock, true);
      root.addEventListener('keydown',     unlock, true);
      root.addEventListener('touchstart',  unlock, true);
    },

    _notify: function (change) {
      for (var i = 0; i < this._listeners.length; i++) {
        try { this._listeners[i](change); } catch (e) { /* ignore listener errors */ }
      }
    },
  };

  // Singleton — one per page.
  root.DerbyAudio = root.DerbyAudio || new AudioPlayer();

  // CommonJS export (tests).
  /* global module */
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { AudioPlayer: AudioPlayer };
  }
}(typeof window !== 'undefined' ? window : this));
