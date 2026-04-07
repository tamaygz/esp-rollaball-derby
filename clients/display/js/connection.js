'use strict';

/**
 * DisplayConnection — WebSocket client for the display SPA.
 *
 * Registers as client type "display" (observer only, no player entry).
 * Provides exponential-backoff auto-reconnect (REQ-011).
 *
 * Public API:
 *   DisplayConnection.connect()        — open (or reopen) the WebSocket
 *   DisplayConnection.send(msg)        — send JSON message; returns true on success
 *   DisplayConnection.onMessage(fn)    — register a message handler
 *   DisplayConnection.disconnect()     — close and cancel pending reconnect
 */
var DisplayConnection = (function () {
  var INITIAL_DELAY = 1000;
  var MAX_DELAY     = 30000;

  var ws             = null;
  var reconnectTimer = null;
  var reconnectDelay = INITIAL_DELAY;
  var _handlers      = [];
  var _shouldReconnect = false;

  // ── Helpers ─────────────────────────────────────────────────────────────────

  function _wsUrl() {
    var proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return proto + '//' + location.host;
  }

  function _setStatus(state) {
    var dot = document.getElementById('status-dot');
    if (!dot) return;
    dot.className = state; // 'connected' | 'connecting' | ''
  }

  function _dispatch(msg) {
    for (var i = 0; i < _handlers.length; i++) {
      try { _handlers[i](msg); } catch (e) { console.error('[DisplayConnection] handler error', e); }
    }
  }

  // ── Connection lifecycle ─────────────────────────────────────────────────────

  function connect() {
    if (ws && ws.readyState !== WebSocket.CLOSED) {
      _shouldReconnect = false;
      ws.close();
    }
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    _shouldReconnect = true;
    _setStatus('connecting');

    var socket;
    try {
      socket = new WebSocket(_wsUrl());
    } catch (e) {
      console.error('[DisplayConnection] init error', e);
      _setStatus('');
      _scheduleReconnect();
      return;
    }
    ws = socket;

    socket.addEventListener('open', function () {
      reconnectDelay = INITIAL_DELAY;
      _setStatus('connected');
      // Register as display observer — no playerName needed (TASK-005)
      socket.send(JSON.stringify({ type: 'register', payload: { type: 'display' } }));
    });

    socket.addEventListener('message', function (event) {
      var msg;
      try { msg = JSON.parse(event.data); } catch (e) { return; }
      _dispatch(msg);
    });

    socket.addEventListener('close', function () {
      _setStatus('');
      // Only reconnect if this socket is still the current one
      if (socket === ws && _shouldReconnect) _scheduleReconnect();
    });

    socket.addEventListener('error', function () {
      _setStatus('');
      // 'close' fires after 'error', so reconnect is triggered there
    });
  }

  function _scheduleReconnect() {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      connect();
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
    _shouldReconnect = false;
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    if (ws) { ws.close(); ws = null; }
    reconnectDelay = INITIAL_DELAY;
    _setStatus('');
  }

  return { connect: connect, send: send, onMessage: onMessage, disconnect: disconnect };
}());
