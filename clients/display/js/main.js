'use strict';

/**
 * main.js — Display SPA entry point (REQ-001 / TASK-002).
 *
 * Initialises a fullscreen Pixi v8 Application, builds the scene, and routes
 * incoming WebSocket messages to the appropriate scene objects.
 *
 * Message routing:
 *   state   → RaceTrack.setState()   + status overlays
 *   scored  → RaceTrack.triggerScoringEffect()
 *   winner  → WinnerOverlay.show()
 */

/* global PIXI, DisplayConnection, ThemeManager, RaceTrack, WinnerOverlay, CountdownEffect, GameEvents, DerbyAudio, SoundDecision */

(async function () {

  // ── Audio: shared cross-browser player + mute toggle ──────────────────────
  // Kick off config fetch in parallel with Pixi init.
  if (typeof DerbyAudio !== 'undefined') {
    DerbyAudio.init().catch(function () { /* ignore */ });
    (function () {
      var btn = document.getElementById('audio-toggle');
      if (!btn) return;
      function syncBtn() {
        var muted = !DerbyAudio.isEnabled();
        btn.classList.toggle('muted', muted);
        btn.setAttribute('aria-pressed', muted ? 'true' : 'false');
        btn.title = muted ? 'Audio muted — click to enable' : 'Audio on — click to mute';
      }
      btn.addEventListener('click', function () { DerbyAudio.toggle(); syncBtn(); });
      DerbyAudio.onChange(syncBtn);
      syncBtn();
    }());
  }

  // ── Pixi Application (fullscreen, resizes with window) ─────────────────────
  var app = new PIXI.Application();
  await app.init({
    background: 0x0a0a0a,
    resizeTo: window,
    antialias: true,
    autoDensity: true,
    resolution: window.devicePixelRatio || 1,
  });
  document.body.appendChild(app.canvas);

  // ── Load available concrete themes from shared manifest ──────────────────────
  // Fetched at startup so display and server share a single source of truth.
  // Falls back to a hardcoded list if the manifest cannot be fetched.
  var CONCRETE_THEMES = await (async function () {
    try {
      var r = await fetch('/assets/themes/shared/themes.json');
      if (r.ok) {
        var m = await r.json();
        if (Array.isArray(m.concreteThemes) && m.concreteThemes.length) {
          return m.concreteThemes;
        }
      }
    } catch (e) { /* fall through */ }
    return ['horse', 'camel'];
  }());

  // ── Scene objects ────────────────────────────────────────────────────────────
  var raceTrack       = null;
  var winnerOverlay   = null;
  var countdownEffect = null;

  var _resolvedTheme  = null;   // concrete theme picked once 'auto' is encountered

  /**
   * Resolve a possibly-'auto' or unrecognised theme to a known concrete one,
   * persisting the choice across state updates.
   */
  function _resolveTheme(theme) {
    if (theme && theme !== 'auto' && CONCRETE_THEMES.indexOf(theme) !== -1) {
      _resolvedTheme = theme;   // server gave us a concrete theme (game started)
      return theme;
    }
    // theme is null/undefined, 'auto', or an unrecognised value — use/pick a concrete one
    if (!_resolvedTheme) {
      _resolvedTheme = CONCRETE_THEMES[Math.floor(Math.random() * CONCRETE_THEMES.length)];
    }
    return _resolvedTheme;
  }

  // Initialise scene once we receive the first state message
  async function _initScene(theme) {
    var resolvedTheme = _resolveTheme(theme);
    try {
      await ThemeManager.load(resolvedTheme);
    } catch (e) {
      // If the resolved theme fails to load, fall back to the first concrete theme
      // that is different from the one that failed (handles corrupted/missing assets).
      var fallback = CONCRETE_THEMES.find(function (t) { return t !== resolvedTheme; }) || CONCRETE_THEMES[0];
      _resolvedTheme = fallback;
      await ThemeManager.load(fallback);
    }

    raceTrack = new RaceTrack(app);
    app.stage.addChild(raceTrack);

    winnerOverlay = new WinnerOverlay(app.screen.width, app.screen.height);
    app.stage.addChild(winnerOverlay);

    countdownEffect = new CountdownEffect(app.screen.width, app.screen.height);
    app.stage.addChild(countdownEffect);
  }

  // ── Resize handler ────────────────────────────────────────────────────────────
  app.renderer.on('resize', function () {
    if (raceTrack)       raceTrack.resize(app.screen.width, app.screen.height);
    if (winnerOverlay)   winnerOverlay.resize(app.screen.width, app.screen.height);
    if (countdownEffect) countdownEffect.resize(app.screen.width, app.screen.height);
  });

  // ── Message handlers ──────────────────────────────────────────────────────────

  async function _handleState(state) {
    // Resolve 'auto' to a concrete theme before any component sees state.config.theme
    if (state.config) {
      state.config.theme = _resolveTheme(state.config.theme);
    }
    if (!raceTrack) {
      await _initScene(state.config && state.config.theme);
    }
    if (winnerOverlay && winnerOverlay.visible && state.status !== 'finished') {
      winnerOverlay.hide();
    }
    if (countdownEffect && countdownEffect.visible) {
      countdownEffect.hide();
    }
    await raceTrack.setState(state);
  }

  function _handleScored(payload) {
    if (!raceTrack) return;
    // Use events from server if present; fall back to deriving from points.
    var events = (payload.events && payload.events.length)
      ? payload.events
      : (payload.points === 0 ? [GameEvents.ZERO_ROLL]
        : payload.points === 3 ? [GameEvents.SCORE_3]
        : payload.points === 2 ? [GameEvents.SCORE_2]
        : [GameEvents.SCORE_1]);
    raceTrack.triggerEffect(payload.playerId, events);
  }

  function _handleWinner(payload) {
    if (winnerOverlay) winnerOverlay.show(payload.name || 'WINNER');
    if (raceTrack)     raceTrack.triggerScoringEffect(payload.playerId);
  }

  function _handleCountdown(payload) {
    if (countdownEffect) countdownEffect.show(payload.count);
  }

  // ── WebSocket routing ─────────────────────────────────────────────────────────

  var _lastSeenSeq = 0;

  DisplayConnection.onMessage(function (msg) {
    // Deduplicate messages by sequence number (T11).
    // Treat reconnects / server seq resets as a new epoch so fresh state
    // messages are not dropped after the server counter starts over.
    if (msg.type === 'registered') {
      _lastSeenSeq = 0;
    }

    if (typeof msg.seq === 'number' && msg.seq > 0) {
      if (_lastSeenSeq > 0 && msg.seq < _lastSeenSeq) {
        _lastSeenSeq = 0;
      }
      if (msg.seq <= _lastSeenSeq) return;
      _lastSeenSeq = msg.seq;
    }

    switch (msg.type) {
      case 'state':      _handleState(msg.payload);    break;
      case 'scored':     _handleScored(msg.payload);   break;
      case 'winner':     _handleWinner(msg.payload);   break;
      case 'countdown':  _handleCountdown(msg.payload); break;
      case 'registered':
        console.log('[Display] registered', msg.payload.id);
        break;
      default:
        break;
    }
  });

  // ── Connect ───────────────────────────────────────────────────────────────────
  DisplayConnection.connect();

  // ── Optional auto-fullscreen via ?fullscreen=1 (TASK-026) ────────────────────
  var params = new URLSearchParams(location.search);
  if (params.get('fullscreen') === '1') {
    document.documentElement.requestFullscreen().catch(function () {
      // Fullscreen may fail if not triggered by user gesture; silently ignore.
    });
  }

}());
