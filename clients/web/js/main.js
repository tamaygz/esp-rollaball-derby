'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.main — entry point.
 * Reads localStorage for player name, wires up modules, and initiates the
 * WebSocket connection.
 */
(function () {
  var LS_KEY = 'derby-player-name';

  function _el(id) { return document.getElementById(id); }

  function _timestamp() {
    return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

  // ── Message router ────────────────────────────────────────────────────────

  Derby.Connection.onMessage(function (msg) {
    switch (msg.type) {

      case 'registered': {
        var id   = msg.payload.id;
        var name = msg.payload.name;
        var ptype = msg.payload.playerType;
        Derby.State.setMyId(id);
        // Persist so the next reconnect can supply this ID and reuse the entry.
        localStorage.setItem('derby-player-id', id);
        var infoEl = _el('my-id-info');
        if (infoEl) {
          infoEl.textContent = 'You: ' + name + ' (' + ptype + ') — ID: ' + id.slice(0, 8) + '…';
          infoEl.classList.remove('hidden');
        }
        Derby.State.addLogEntry({ type: 'info', text: _timestamp() + ' ✓ Registered as "' + name + '" (' + ptype + ')' });
        break;
      }

      case 'state':
        Derby.State.render(msg.payload);
        // Keep score buttons in sync after a state update
        if (Derby.Test) Derby.Test.syncButtons();
        // Keep bots in sync (start/stop timers, refresh player list)
        if (Derby.Bots) Derby.Bots.syncState(msg.payload);
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
  Derby.Test.init();
  Derby.Bots.init();

  // ── Player name (localStorage persistence) ────────────────────────────────

  var nameInput = _el('my-name');
  var savedName = localStorage.getItem(LS_KEY) || '';

  if (nameInput) {
    nameInput.value = savedName;
    nameInput.addEventListener('change', function () {
      localStorage.setItem(LS_KEY, nameInput.value.trim());
    });
  }

  // ── Reconnect button ──────────────────────────────────────────────────────

  var btnReconnect = _el('btn-reconnect');
  if (btnReconnect) {
    btnReconnect.addEventListener('click', function () {
      var name = nameInput ? nameInput.value.trim() : '';
      localStorage.setItem(LS_KEY, name);
      Derby.Connection.disconnect();
      Derby.Connection.connect(name);
    });
  }

  // ── Connect ───────────────────────────────────────────────────────────────

  Derby.Connection.connect(savedName);
}());
