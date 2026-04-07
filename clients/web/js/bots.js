'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.Bots — virtual sensor bots that simulate score rolls.
 *
 * Each bot is tied to a player and independently fires at random
 * human-like intervals (2–8 s) while the game is running.
 *
 * Roll probability table (number drawn from 0–100 inclusive):
 *   0–10  → +3 points  (~10.9 %)
 *   11–25 → +2 points  (~14.9 %)
 *   26–55 → +1 point   (~29.7 %)
 *   56–100 → 0 points  (~44.6 %)
 *
 * Public API:
 *   Derby.Bots.init()              — attach event listeners
 *   Derby.Bots.syncState(payload)  — update game status + player list
 */
Derby.Bots = (function () {
  var _bots = [];       // [{id, playerId, playerName, timer}]
  var _status = 'idle';
  var _nextBotId = 1;

  function _el(id) { return document.getElementById(id); }

  function _esc(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  // ── Roll probability ────────────────────────────────────────────────────────

  function _calcPoints(roll) {
    if (roll <= 10) return 3;
    if (roll <= 25) return 2;
    if (roll <= 55) return 1;
    return 0;
  }

  // ── Per-bot timer logic ─────────────────────────────────────────────────────

  function _scheduleNext(bot) {
    if (bot.timer) clearTimeout(bot.timer);
    // Human-like delay: 2–8 s
    var delay = 2000 + Math.floor(Math.random() * 6000);
    bot.timer = setTimeout(function () {
      bot.timer = null;
      _rollFor(bot);
    }, delay);
  }

  function _rollFor(bot) {
    if (_status !== 'running') return;
    var roll = Math.floor(Math.random() * 101); // 0–100 inclusive
    var points = _calcPoints(roll);
    if (points > 0) {
      Derby.Connection.send({ type: 'score', payload: { playerId: bot.playerId, points: points } });
    }
    _scheduleNext(bot);
  }

  function _startBot(bot) {
    if (bot.timer) return;
    // Stagger initial throw (0.5–2.5 s) so bots don't all fire simultaneously
    var initialDelay = 500 + Math.floor(Math.random() * 2000);
    bot.timer = setTimeout(function () {
      bot.timer = null;
      _rollFor(bot);
    }, initialDelay);
  }

  function _stopBot(bot) {
    if (bot.timer) { clearTimeout(bot.timer); bot.timer = null; }
  }

  // ── Rendering ───────────────────────────────────────────────────────────────

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
        '<div class="bot-row" data-id="' + bot.id + '">' +
          '<span class="bot-player-name">' + _esc(bot.playerName) + '</span>' +
          '<span class="bot-status-badge ' + (isRunning ? 'bot-active' : 'bot-idle') + '">' +
            (isRunning ? 'rolling' : 'waiting') +
          '</span>' +
          '<button class="btn btn-bot-remove" data-id="' + bot.id + '" ' +
            'aria-label="Remove bot for ' + _esc(bot.playerName) + '">✕</button>' +
        '</div>'
      );
    }).join('');

    list.querySelectorAll('.btn-bot-remove').forEach(function (btn) {
      btn.addEventListener('click', function () {
        _removeBot(parseInt(btn.dataset.id, 10));
      });
    });
  }

  function _updatePlayerSelect(players) {
    var select = _el('bot-player-select');
    if (!select) return;
    var prev = select.value;
    var opts = '<option value="">— select player —</option>' +
      (players || []).map(function (p) {
        return '<option value="' + _esc(p.id) + '"' + (p.id === prev ? ' selected' : '') + '>' + _esc(p.name) + '</option>';
      }).join('');
    select.innerHTML = opts;
  }

  // ── Bot management ──────────────────────────────────────────────────────────

  function _addBot() {
    var select = _el('bot-player-select');
    if (!select || !select.value) return;
    var playerId = select.value;
    var playerName = select.options[select.selectedIndex].text;
    var bot = { id: _nextBotId++, playerId: playerId, playerName: playerName, timer: null };
    _bots.push(bot);
    if (_status === 'running') _startBot(bot);
    _renderBotList();
    select.value = '';
  }

  function _removeBot(botId) {
    for (var i = 0; i < _bots.length; i++) {
      if (_bots[i].id === botId) {
        _stopBot(_bots[i]);
        _bots.splice(i, 1);
        break;
      }
    }
    _renderBotList();
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  /**
   * Called on every incoming `state` message. Updates game status and the
   * player dropdown, and starts/stops bot timers as needed.
   */
  function syncState(statePayload) {
    var newStatus = (statePayload && statePayload.status) || 'idle';
    var players   = (statePayload && statePayload.players) || [];
    var wasRunning = (_status === 'running');
    _status = newStatus;

    if (newStatus === 'running' && !wasRunning) {
      _bots.forEach(_startBot);
    } else if (newStatus !== 'running' && wasRunning) {
      _bots.forEach(_stopBot);
    }

    _updatePlayerSelect(players);
    _renderBotList();
  }

  function init() {
    var btnAdd = _el('btn-add-bot');
    if (btnAdd) btnAdd.addEventListener('click', _addBot);
    _renderBotList();
  }

  return { init: init, syncState: syncState };
}());
