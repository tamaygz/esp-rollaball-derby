'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.Admin — game control buttons, config form, and player renaming.
 *
 * Public API:
 *   Derby.Admin.init()                    — attach event listeners
 *   Derby.Admin.renamePlayer(id, name)    — PUT /api/players/:id
 */
Derby.Admin = (function () {
  function _el(id) { return document.getElementById(id); }

  // ── Error display ───────────────────────────────────────────────────────────

  function _showError(elId, msg) {
    var el = _el(elId);
    if (!el) return;
    el.textContent = msg;
    el.classList.remove('hidden');
    setTimeout(function () { el.classList.add('hidden'); }, 5000);
  }

  // ── REST helpers ────────────────────────────────────────────────────────────

  function _post(path) {
    return fetch(path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    }).then(function (res) { return res.json(); });
  }

  function _put(path, body) {
    return fetch(path, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    }).then(function (res) { return res.json(); });
  }

  function _delete(path) {
    return fetch(path, {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
    }).then(function (res) { return res.json(); });
  }

  // ── Game control ────────────────────────────────────────────────────────────

  function _onStart() {
    _post('/api/game/start').then(function (data) {
      if (data.error) _showError('ctrl-error', data.error);
    }).catch(function (e) {
      _showError('ctrl-error', 'Network error: ' + e.message);
    });
  }

  function _onPause() {
    _post('/api/game/pause').then(function (data) {
      if (data.error) _showError('ctrl-error', data.error);
    }).catch(function (e) {
      _showError('ctrl-error', 'Network error: ' + e.message);
    });
  }

  function _onReset() {
    if (!window.confirm('Reset the game? All player positions will be cleared.')) return;
    _post('/api/game/reset').then(function (data) {
      if (data.error) _showError('ctrl-error', data.error);
    }).catch(function (e) {
      _showError('ctrl-error', 'Network error: ' + e.message);
    });
  }

  function _onSaveConfig(e) {
    e.preventDefault();
    var trackLength = parseInt((_el('cfg-track') || {}).value, 10);
    var maxPlayers  = parseInt((_el('cfg-max')   || {}).value, 10);
    var themeEl     = _el('cfg-theme');
    var theme       = themeEl ? themeEl.value : 'horse';

    _put('/api/game/config', { trackLength: trackLength, maxPlayers: maxPlayers, theme: theme })
      .then(function (data) {
        if (data.error) _showError('config-error', data.error);
      })
      .catch(function (e) {
        _showError('config-error', 'Network error: ' + e.message);
      });
  }

  // ── Player management ───────────────────────────────────────────────────────

  function renamePlayer(id, name) {
    return _put('/api/players/' + encodeURIComponent(id), { name: name })
      .then(function (data) {
        if (data.error) console.error('[Derby.Admin] Rename failed:', data.error);
      })
      .catch(function (e) {
        console.error('[Derby.Admin] Rename error:', e.message);
      });
  }

  function removePlayer(id) {
    return _delete('/api/players/' + encodeURIComponent(id))
      .then(function (data) {
        if (data.error) console.error('[Derby.Admin] Remove failed:', data.error);
      })
      .catch(function (e) {
        console.error('[Derby.Admin] Remove error:', e.message);
      });
  }

  // ── Init ────────────────────────────────────────────────────────────────────

  function init() {
    var btnStart  = _el('btn-start');
    var btnPause  = _el('btn-pause');
    var btnReset  = _el('btn-reset');
    var formCfg   = _el('form-config');

    if (btnStart)  btnStart.addEventListener('click', _onStart);
    if (btnPause)  btnPause.addEventListener('click', _onPause);
    if (btnReset)  btnReset.addEventListener('click', _onReset);
    if (formCfg)   formCfg.addEventListener('submit', _onSaveConfig);
  }

  return { init: init, renamePlayer: renamePlayer, removePlayer: removePlayer };
}());
