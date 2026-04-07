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
  var INITIAL_DELAY = 1000;
  var ws = null;
  var reconnectTimer = null;
  var reconnectDelay = INITIAL_DELAY;
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

    // Close any stale socket. Because ws is reassigned below before the old
    // socket can fire its 'close' event, the per-socket guard (ws === thisWs)
    // will evaluate false for the stale socket and suppress spurious reconnects.
    if (ws && ws.readyState !== WebSocket.CLOSED) {
      ws.close();
    }
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    _setStatus('connecting');

    var thisWs;
    try {
      thisWs = new WebSocket(_wsUrl());
    } catch (e) {
      console.error('[Derby.Connection] WebSocket init error', e);
      _setStatus('disconnected');
      _scheduleReconnect();
      return;
    }

    // Update the shared reference *before* attaching listeners so that any
    // close event from a previously-open socket cannot match thisWs.
    ws = thisWs;

    ws.addEventListener('open', function () {
      reconnectDelay = INITIAL_DELAY; // reset backoff on successful connect
      _setStatus('connected');

      // Register as web client
      var payload = { type: 'web' };
      if (_currentPlayerName) payload.playerName = _currentPlayerName;
      thisWs.send(JSON.stringify({ type: 'register', payload: payload }));
    });

    ws.addEventListener('message', function (event) {
      var msg;
      try { msg = JSON.parse(event.data); } catch (e) { return; }
      _dispatch(msg);
    });

    ws.addEventListener('close', function () {
      _setStatus('disconnected');
      // Only schedule a reconnect if this is still the active socket.
      // Stale sockets (closed by connect() or disconnect()) will not match.
      if (ws === thisWs) _scheduleReconnect();
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
    // Null ws *before* closing so the 'close' handler's (ws === thisWs) guard
    // evaluates false and does not trigger an unwanted reconnect.
    var closing = ws;
    ws = null;
    if (closing) closing.close();
    reconnectDelay = INITIAL_DELAY;
    _setStatus('disconnected');
  }

  return { connect: connect, send: send, onMessage: onMessage, disconnect: disconnect };
}());
