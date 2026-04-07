'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.Test — score simulation panel.
 *
 * Public API:
 *   Derby.Test.init()               — attach event listeners
 *   Derby.Test.syncButtons()        — refresh disabled state (called from state.js)
 */
Derby.Test = (function () {
  function _el(id) { return document.getElementById(id); }

  function _flash(btn) {
    btn.classList.add('btn-flash');
    setTimeout(function () { btn.classList.remove('btn-flash'); }, 200);
  }

  /** Re-evaluate whether score buttons should be enabled. */
  function syncButtons() {
    var select   = _el('score-player');
    var status   = Derby.State ? Derby.State.getStatus() : 'idle';
    var playerId = select ? select.value : '';
    var canScore = (status === 'running') && !!playerId;
    var btn1 = _el('btn-score-1');
    var btn2 = _el('btn-score-2');
    var btn3 = _el('btn-score-3');
    if (btn1) btn1.disabled = !canScore;
    if (btn2) btn2.disabled = !canScore;
    if (btn3) btn3.disabled = !canScore;
  }

  function _sendScore(points) {
    var select   = _el('score-player');
    var playerId = select ? select.value : '';
    if (!playerId) return;
    Derby.Connection.send({ type: 'score', payload: { playerId: playerId, points: points } });
  }

  function init() {
    var select = _el('score-player');
    var btn1   = _el('btn-score-1');
    var btn2   = _el('btn-score-2');
    var btn3   = _el('btn-score-3');

    if (select) {
      select.addEventListener('change', syncButtons);
    }

    if (btn1) {
      btn1.addEventListener('click', function () {
        _flash(btn1);
        _sendScore(1);
      });
    }

    if (btn2) {
      btn2.addEventListener('click', function () {
        _flash(btn2);
        _sendScore(2);
      });
    }

    if (btn3) {
      btn3.addEventListener('click', function () {
        _flash(btn3);
        _sendScore(3);
      });
    }
  }

  return { init: init, syncButtons: syncButtons };
}());
