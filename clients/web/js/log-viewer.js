'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.LogViewer — live log display for the /admin/logs page.
 *
 * Listens for `log_line` WebSocket messages and appends colour-coded lines
 * to #log-output.  Each unique sender gets a stable colour from a fixed
 * palette.  Supports filtering by sender, configurable max-lines, autoscroll,
 * and a clear button.
 *
 * Expected DOM (created by logs.ejs):
 *   #log-output      — scrollable container for log lines
 *   #log-filter      — <select> for sender filter
 *   #log-maxlines    — <input type=number> for max line cap
 *   #log-autoscroll  — <input type=checkbox>
 *   #log-clear       — <button> to clear output
 *   #log-status      — status badge
 */
Derby.LogViewer = (function () {

  // ── Colour palette ─────────────────────────────────────────────────────────
  // 14 perceptually distinct colours suitable for dark backgrounds.
  var PALETTE = [
    '#4f8ef7',  // blue       — default for server
    '#63e6be',  // teal
    '#ffd43b',  // yellow
    '#f783ac',  // pink
    '#74c0fc',  // sky
    '#a9e34b',  // lime
    '#ff922b',  // orange
    '#cc5de8',  // purple
    '#20c997',  // cyan
    '#ff6b6b',  // red
    '#94d82d',  // green
    '#f59f00',  // amber
    '#748ffc',  // indigo
    '#fa5252',  // crimson
  ];

  // Server always gets the first palette slot for consistency.
  var SENDER_SERVER_COLOR = PALETTE[0];

  // ── State ──────────────────────────────────────────────────────────────────
  var _senderColors = {};   // senderKey → hex color
  var _senderNames  = {};   // senderKey → human name (for filter dropdown)
  var _paletteIndex = 1;    // next palette slot (0 reserved for server)
  var _lineCount    = 0;
  var _activeFilter = 'all';

  // ── Helpers ────────────────────────────────────────────────────────────────

  function _colorForSender(senderKey, senderType) {
    if (_senderColors[senderKey]) return _senderColors[senderKey];
    var color;
    if (senderType === 'server') {
      color = SENDER_SERVER_COLOR;
    } else {
      color = PALETTE[_paletteIndex % PALETTE.length];
      _paletteIndex++;
    }
    _senderColors[senderKey] = color;
    return color;
  }

  function _formatTs(ts) {
    if (!ts) return '';
    var d = new Date(ts);
    var hh = String(d.getHours()).padStart(2, '0');
    var mm = String(d.getMinutes()).padStart(2, '0');
    var ss = String(d.getSeconds()).padStart(2, '0');
    var ms = String(d.getMilliseconds()).padStart(3, '0');
    return hh + ':' + mm + ':' + ss + '.' + ms;
  }

  // ── Filter dropdown management ─────────────────────────────────────────────

  function _ensureSenderInFilter(senderKey, senderName) {
    if (_senderNames[senderKey]) return;
    _senderNames[senderKey] = senderName;

    var sel = document.getElementById('log-filter');
    if (!sel) return;

    var opt = document.createElement('option');
    opt.value       = senderKey;
    opt.textContent = senderName;
    sel.appendChild(opt);
  }

  // ── Line rendering ─────────────────────────────────────────────────────────

  function _appendLine(entry) {
    var output = document.getElementById('log-output');
    if (!output) return;

    var senderKey  = entry.source   || entry.senderType || 'unknown';
    var senderName = entry.senderName || senderKey;
    var color      = _colorForSender(senderKey, entry.senderType);

    _ensureSenderInFilter(senderKey, senderName);

    // Respect active filter
    var visible = (_activeFilter === 'all' || _activeFilter === senderKey);

    var line = document.createElement('div');
    line.className          = 'log-line';
    line.dataset.senderKey  = senderKey;
    if (!visible) line.style.display = 'none';

    // Timestamp
    var tsSpan = document.createElement('span');
    tsSpan.className   = 'log-ts';
    tsSpan.textContent = _formatTs(entry.ts);

    // Sender badge
    var badgeSpan = document.createElement('span');
    badgeSpan.className   = 'log-sender';
    badgeSpan.style.color = color;
    badgeSpan.textContent = senderName;

    // Message (XSS-safe via textContent)
    var msgSpan = document.createElement('span');
    msgSpan.className   = 'log-msg';
    msgSpan.textContent = entry.message || '';
    // Dim warn/error prefix visually
    if (entry.level === 'error') {
      msgSpan.style.color = '#ff6b6b';
    } else if (entry.level === 'warn') {
      msgSpan.style.color = '#ffd43b';
    }

    line.appendChild(tsSpan);
    line.appendChild(badgeSpan);
    line.appendChild(msgSpan);
    output.appendChild(line);
    _lineCount++;

    _trimLines(output);
    _maybeScroll(output);
  }

  function _trimLines(output) {
    var maxInput = document.getElementById('log-maxlines');
    var max = maxInput ? (parseInt(maxInput.value, 10) || 500) : 500;
    while (_lineCount > max && output.firstChild) {
      output.removeChild(output.firstChild);
      _lineCount--;
    }
  }

  function _maybeScroll(output) {
    var cb = document.getElementById('log-autoscroll');
    if (cb && cb.checked) {
      output.scrollTop = output.scrollHeight;
    }
  }

  // ── Filter change handler ──────────────────────────────────────────────────

  function _applyFilter(senderKey) {
    _activeFilter = senderKey;
    var output = document.getElementById('log-output');
    if (!output) return;
    var lines = output.querySelectorAll('.log-line');
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      var match = (senderKey === 'all' || line.dataset.senderKey === senderKey);
      line.style.display = match ? '' : 'none';
    }
    // Re-scroll if autoscroll is on
    var cb = document.getElementById('log-autoscroll');
    if (cb && cb.checked) output.scrollTop = output.scrollHeight;
  }

  // ── Status badge ───────────────────────────────────────────────────────────

  function _setStatus(text, ok) {
    var badge = document.getElementById('log-status');
    if (!badge) return;
    badge.textContent = text;
    badge.className = 'log-status-badge ' + (ok ? 'log-status-ok' : 'log-status-err');
  }

  // ── Init ───────────────────────────────────────────────────────────────────

  function init() {
    // Filter change
    var sel = document.getElementById('log-filter');
    if (sel) {
      sel.addEventListener('change', function () {
        _applyFilter(sel.value);
      });
    }

    // Clear button
    var clearBtn = document.getElementById('log-clear');
    if (clearBtn) {
      clearBtn.addEventListener('click', function () {
        var output = document.getElementById('log-output');
        if (output) output.innerHTML = '';
        _lineCount = 0;
      });
    }

    // Max-lines: trim immediately when the user changes the value
    var maxInput = document.getElementById('log-maxlines');
    if (maxInput) {
      maxInput.addEventListener('change', function () {
        var output = document.getElementById('log-output');
        if (output) _trimLines(output);
      });
    }

    // WebSocket
    Derby.Connection.onMessage(function (msg) {
      if (msg.type === 'log_line') {
        _appendLine(msg.payload || {});
      }
    });

    Derby.Connection.connect('', 'display');

    // Track WS status via the badge
    // Derby.Connection uses #ws-status internally; we mirror it for the badge
    var observer = new MutationObserver(function () {
      var wsEl = document.getElementById('ws-status');
      if (!wsEl) return;
      var ok = wsEl.classList.contains('ws-connected');
      var connecting = wsEl.classList.contains('ws-connecting');
      if (ok) {
        _setStatus('⬤ Connected', true);
      } else if (connecting) {
        _setStatus('⬤ Connecting…', false);
      } else {
        _setStatus('⬤ Disconnected', false);
      }
    });

    var wsEl = document.getElementById('ws-status');
    if (wsEl) observer.observe(wsEl, { attributes: true, childList: true, characterData: true });
  }

  document.addEventListener('DOMContentLoaded', init);

  return { /* no public API needed */ };
}());
