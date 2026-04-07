'use strict';

/**
 * RaceTrack — manages all player lanes and shared overlays (REQ-003 / TASK-007).
 *
 * A PIXI.Container that:
 *   • Creates / removes Lane instances as players join or leave
 *   • Divides canvas height equally among active players (TASK-009)
 *   • Reflows on resize (TASK-013)
 *   • Exposes status overlays for idle / paused states (TASK-022 / TASK-023)
 *
 * Usage:
 *   var track = new RaceTrack(app);
 *   app.stage.addChild(track);
 *   await track.setState(statePayload);   // called on every 'state' WS message
 *   track.triggerScoringEffect(playerId);
 *   track.resize(newW, newH);
 */

/* global PIXI, ThemeManager, Lane */

class RaceTrack extends PIXI.Container {
  constructor(app) {
    super();
    this._app     = app;
    this._lanes   = new Map();  // playerId → Lane
    this._players = [];         // ordered snapshot
    this._config  = { trackLength: 15, theme: 'horse' };
    this._loadedTheme = null;

    this._statusOverlay = null;
    this._buildStatusOverlay();
  }

  // ── Status overlay ───────────────────────────────────────────────────────────

  _buildStatusOverlay() {
    if (this._statusOverlay) {
      this.removeChild(this._statusOverlay);
    }
    var w = this._app.screen.width;
    var h = this._app.screen.height;

    this._statusOverlay = new PIXI.Container();

    var bg = new PIXI.Graphics();
    bg.rect(0, 0, w, h).fill({ color: 0x000000, alpha: 0.72 });
    this._statusOverlay.addChild(bg);

    var fontSize = Math.max(36, Math.min(w * 0.07, 100));
    this._statusText = new PIXI.Text({
      text: '',
      style: new PIXI.TextStyle({
        fontFamily: '"Arial Black", Arial, sans-serif',
        fontWeight: '900',
        fontSize: fontSize,
        fill: '#ffffff',
        stroke: { color: '#000000', width: Math.max(4, fontSize * 0.07) },
        align: 'center',
      }),
    });
    this._statusText.anchor.set(0.5);
    this._statusText.x = w / 2;
    this._statusText.y = h / 2;
    this._statusOverlay.addChild(this._statusText);

    this._statusOverlay.visible = false;
    this.addChild(this._statusOverlay);
  }

  _showStatus(text) {
    if (!this._statusOverlay) return;
    this._statusText.text = text;
    this._statusOverlay.visible = true;
  }

  _hideStatus() {
    if (this._statusOverlay) this._statusOverlay.visible = false;
  }

  // ── Main state update ────────────────────────────────────────────────────────

  /**
   * Apply a full game state snapshot from the server.
   * @param {object} state  Payload from the 'state' WebSocket message.
   * @returns {Promise<void>}
   */
  async setState(state) {
    this._config = state.config || this._config;

    // Load theme if changed
    if (this._loadedTheme !== this._config.theme) {
      await ThemeManager.load(this._config.theme);
      this._loadedTheme = this._config.theme;
      // Rebuild all lanes with new theme
      this._clearLanes();
    }

    var players = (state.players || []).filter(function (p) { return p.connected || state.status !== 'idle'; });

    // Detect player set change (add / remove)
    var newIds = players.map(function (p) { return p.id; }).sort().join(',');
    var curIds = [...this._lanes.keys()].sort().join(',');
    if (newIds !== curIds) {
      this._rebuildLanes(players);
    } else {
      // Update positions and connected state
      for (var i = 0; i < players.length; i++) {
        var p    = players[i];
        var lane = this._lanes.get(p.id);
        if (lane) {
          lane.updatePosition(p.position, this._config.trackLength);
          lane.setConnected(p.connected);
        }
      }
    }

    // Status overlay
    var status = state.status;
    if (status === 'idle') {
      var hasPlayers = players.length > 0;
      this._showStatus(hasPlayers ? 'READY — START GAME' : 'WAITING FOR PLAYERS');
    } else if (status === 'paused') {
      this._showStatus('⏸  PAUSED');
    } else if (status === 'finished') {
      this._hideStatus();
    } else {
      this._hideStatus();
    }
  }

  // ── Lane management ──────────────────────────────────────────────────────────

  _clearLanes() {
    for (var lane of this._lanes.values()) {
      this.removeChild(lane);
    }
    this._lanes.clear();
    this._players = [];
  }

  _rebuildLanes(players) {
    this._clearLanes();
    this._players = players;

    var w = this._app.screen.width;
    var h = this._app.screen.height;
    var n = players.length || 1;

    for (var i = 0; i < players.length; i++) {
      var p    = players[i];
      var lane = new Lane(p, i, i, w, Math.floor(h / n));
      lane.y   = i * Math.floor(h / n);
      lane.updatePosition(p.position, this._config.trackLength);
      lane.setConnected(p.connected);
      this._lanes.set(p.id, lane);
      // Insert before status overlay so overlay stays on top
      this.addChildAt(lane, this.children.indexOf(this._statusOverlay));
    }
  }

  _reflowLanes() {
    var w  = this._app.screen.width;
    var h  = this._app.screen.height;
    var n  = this._players.length || 1;
    var lh = Math.floor(h / n);
    var i  = 0;
    for (var lane of this._lanes.values()) {
      lane.resize(w, lh);
      lane.y = i * lh;
      i++;
    }
  }

  // ── Scoring effect ───────────────────────────────────────────────────────────

  triggerScoringEffect(playerId) {
    var lane = this._lanes.get(playerId);
    if (lane) lane.triggerScoringEffect();
  }

  /**
   * Trigger action effects for the given events on a player's lane.
   * @param {string}   playerId
   * @param {string[]} events    Event strings from the server scored message.
   */
  triggerEffect(playerId, events) {
    var lane = this._lanes.get(playerId);
    if (lane) lane.triggerEffect(events);
  }

  // ── Resize ───────────────────────────────────────────────────────────────────

  resize(w, h) {
    this._reflowLanes();
    this._buildStatusOverlay();
  }
}
