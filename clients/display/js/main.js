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

/* global PIXI, DisplayConnection, ThemeManager, RaceTrack, WinnerOverlay, CountdownEffect */

(async function () {

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

  // ── Scene objects ────────────────────────────────────────────────────────────
  var raceTrack       = null;
  var winnerOverlay   = null;
  var countdownEffect = null;

  var CONCRETE_THEMES = ['horse', 'camel'];
  var _resolvedTheme  = null;   // concrete theme picked once 'auto' is encountered

  /** Resolve a possibly-'auto' theme to a concrete one, persisting the choice. */
  function _resolveTheme(theme) {
    if (theme && theme !== 'auto') {
      _resolvedTheme = theme;   // server gave us a concrete theme (game started)
      return theme;
    }
    if (!_resolvedTheme) {
      _resolvedTheme = CONCRETE_THEMES[Math.floor(Math.random() * CONCRETE_THEMES.length)];
    }
    return _resolvedTheme;
  }

  // Initialise scene once we receive the first state message
  async function _initScene(theme) {
    var resolvedTheme = _resolveTheme(theme);
    await ThemeManager.load(resolvedTheme);

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
      : (payload.points === 0 ? ['zero_roll']
        : payload.points === 3 ? ['score_3']
        : payload.points === 2 ? ['score_2']
        : ['score_1']);
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

  DisplayConnection.onMessage(function (msg) {
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
