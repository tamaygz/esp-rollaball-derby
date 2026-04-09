'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.State — live game-state tracking and DOM rendering.
 *
 * Public API:
 *   Derby.State.render(statePayload)      — update all UI elements from a state message
 *   Derby.State.setMyId(id)               — store this client's player ID
 *   Derby.State.getMyId()                 — retrieve this client's player ID
 *   Derby.State.getStatus()               — retrieve current game status string
 *   Derby.State.addLogEntry(entry)        — push { type, text } to the event log
 *   Derby.State.showWinner(name)          — show winner banner independently
 */
Derby.State = (function () {
  var MAX_NAME_LEN = 20;

  // ── Internal state ──────────────────────────────────────────────────────────
  var _current = {
    status: 'idle',
    config: { trackLength: 15, maxPlayers: 16, theme: 'horse' },
    players: [],
    startedAt: null,
    connectedClients: { total: 0, sensor: 0, web: 0, motor: 0, display: 0 },
  };
  var _myId = null;
  var MAX_LOG = 50;

  // Player colour palette (mirrors shared/player-colors.json)
  var PLAYER_COLORS = [
    '#E53E3E','#3182CE','#38A169','#D69E2E','#805AD5',
    '#D53F8C','#00B5D8','#68D391','#FF6B35','#B7791F',
    '#9B2C2C','#2C7A7B','#744210','#553C9A','#C05621','#276749',
  ];

  function _color(index) {
    return PLAYER_COLORS[index % PLAYER_COLORS.length];
  }

  function _esc(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function _el(id) { return document.getElementById(id); }

  // ── Render helpers ──────────────────────────────────────────────────────────

  function _renderStatusBadge(status) {
    var badge = _el('game-status-badge');
    if (!badge) return;
    var labels = { idle: 'Idle', running: 'Running', paused: 'Paused', finished: 'Finished' };
    badge.textContent = labels[status] || status;
    badge.className = 'badge badge-' + (status || 'idle');
  }

  function _renderClientsSummary(counts) {
    var el = _el('clients-summary');
    if (!el) return;
    var parts = [];
    if (counts.sensor)  parts.push(counts.sensor  + ' sensor' + (counts.sensor  !== 1 ? 's' : ''));
    if (counts.web)     parts.push(counts.web     + ' web');
    if (counts.motor)   parts.push(counts.motor   + ' motor');
    if (counts.display) parts.push(counts.display + ' display');
    el.textContent = counts.total + ' total' + (parts.length ? ' (' + parts.join(', ') + ')' : '');
  }

  function _renderButtons(status) {
    var btnStart = _el('btn-start');
    var btnPause = _el('btn-pause');
    var btnReset = _el('btn-reset');

    if (btnStart) btnStart.disabled = (status !== 'idle');
    if (btnPause) {
      btnPause.disabled = (status !== 'running' && status !== 'paused');
      btnPause.textContent = status === 'paused' ? '▶ Resume' : '⏸ Pause';
    }
    if (btnReset) btnReset.disabled = (status === 'idle');
  }

  function _renderConfig(config, status) {
    var cfgTrack     = _el('cfg-track');
    var cfgMax       = _el('cfg-max');
    var cfgTheme     = _el('cfg-theme');
    var cfgCountdown = _el('cfg-countdown');
    var btnSave      = _el('btn-save-config');
    var isIdle       = (status === 'idle');

    if (cfgTrack)     { cfgTrack.value     = config.trackLength;          cfgTrack.disabled     = !isIdle; }
    if (cfgMax)       { cfgMax.value       = config.maxPlayers;           cfgMax.disabled       = !isIdle; }
    if (cfgTheme)     { cfgTheme.value     = config.theme;                cfgTheme.disabled     = !isIdle; }
    if (cfgCountdown) { cfgCountdown.value = config.countdown != null ? String(config.countdown) : '0';
                        cfgCountdown.disabled = !isIdle; }
    if (btnSave)      { btnSave.disabled = !isIdle; }
  }

  function _renderWinnerBanner(status) {
    var banner = _el('winner-banner');
    if (!banner) return;
    if (status === 'finished') {
      banner.classList.remove('hidden');
    } else {
      banner.classList.add('hidden');
      var nameEl = _el('winner-name');
      if (nameEl) nameEl.textContent = '—';
    }
  }

  function _renderPlayersList(players, trackLength) {
    var container = _el('players-list');
    if (!container) return;

    if (!players || players.length === 0) {
      container.innerHTML = '<p class="empty-msg">No players connected yet.</p>';
      return;
    }

    var html = players.map(function (p, i) {
      var pct = trackLength > 0 ? Math.min((p.position / trackLength) * 100, 100) : 0;
      var ci = (typeof p.colorIndex === 'number') ? p.colorIndex : i;
      var color = _color(ci);
      var dot = p.connected ? '🟢' : '🔴';
      var isMine = (p.id === _myId);
      var displayName = _esc(p.name) + (isMine ? ' <em style="opacity:.6;font-size:.8em">(you)</em>' : '');

      return (
        '<div class="player-row" data-id="' + _esc(p.id) + '">' +
          '<div class="player-header">' +
            '<span class="player-color-dot" style="background:' + color + '"></span>' +
            '<span class="player-name" data-id="' + _esc(p.id) + '" data-name="' + _esc(p.name) + '">' + displayName + '</span>' +
            '<button class="btn-edit-name" data-id="' + _esc(p.id) + '" title="Rename player" aria-label="Rename player">✏️</button>' +
            '<button class="btn-remove-player" data-id="' + _esc(p.id) + '" data-name="' + _esc(p.name) + '" title="Remove player" aria-label="Remove player ' + _esc(p.name) + '">🗑️</button>' +
            '<span class="player-type-badge">' + _esc(p.type) + '</span>' +
            '<span class="player-status">' + dot + '</span>' +
          '</div>' +
          '<div class="progress-wrap">' +
            '<div class="progress-bar" style="width:' + pct.toFixed(1) + '%;background:' + color + '"></div>' +
          '</div>' +
          '<div class="progress-label">' + p.position + ' / ' + trackLength + '</div>' +
        '</div>'
      );
    }).join('');

    container.innerHTML = html;

    // Attach rename listeners
    var editBtns = container.querySelectorAll('.btn-edit-name');
    editBtns.forEach(function (btn) {
      btn.addEventListener('click', function () {
        _startNameEdit(btn.dataset.id);
      });
    });

    // Attach remove listeners
    var removeBtns = container.querySelectorAll('.btn-remove-player');
    removeBtns.forEach(function (btn) {
      btn.addEventListener('click', function () {
        var name = btn.dataset.name || 'this player';
        if (!window.confirm('Remove "' + name + '" from the game?')) return;
        Derby.Admin.removePlayer(btn.dataset.id);
      });
    });
  }

  function _startNameEdit(playerId) {
    var container = _el('players-list');
    if (!container) return;
    var nameSpan = container.querySelector('.player-name[data-id="' + playerId + '"]');
    if (!nameSpan) return;

    // Read the stored plain-text name from the data attribute (avoids fragile innerText parsing)
    var current = nameSpan.dataset.name || '';

    // Build the input element programmatically to avoid innerHTML injection
    var input = document.createElement('input');
    input.type = 'text';
    input.className = 'name-edit-input';
    input.maxLength = MAX_NAME_LEN;
    input.value = current;

    nameSpan.innerHTML = '';
    nameSpan.appendChild(input);
    input.focus();
    input.select();

    var committed = false;
    function commit() {
      if (committed) return;
      committed = true;
      // input.value comes from a text input, so normalize it here with trim/length limiting;
      // the server may still apply its own sanitization before storing the final name.
      var newName = input.value.trim().slice(0, MAX_NAME_LEN);
      if (newName && newName !== current) {
        Derby.Admin.renamePlayer(playerId, newName);
      } else {
        // Restore by re-rendering current state
        _renderPlayersList(_current.players, _current.config.trackLength);
      }
    }

    input.addEventListener('blur', commit);
    input.addEventListener('keydown', function (e) {
      if (e.key === 'Enter')  { input.blur(); }
      if (e.key === 'Escape') { committed = true; _renderPlayersList(_current.players, _current.config.trackLength); }
    });
  }

  function _renderScoreSelector(players, status) {
    var select = _el('score-player');
    if (!select) return;

    var prevValue = select.value;
    var connected = players.filter(function (p) { return p.connected; });

    select.innerHTML = '<option value="">— select player —</option>';
    connected.forEach(function (p) {
      var opt = document.createElement('option');
      opt.value = p.id;
      opt.textContent = p.name + ' (' + p.type + ')';
      if (p.id === prevValue) opt.selected = true;
      select.appendChild(opt);
    });

    // Delegate button state sync to Derby.Test once it is initialised
    if (window.Derby && Derby.Test) {
      Derby.Test.syncButtons();
    } else {
      // Minimal fallback used only during initial page load before test.js runs
      var canScore = (status === 'running') && !!select.value;
      var btn1 = _el('btn-score-1');
      var btn3 = _el('btn-score-3');
      if (btn1) btn1.disabled = !canScore;
      if (btn3) btn3.disabled = !canScore;
    }
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  function render(state) {
    var previous = _current;
    state = state || {};

    var status = state.status || previous.status || 'idle';
    var config = state.config || previous.config || { trackLength: 15, maxPlayers: 16, theme: 'horse' };
    var players = state.players || previous.players || [];
    var counts = state.connectedClients || previous.connectedClients || { total: 0, sensor: 0, web: 0, motor: 0, display: 0 };

    _current = {
      status: status,
      config: config,
      players: players,
      connectedClients: counts,
    };
    _renderStatusBadge(status);
    _renderClientsSummary(counts);
    _renderButtons(status);
    _renderConfig(config, status);
    _renderWinnerBanner(status);
    _renderPlayersList(players, config.trackLength);
    _renderScoreSelector(players, status);
  }

  function setMyId(id) { _myId = id; }
  function getMyId()   { return _myId; }
  function getStatus() { return _current.status; }

  function addLogEntry(entry) {
    var logEl = _el('event-log');
    if (!logEl) return;
    var li = document.createElement('li');
    li.className = 'log-entry log-' + (entry.type || 'info');
    li.textContent = entry.text;
    logEl.insertBefore(li, logEl.firstChild);
    while (logEl.children.length > MAX_LOG) {
      logEl.removeChild(logEl.lastChild);
    }
  }

  function showWinner(name) {
    var banner = _el('winner-banner');
    if (banner) banner.classList.remove('hidden');
    var nameEl = _el('winner-name');
    if (nameEl) nameEl.textContent = name;
  }

  return {
    render: render,
    setMyId: setMyId,
    getMyId: getMyId,
    getStatus: getStatus,
    addLogEntry: addLogEntry,
    showWinner: showWinner,
  };
}());
