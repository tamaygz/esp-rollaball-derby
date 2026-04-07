'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.Connection — WebSocket client with exponential-backoff auto-reconnect.
 *
 * Public API:
 *   Derby.Connection.connect(playerName)  — open (or reopen) the WebSocket
 *   Derby.Connection.send(msg)            — send a JSON message; returns true on success
 *   Derby.Connection.onMessage(fn)        — register a message handler
 *   Derby.Connection.disconnect()         — close and cancel any pending reconnect
 */
Derby.Connection = (function () {
  var ws = null;
  var reconnectTimer = null;
  var reconnectDelay = 1000;
  var MAX_DELAY = 30000;
  var _currentPlayerName = '';
  var _handlers = [];

  // ── Helpers ────────────────────────────────────────────────────────────────

  function _wsUrl() {
    var proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return proto + '//' + location.host;
  }

  function _setStatus(state) {
    var el = document.getElementById('ws-status');
    if (!el) return;
    if (state === 'connected') {
      el.textContent = '⬤ Connected';
      el.className = 'ws-status ws-connected';
    } else if (state === 'connecting') {
      el.textContent = '⬤ Connecting…';
      el.className = 'ws-status ws-connecting';
    } else {
      el.textContent = '⬤ Disconnected';
      el.className = 'ws-status ws-disconnected';
    }
  }

  function _dispatch(msg) {
    for (var i = 0; i < _handlers.length; i++) {
      try { _handlers[i](msg); } catch (e) { console.error('[Derby.Connection] handler error', e); }
    }
  }

  // ── Connection lifecycle ────────────────────────────────────────────────────

  function connect(playerName) {
    _currentPlayerName = (typeof playerName === 'string') ? playerName.trim() : '';

    // Close any existing socket before re-opening
    if (ws && ws.readyState !== WebSocket.CLOSED) {
      ws.onclose = null; // prevent stale reconnect loop
      ws.close();
    }
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    _setStatus('connecting');

    try {
      ws = new WebSocket(_wsUrl());
    } catch (e) {
      console.error('[Derby.Connection] WebSocket init error', e);
      _setStatus('disconnected');
      _scheduleReconnect();
      return;
    }

    ws.addEventListener('open', function () {
      reconnectDelay = 1000; // reset backoff on successful connect
      _setStatus('connected');

      // Register as web client
      var payload = { type: 'web' };
      if (_currentPlayerName) payload.playerName = _currentPlayerName;
      ws.send(JSON.stringify({ type: 'register', payload: payload }));
    });

    ws.addEventListener('message', function (event) {
      var msg;
      try { msg = JSON.parse(event.data); } catch (e) { return; }
      _dispatch(msg);
    });

    ws.addEventListener('close', function () {
      _setStatus('disconnected');
      _scheduleReconnect();
    });

    ws.addEventListener('error', function () {
      _setStatus('disconnected');
      // 'close' event fires after 'error', so reconnect is triggered there
    });
  }

  function _scheduleReconnect() {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      connect(_currentPlayerName);
    }, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, MAX_DELAY);
  }

  function send(msg) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(msg));
      return true;
    }
    return false;
  }

  function onMessage(handler) {
    _handlers.push(handler);
  }

  function disconnect() {
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    if (ws) {
      ws.onclose = null;
      ws.close();
      ws = null;
    }
    reconnectDelay = 1000;
    _setStatus('disconnected');
  }

  return { connect: connect, send: send, onMessage: onMessage, disconnect: disconnect };
}());
