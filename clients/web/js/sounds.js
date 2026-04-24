/* eslint-disable no-var */
/**
 * Admin → Sounds Config page logic.
 * Loads /api/sounds/config, renders a per-event URL table, saves back via POST.
 * Preview button plays the currently-entered URL (or default if empty) via
 * the shared DerbyAudio player, so what you see is what clients will hear.
 */
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };

  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }

  var state = { events: [], urls: {}, overrides: {}, defaults: {} };

  function setStatus(msg, isError) {
    var el = $('sounds-status');
    el.textContent = msg || '';
    el.classList.toggle('error', Boolean(isError));
  }

  function showError(msg) {
    var el = $('sounds-error');
    if (!msg) { el.classList.add('hidden'); el.textContent = ''; return; }
    el.textContent = msg;
    el.classList.remove('hidden');
  }

  function render() {
    var tbody = $('sounds-tbody');
    if (!state.events.length) {
      tbody.innerHTML = '<tr><td colspan="4" class="muted">No events.</td></tr>';
      return;
    }
    var rows = state.events.map(function (ev) {
      var def      = state.defaults[ev];
      var override = state.overrides[ev] || '';
      var defHtml  = def
        ? '<code>' + esc(def) + '</code>'
        : '<span class="muted">(no file)</span>';
      return (
        '<tr data-event="' + esc(ev) + '">' +
          '<td><code>' + esc(ev) + '</code></td>' +
          '<td>' + defHtml + '</td>' +
          '<td><input type="url" class="sound-url" ' +
               'placeholder="' + (def ? esc(def) : 'https://example.com/sound.mp3') + '" ' +
               'value="' + esc(override) + '" style="width:100%;"></td>' +
          '<td><button type="button" class="btn btn-secondary btn-sm sound-preview">▶ Play</button></td>' +
        '</tr>'
      );
    }).join('');
    tbody.innerHTML = rows;
  }

  function collectUrls() {
    var urls = {};
    var rows = document.querySelectorAll('#sounds-tbody tr[data-event]');
    rows.forEach(function (tr) {
      var ev    = tr.getAttribute('data-event');
      var input = tr.querySelector('input.sound-url');
      var v     = (input && input.value || '').trim();
      if (v) urls[ev] = v;
    });
    return urls;
  }

  function load() {
    setStatus('Loading…');
    return fetch('/api/sounds/config', { cache: 'no-cache' })
      .then(function (r) {
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return r.json();
      })
      .then(function (cfg) {
        state = {
          events:    cfg.events    || [],
          urls:      cfg.urls      || {},
          overrides: cfg.overrides || {},
          defaults:  cfg.defaults  || {},
        };
        render();
        setStatus('Loaded ' + state.events.length + ' events.');
        showError('');
      })
      .catch(function (err) {
        setStatus('Load failed', true);
        showError(err.message);
      });
  }

  function save() {
    var urls = collectUrls();
    setStatus('Saving…');
    showError('');
    fetch('/api/sounds/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ urls: urls }),
    }).then(function (r) {
      if (!r.ok) return r.json().then(function (j) { throw new Error(j.error || ('HTTP ' + r.status)); });
      return r.json();
    }).then(function (cfg) {
      state = {
        events:    cfg.events    || [],
        urls:      cfg.urls      || {},
        overrides: cfg.overrides || {},
        defaults:  cfg.defaults  || {},
      };
      render();
      setStatus('Saved.');
      if (window.DerbyAudio && typeof window.DerbyAudio.refresh === 'function') {
        window.DerbyAudio.refresh();
      }
    }).catch(function (err) {
      setStatus('Save failed', true);
      showError(err.message);
    });
  }

  function previewRow(tr) {
    var ev    = tr.getAttribute('data-event');
    var input = tr.querySelector('input.sound-url');
    var url   = (input && input.value || '').trim() || state.defaults[ev];
    if (!url) { setStatus('No URL for ' + ev, true); return; }
    try {
      var audio = new Audio(url);
      audio.play().catch(function (err) {
        setStatus('Playback failed: ' + err.message, true);
      });
    } catch (err) {
      setStatus('Playback failed: ' + err.message, true);
    }
  }

  document.addEventListener('DOMContentLoaded', function () {
    $('btn-sounds-save').addEventListener('click', save);
    $('btn-sounds-reset').addEventListener('click', function () { render(); setStatus('Reset.'); });
    $('sounds-tbody').addEventListener('click', function (e) {
      var btn = e.target.closest && e.target.closest('.sound-preview');
      if (!btn) return;
      var tr = btn.closest('tr[data-event]');
      if (tr) previewRow(tr);
    });
    load();
  });
}());
