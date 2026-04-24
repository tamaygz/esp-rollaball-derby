'use strict';

const util = require('node:util');

// ─── Server-side log interceptor ──────────────────────────────────────────────
// Wraps console.log/warn/error so every server-side log line is also forwarded
// to connected web/display clients as a `log_line` WebSocket message.
//
// Guarantees:
//   • Native console output is NEVER suppressed — original methods are always
//     called first before the forwarding logic runs.
//   • Lines emitted before setConnectionManager() is called are buffered in a
//     bounded ring buffer (SERVER_LOG_BUFFER_MAX entries) and flushed once the
//     ConnectionManager is ready.
//
// Usage:
//   const serverLog = require('./log');
//   // … after connectionManager is created:
//   serverLog.setConnectionManager(connectionManager);

const SERVER_LOG_BUFFER_MAX = 200;

let _cm      = null;
const _queue = [];   // ring buffer of { source, senderName, senderType, level, message, ts }

const _origLog   = console.log.bind(console);
const _origWarn  = console.warn.bind(console);
const _origError = console.error.bind(console);

function _fmt(...args) {
  try {
    return util.format(...args);
  } catch (error) {
    try {
      return args
        .map((a) => {
          if (a instanceof Error) return a.stack || a.message;
          if (typeof a === 'string') return a;
          return util.inspect(a, { depth: 2, breakLength: Infinity });
        })
        .join(' ');
    } catch (innerError) {
      return '[log format error]';
    }
  }
}

function _emit(level, ...args) {
  const message = _fmt(...args);
  const entry = {
    source:     'server',
    senderName: 'Server',
    senderType: 'server',
    level,
    message,
    ts: Date.now(),
  };
  if (_cm) {
    _cm.broadcastLog(entry);
  } else if (_queue.length < SERVER_LOG_BUFFER_MAX) {
    _queue.push(entry);
  } else {
    // Ring: overwrite oldest
    _queue.shift();
    _queue.push(entry);
  }
}

// Install interceptors — always call native first.
console.log   = (...args) => { _origLog(...args);   _emit('info',  ...args); };
console.warn  = (...args) => { _origWarn(...args);  _emit('warn',  ...args); };
console.error = (...args) => { _origError(...args); _emit('error', ...args); };

/**
 * Wire up the ConnectionManager.  All buffered entries are flushed immediately,
 * and subsequent server logs are forwarded in real time.
 * @param {object} cm - ConnectionManager instance
 */
function setConnectionManager(cm) {
  _cm = cm;
  for (const entry of _queue) {
    cm.broadcastLog(entry);
  }
  _queue.length = 0;
}

module.exports = { setConnectionManager };
