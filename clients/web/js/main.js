'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.main — entry point for the admin page.
 * Connects to the server as an observer (display type) and wires up modules.
 */
(function () {
  function _timestamp() {
    return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

  // ── Message router ────────────────────────────────────────────────────────

  Derby.Connection.onMessage(function (msg) {
    switch (msg.type) {

      case 'registered':
        Derby.State.addLogEntry({ type: 'info', text: _timestamp() + ' ✓ Connected as observer' });
        break;

      case 'state':
        Derby.State.render(msg.payload);
        // Keep bots in sync (start/stop timers, refresh player list)
        if (Derby.Bots) Derby.Bots.syncState(msg.payload);
        // Update LED device list with connected devices
        if (Derby.LED) Derby.LED.updateDeviceList(msg.payload.devices || []);
        break;

      case 'scored': {
        var pn  = msg.payload.playerName;
        var pts = msg.payload.points;
        var pos = msg.payload.newPosition;
        Derby.State.addLogEntry({
          type: 'score',
          text: _timestamp() + ' — ' + pn + ': +' + pts + ' → position ' + pos,
        });
        break;
      }

      case 'winner': {
        var wname = msg.payload.name;
        Derby.State.showWinner(wname);
        Derby.State.addLogEntry({ type: 'winner', text: _timestamp() + ' 🏆 ' + wname + ' wins!' });
        break;
      }

      case 'error':
        Derby.State.addLogEntry({ type: 'error', text: _timestamp() + ' ⚠ ' + msg.payload.message });
        break;

      default:
        break;
    }
  });

  // ── Init modules ──────────────────────────────────────────────────────────

  Derby.Admin.init();
  Derby.Bots.init();
  try { if (Derby.LED) Derby.LED.init(); } catch (e) { console.error('[Derby] LED init failed:', e); }

  // ── Connect as observer (display type — no player entry created) ──────────

  Derby.Connection.connect('', 'display');
}());
