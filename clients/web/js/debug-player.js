'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.DebugPlayer — debug player page controller.
 *
 * Manages a single "web" player connection, exposes score controls (+0/+1/+2/+3),
 * a rename form, and an event log. Intended for use on debug-player.html only.
 */
(function () {
  var LS_KEY_NAME = 'derby-player-name';
  var MAX_NAME_LEN = 20;
  var MAX_LOG = 50;

  var _myId = null;
  var _myName = null;
  var _gameStatus = 'idle';
  var _trackLength = 15;
  var _connected = false;

  // ── Helpers ────────────────────────────────────────────────────────────────

  function _el(id) { return document.getElementById(id); }

  function _timestamp() {
    return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

  function _addLog(type, text) {
    var logEl = _el('event-log');
    if (!logEl) return;
    var li = document.createElement('li');
    li.className = 'log-entry log-' + (type || 'info');
    li.textContent = text;
    logEl.insertBefore(li, logEl.firstChild);
    while (logEl.children.length > MAX_LOG) {
      logEl.removeChild(logEl.lastChild);
    }
  }

  function _showError(elId, msg) {
    var el = _el(elId);
    if (!el) return;
    el.textContent = msg;
    el.classList.remove('hidden');
    setTimeout(function () { el.classList.add('hidden'); }, 5000);
  }

  function _flash(btn) {
    btn.classList.add('btn-flash');
    setTimeout(function () { btn.classList.remove('btn-flash'); }, 200);
  }

  // ── UI state ───────────────────────────────────────────────────────────────

  function _syncScoreButtons() {
    var canScore = _connected && !!_myId && (_gameStatus === 'running');
    var ids = ['btn-score-0', 'btn-score-1', 'btn-score-2', 'btn-score-3'];
    ids.forEach(function (id) {
      var btn = _el(id);
      if (btn) btn.disabled = !canScore;
    });
  }

  function _syncRenameButton() {
    var btn = _el('btn-rename');
    if (btn) btn.disabled = !(_connected && !!_myId);
  }

  function _syncConnectButtons() {
    var btnConnect    = _el('btn-connect');
    var btnDisconnect = _el('btn-disconnect');
    var nameInput     = _el('my-name');
    if (btnConnect)    btnConnect.disabled    = _connected;
    if (btnDisconnect) btnDisconnect.disabled = !_connected;
    if (nameInput)     nameInput.disabled     = _connected;
  }

  function _renderGameBadge(status) {
    var badge = _el('dp-game-badge');
    if (!badge) return;
    var labels = { idle: 'Idle', running: 'Running', paused: 'Paused', finished: 'Finished' };
    badge.textContent = labels[status] || status;
    badge.className = 'badge badge-' + (status || 'idle');
  }

  function _renderPlayerInfo() {
    var notConn = _el('dp-not-connected');
    var info    = _el('dp-player-info');
    if (!_connected || !_myId) {
      if (notConn) notConn.classList.remove('hidden');
      if (info)    info.classList.add('hidden');
      return;
    }

    if (notConn) notConn.classList.add('hidden');
    if (info)    info.classList.remove('hidden');

    var nameEl = _el('dp-player-name');
    if (nameEl) nameEl.textContent = _myName || '—';
  }

  function _renderPlayerPosition(position) {
    var posEl = _el('dp-player-pos');
    var bar   = _el('dp-progress-bar');
    if (posEl) posEl.textContent = position + ' / ' + _trackLength;
    if (bar) {
      var pct = _trackLength > 0 ? Math.min((position / _trackLength) * 100, 100) : 0;
      bar.style.width = pct.toFixed(1) + '%';
    }
  }

  // ── Message handlers ───────────────────────────────────────────────────────

  function _onRegistered(payload) {
    _myId   = payload.id;
    _myName = payload.name;
    // Persist ID for reconnect
    localStorage.setItem('derby-player-id', _myId);

    var infoEl = _el('my-id-info');
    if (infoEl) {
      infoEl.textContent = 'You: ' + _myName + ' — ID: ' + _myId.slice(0, 8) + '…';
      infoEl.classList.remove('hidden');
    }

    _addLog('info', _timestamp() + ' ✓ Registered as "' + _myName + '"');
    _renderPlayerInfo();
    _syncScoreButtons();
    _syncRenameButton();
  }

  function _onState(payload) {
    _gameStatus  = (payload && payload.status) || 'idle';
    _trackLength = (payload && payload.config && payload.config.trackLength) || 15;

    _renderGameBadge(_gameStatus);

    // Update own player's position from the state snapshot
    if (_myId && payload && payload.players) {
      var me = null;
      for (var i = 0; i < payload.players.length; i++) {
        if (payload.players[i].id === _myId) { me = payload.players[i]; break; }
      }
      if (me) {
        _myName = me.name;
        _renderPlayerInfo();
        _renderPlayerPosition(me.position);
      }
    }

    _syncScoreButtons();
  }

  function _onScored(payload) {
    var pn  = payload.playerName;
    var pts = payload.points;
    var pos = payload.newPosition;
    _addLog('score', _timestamp() + ' — ' + pn + ': +' + pts + ' → position ' + pos);

    // Update own position if it's our score
    if (payload.playerId === _myId) {
      _renderPlayerPosition(pos);
    }
  }

  function _onWinner(payload) {
    _addLog('winner', _timestamp() + ' 🏆 ' + payload.name + ' wins!');
  }

  function _onError(payload) {
    _addLog('error', _timestamp() + ' ⚠ ' + payload.message);
  }

  // ── Score sending ──────────────────────────────────────────────────────────

  function _clearScoreError() {
    var errorEl = _el('score-error');
    if (errorEl) {
      errorEl.textContent = '';
    }
  }

  function _sendScore(points) {
    if (!_myId) {
      _showError('score-error', 'You are not connected as a player yet.');
      return;
    }

    var sent = Derby.Connection.send({ type: 'score', payload: { playerId: _myId, points: points } });
    if (!sent) {
      _showError('score-error', 'Score could not be sent while disconnected. Please wait for reconnection.');
      _addLog('error', _timestamp() + ' ⚠ Failed to send score: disconnected');
      return;
    }

    _clearScoreError();
  }

  // ── REST: rename player ────────────────────────────────────────────────────

  function _onRename(e) {
    e.preventDefault();
    if (!_myId) return;

    var input = _el('rename-input');
    if (!input) return;
    var newName = input.value.trim().slice(0, MAX_NAME_LEN);
    if (!newName) return;

    fetch('/api/players/' + encodeURIComponent(_myId), {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: newName }),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) {
          _showError('rename-error', data.error);
        } else {
          _myName = newName;
          _addLog('info', _timestamp() + ' ✏️ Renamed to "' + newName + '"');
          input.value = '';
          _renderPlayerInfo();
        }
      })
      .catch(function (e) {
        _showError('rename-error', 'Network error: ' + e.message);
      });
  }

  // ── Init ───────────────────────────────────────────────────────────────────

  // Message router
  Derby.Connection.onMessage(function (msg) {
    switch (msg.type) {
      case 'registered':
        _onRegistered(msg.payload);
        break;
      case 'state':
        _onState(msg.payload);
        break;
      case 'scored':
        _onScored(msg.payload);
        break;
      case 'winner':
        _onWinner(msg.payload);
        break;
      case 'error':
        _onError(msg.payload);
        break;
      default:
        break;
    }
  });

  // Connect button
  var btnConnect = _el('btn-connect');
  if (btnConnect) {
    btnConnect.addEventListener('click', function () {
      var nameInput = _el('my-name');
      var name = nameInput ? nameInput.value.trim() : '';
      localStorage.setItem(LS_KEY_NAME, name);
      _connected = true;
      _syncConnectButtons();
      Derby.Connection.connect(name);
    });
  }

  // Disconnect button
  var btnDisconnect = _el('btn-disconnect');
  if (btnDisconnect) {
    btnDisconnect.addEventListener('click', function () {
      _connected = false;
      _myId      = null;
      _myName    = null;
      var infoEl = _el('my-id-info');
      if (infoEl) infoEl.classList.add('hidden');
      Derby.Connection.disconnect();
      _addLog('info', _timestamp() + ' Disconnected.');
      _renderGameBadge('idle');
      _renderPlayerInfo();
      _syncConnectButtons();
      _syncScoreButtons();
      _syncRenameButton();
    });
  }

  // Score buttons
  [0, 1, 2, 3].forEach(function (pts) {
    var btn = _el('btn-score-' + pts);
    if (btn) {
      btn.addEventListener('click', function () {
        _flash(btn);
        _sendScore(pts);
      });
    }
  });

  // Rename form
  var formRename = _el('form-rename');
  if (formRename) formRename.addEventListener('submit', _onRename);

  // Load saved name
  var nameInput = _el('my-name');
  var savedName = localStorage.getItem(LS_KEY_NAME) || '';
  if (nameInput) nameInput.value = savedName;
}());
