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
 * Background music support:
 *   - Lobby music plays on loop when no game is active.
 *   - Game music randomly cycles through a per-theme track list.
 *   - Falls back to the fallback list if no theme/lobby tracks are configured.
 *   - Volumes are controlled independently per channel (effects, lobby, game).
 *   - Game pause ducks background music to 50%.
 *
 * Usage (browser):
 *   <script src="/shared/js/gameEvents.js"></script>
 *   <script src="/shared/js/AudioPlayer.js"></script>
 *   <script>
 *     DerbyAudio.init().then(() => {
 *       DerbyAudio.startLobby();
 *     });
 *   </script>
 *
 * Config is loaded from GET /api/sounds/config.
 */
(function (root) {
  'use strict';

  var STORAGE_KEY = 'derby.audio.enabled';
  var POOL_SIZE   = 3; // simultaneous overlapping plays per effect event
  var FADE_TICK   = 50; // ms between fade steps

  // Tiny silent WAV used to unlock audio on iOS/Safari on first user gesture.
  var SILENT_WAV =
    'data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQAAAAA=';

  // ── BackgroundPlayer ──────────────────────────────────────────────────────────
  // Plays a list of tracks in a shuffled loop. Supports duck (50% volume),
  // volume updates, and a smooth fade-out.

  function BackgroundPlayer() {
    this._el       = null;    // current HTMLAudioElement
    this._tracks   = [];      // current URL list
    this._order    = [];      // shuffled index array
    this._orderPos = 0;       // position in _order
    this._baseVol  = 0.5;     // configured base volume
    this._ducked   = false;   // is 50% duck active?
    this._playing  = false;
    this._fadeId   = null;    // setInterval handle for fade-out
  }

  BackgroundPlayer.prototype = {

    /**
     * Start playing the given tracks array, shuffled, on continuous loop.
     * Replaces any currently-playing music immediately.
     * @param {string[]} tracks
     * @param {number}   volume  0–1 base volume
     */
    play: function (tracks, volume) {
      this.stop();
      if (!tracks || !tracks.length) return;
      this._tracks  = tracks.slice();
      this._baseVol = (typeof volume === 'number') ? Math.max(0, Math.min(1, volume)) : 0.5;
      this._ducked  = false;
      this._reshuffle();
      this._playing = true;
      this._playAt(0);
    },

    /** Stop immediately. */
    stop: function () {
      this._playing = false;
      if (this._fadeId) { clearInterval(this._fadeId); this._fadeId = null; }
      if (this._el) {
        try { this._el.pause(); this._el.src = ''; } catch (e) { /* ignore */ }
        this._el = null;
      }
    },

    /**
     * Smoothly reduce volume to 0 over durationMs, then stop.
     * @param {number}   durationMs
     * @param {function} [onDone]
     */
    fadeOut: function (durationMs, onDone) {
      if (!this._el || !this._playing) { this.stop(); if (onDone) onDone(); return; }
      if (this._fadeId) { clearInterval(this._fadeId); this._fadeId = null; }
      var self     = this;
      var startVol = this._el.volume;
      var steps    = Math.max(1, Math.round(durationMs / FADE_TICK));
      var decr     = startVol / steps;
      this._fadeId = setInterval(function () {
        if (!self._el) { clearInterval(self._fadeId); self._fadeId = null; if (onDone) onDone(); return; }
        var v = Math.max(0, self._el.volume - decr);
        try { self._el.volume = v; } catch (e) { /* ignore */ }
        if (v <= 0) {
          clearInterval(self._fadeId); self._fadeId = null;
          self.stop();
          if (onDone) onDone();
        }
      }, FADE_TICK);
    },

    /** Reduce to 50% of base volume (game pause). */
    duck: function () {
      if (this._ducked) return;
      this._ducked = true;
      this._applyVol();
    },

    /** Restore full base volume. */
    unduck: function () {
      if (!this._ducked) return;
      this._ducked = false;
      this._applyVol();
    },

    /**
     * Update the base volume while possibly playing.
     * @param {number} v  0–1
     */
    setVolume: function (v) {
      this._baseVol = (typeof v === 'number') ? Math.max(0, Math.min(1, v)) : 0.5;
      this._applyVol();
    },

    /** @returns {boolean} */
    isPlaying: function () { return this._playing; },

    // ── internals ──────────────────────────────────────────────────────────────

    _applyVol: function () {
      if (this._el) {
        var vol = this._ducked ? this._baseVol * 0.5 : this._baseVol;
        try { this._el.volume = vol; } catch (e) { /* ignore */ }
      }
    },

    _reshuffle: function () {
      var n = this._tracks.length;
      this._order = [];
      for (var i = 0; i < n; i++) this._order.push(i);
      // Fisher-Yates shuffle
      for (var j = n - 1; j > 0; j--) {
        var k = Math.floor(Math.random() * (j + 1));
        var tmp = this._order[j]; this._order[j] = this._order[k]; this._order[k] = tmp;
      }
      this._orderPos = 0;
    },

    _playAt: function (orderPos) {
      if (!this._playing || !this._tracks.length) return;
      var self = this;
      var idx  = this._order[orderPos % this._order.length];
      var url  = this._tracks[idx];

      var el = new Audio();
      el.src     = url;
      el.preload = 'auto';
      el.volume  = this._ducked ? this._baseVol * 0.5 : this._baseVol;
      this._el   = el;

      el.addEventListener('ended', function () {
        if (!self._playing) return;
        var next = orderPos + 1;
        if (next >= self._order.length) { self._reshuffle(); next = 0; }
        self._playAt(next);
      });

      el.addEventListener('error', function () {
        if (!self._playing) return;
        var next = orderPos + 1;
        if (next >= self._order.length) { self._reshuffle(); next = 0; }
        self._playAt(next);
      });

      var p = el.play();
      if (p && typeof p.catch === 'function') {
        p.catch(function (err) {
          if (!self._playing) return;
          // Skip on NotAllowedError (autoplay policy) or AbortError — advance to next
          if (err && (err.name === 'NotAllowedError' || err.name === 'AbortError')) return;
          var next = orderPos + 1;
          if (next >= self._order.length) { self._reshuffle(); next = 0; }
          self._playAt(next);
        });
      }
    },
  };

  // ── Helpers ───────────────────────────────────────────────────────────────────

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

  // ── AudioPlayer ───────────────────────────────────────────────────────────────

  function AudioPlayer() {
    this._urls      = {};     // eventName → effect URL
    this._pools     = {};     // eventName → [HTMLAudioElement]
    this._poolIdx   = {};     // eventName → next pool index
    this._enabled   = true;
    this._unlocked  = false;
    this._listeners = [];     // onChange subscribers
    this._configUrl = '/api/sounds/config';

    // Background music engine
    this._bg           = new BackgroundPlayer();
    this._bgMode       = null;  // 'lobby' | 'game' | null

    // Volume config (from server, applied after init)
    this._volumes      = { lobby: 0.6, game: 0.5, effects: 0.9 };

    // Track lists (from server config)
    this._lobbyTracks    = [];
    this._themeMusic     = {};
    this._fallbackTracks = [];
  }

  AudioPlayer.prototype = {

    /**
     * Initialise the player. Fetches config, builds effect pools, installs
     * first-gesture unlock. Safe to call multiple times (re-fetches config).
     *
     * @param {object}  [options]
     * @param {boolean} [options.enabled]
     * @param {string}  [options.configUrl]
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
        console.warn('[AudioPlayer] config fetch failed:', err && err.message);
      });
    },

    /** @returns {boolean} */
    isEnabled: function () { return this._enabled; },

    setEnabled: function (enabled) {
      enabled = Boolean(enabled);
      if (enabled === this._enabled) return;
      this._enabled = enabled;
      _saveEnabled(enabled);
      if (!enabled) this._bg.stop();
      this._notify({ enabled: enabled });
    },

    toggle: function () { this.setEnabled(!this._enabled); return this._enabled; },

    /**
     * Play a one-shot sound effect for the given event name.
     * Respects effects volume. No-op if disabled or event unknown.
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

      try { audio.currentTime = 0; } catch (e) { /* ignore */ }

      var p = audio.play();
      if (p && typeof p.catch === 'function') {
        p.catch(function (err) {
          if (err && err.name !== 'NotAllowedError' && err.name !== 'AbortError') {
            console.warn('[AudioPlayer] play failed for', eventName, err.message);
          }
        });
      }
    },

    // ── Background music public API ───────────────────────────────────────────

    /**
     * Start lobby music (looping, shuffled).
     * Uses lobbyMusic tracks, falls back to fallbackMusic.
     * No-op when disabled or no tracks configured.
     */
    startLobby: function () {
      if (!this._enabled) return;
      var tracks = this._lobbyTracks.length
        ? this._lobbyTracks
        : this._fallbackTracks;
      if (!tracks.length) return;
      this._bgMode = 'lobby';
      this._bg.play(tracks, this._volumes.lobby);
    },

    /**
     * Start game background music for the given theme, cycling randomly.
     * Falls back to fallbackMusic if theme has no tracks configured.
     * No-op when disabled or no tracks available.
     * @param {string} [theme]  e.g. 'horse', 'camel', 'reef'
     */
    startGameMusic: function (theme) {
      if (!this._enabled) return;
      var themeTracks = (theme && this._themeMusic[theme]) ? this._themeMusic[theme] : [];
      var tracks = themeTracks.length ? themeTracks : this._fallbackTracks;
      if (!tracks.length) return;
      this._bgMode = 'game';
      this._bg.play(tracks, this._volumes.game);
    },

    /**
     * Stop background music.
     * @param {object}  [options]
     * @param {boolean} [options.fade=false]
     * @param {number}  [options.durationMs=2000]
     */
    stopBackgroundMusic: function (options) {
      var fade       = options && options.fade;
      var durationMs = (options && options.durationMs) || 2000;
      if (fade) {
        this._bg.fadeOut(durationMs);
      } else {
        this._bg.stop();
      }
      this._bgMode = null;
    },

    /**
     * Fade out background music over durationMs then stop.
     * @param {number} [durationMs=2000]
     */
    fadeOutBackgroundMusic: function (durationMs) {
      var self = this;
      this._bg.fadeOut(typeof durationMs === 'number' ? durationMs : 2000, function () {
        self._bgMode = null;
      });
    },

    /**
     * Temporarily reduce background music to 50% of configured volume
     * (e.g. during game pause). Reversed by unduckBackgroundMusic().
     */
    duckBackgroundMusic: function () { this._bg.duck(); },

    /** Restore background music to full configured volume after duck. */
    unduckBackgroundMusic: function () { this._bg.unduck(); },

    /** @returns {boolean} True if background music is currently active. */
    isBgPlaying: function () { return this._bg.isPlaying(); },

    /** @returns {'lobby'|'game'|null} Current background music mode. */
    getBgMode: function () { return this._bgMode; },

    // ── onChange / refresh ────────────────────────────────────────────────────

    /**
     * Register a callback for state changes (enabled, config, volumes).
     * @param {function} fn
     * @returns {function} unsubscribe
     */
    onChange: function (fn) {
      if (typeof fn !== 'function') return function () {};
      this._listeners.push(fn);
      var self = this;
      return function () {
        self._listeners = self._listeners.filter(function (l) { return l !== fn; });
      };
    },

    /** Re-fetch config from the server (e.g. after admin saves). */
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
      // Effect URLs + pool reset
      var urls = (cfg && cfg.urls) || {};
      this._urls    = urls;
      this._pools   = {};
      this._poolIdx = {};

      // Volumes
      var v = cfg && cfg.volumes;
      this._volumes = {
        lobby:   (v && typeof v.lobby   === 'number') ? v.lobby   : 0.6,
        game:    (v && typeof v.game    === 'number') ? v.game    : 0.5,
        effects: (v && typeof v.effects === 'number') ? v.effects : 0.9,
      };

      // Background music track lists
      this._lobbyTracks    = (cfg && Array.isArray(cfg.lobbyMusic))    ? cfg.lobbyMusic    : [];
      this._themeMusic     = (cfg && cfg.themeMusic)                   ? cfg.themeMusic    : {};
      this._fallbackTracks = (cfg && Array.isArray(cfg.fallbackMusic)) ? cfg.fallbackMusic : [];

      // Live-update background music volume if playing
      if (this._bg.isPlaying()) {
        var newVol = (this._bgMode === 'lobby') ? this._volumes.lobby : this._volumes.game;
        this._bg.setVolume(newVol);
      }

      this._notify({ urls: urls, volumes: this._volumes });
    },

    _buildPool: function (url) {
      var pool = [];
      var vol  = this._volumes ? this._volumes.effects : 0.9;
      for (var i = 0; i < POOL_SIZE; i++) {
        var a = new Audio();
        a.src     = url;
        a.preload = 'auto';
        a.volume  = vol;
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
    module.exports = { AudioPlayer: AudioPlayer, BackgroundPlayer: BackgroundPlayer };
  }
}(typeof window !== 'undefined' ? window : this));
