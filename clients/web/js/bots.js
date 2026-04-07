'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.Bots — manages server-side bot players via REST API.
 *
 * Each bot is a fully autonomous server-side player that scores at random
 * human-like intervals (2–8 s) while the game is running.  No client-side
 * timers needed — the server runs the bots.
 *
 * Public API:
 *   Derby.Bots.init()              — attach event listeners
 *   Derby.Bots.syncState(payload)  — refresh bot list from server
 */
Derby.Bots = (function () {
  var _bots   = [];   // [{ id, playerId, playerName }]
  var _status = 'idle';

  function _el(id) { return document.getElementById(id); }

  function _esc(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  // ── REST helpers ──────────────────────────────────────────────────────────

  function _fetchBots() {
    return fetch('/api/bots')
      .then(function (r) { return r.json(); })
      .then(function (list) {
        _bots = list || [];
        _renderBotList();
      })
      .catch(function (e) { console.error('[Bots] fetch error:', e.message); });
  }

  function _addBot() {
    fetch('/api/bots', { method: 'POST', headers: { 'Content-Type': 'application/json' } })
      .then(function (r) { return r.json(); })
      .then(function (bot) {
        if (bot.error) { console.error('[Bots]', bot.error); return; }
        _bots.push(bot);
        _renderBotList();
      })
      .catch(function (e) { console.error('[Bots] add error:', e.message); });
  }

  function _removeBot(botId) {
    fetch('/api/bots/' + encodeURIComponent(botId), { method: 'DELETE' })
      .then(function (r) { return r.json(); })
      .then(function () { _fetchBots(); })
      .catch(function (e) { console.error('[Bots] remove error:', e.message); });
  }

  // ── Rendering ─────────────────────────────────────────────────────────────

  function _renderBotList() {
    var list = _el('bot-list');
    if (!list) return;

    if (_bots.length === 0) {
      list.innerHTML = '<p class="empty-msg">No bots added yet.</p>';
      return;
    }

    var isRunning = (_status === 'running');
    list.innerHTML = _bots.map(function (bot) {
      return (
        '<div class="bot-row" data-id="' + _esc(bot.id) + '">' +
          '<span class="bot-player-name">🤖 ' + _esc(bot.playerName) + '</span>' +
          '<span class="bot-status-badge ' + (isRunning ? 'bot-active' : 'bot-idle') + '">' +
            (isRunning ? 'rolling' : 'waiting') +
          '</span>' +
          '<button class="btn btn-bot-remove" data-id="' + _esc(bot.id) + '" ' +
            'aria-label="Remove bot ' + _esc(bot.playerName) + '">✕</button>' +
        '</div>'
      );
    }).join('');

    list.querySelectorAll('.btn-bot-remove').forEach(function (btn) {
      btn.addEventListener('click', function () {
        _removeBot(btn.dataset.id);
      });
    });
  }

  // ── Public API ────────────────────────────────────────────────────────────

  function syncState(statePayload) {
    _status = (statePayload && statePayload.status) || 'idle';
    _fetchBots();
  }

  function init() {
    var btnAdd = _el('btn-add-bot');
    if (btnAdd) btnAdd.addEventListener('click', _addBot);
    _fetchBots();
  }

  return { init: init, syncState: syncState };
}());

