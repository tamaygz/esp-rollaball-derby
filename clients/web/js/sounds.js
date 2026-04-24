/* eslint-disable no-var */
/**
 * Admin → Sounds Config page logic.
 *
 * Manages two config cards:
 *  1. Effect sounds — per-event URL overrides (existing table).
 *  2. Background music — volumes, lobby music, per-theme music, fallback music.
 *
 * All config is stored and sent as:
 *   {
 *     urls: { <event>: "<url>", … },
 *     volumes: { lobby: 0–1, game: 0–1, effects: 0–1 },
 *     lobbyMusic: ["url", …],
 *     themeMusic: { <theme>: ["url", …] },
 *     fallbackMusic: ["url", …],
 *   }
 *
 * Volume is displayed/entered as 0–100 integer but stored/sent as 0–1 float.
 */
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };

  function esc(s) {
    return String(s == null ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
  }

  var state = {
    events:       [],
    urls:         {},
    overrides:    {},
    defaults:     {},
    volumes:      { lobby: 0.6, game: 0.5, effects: 0.9 },
    lobbyMusic:   [],
    themeMusic:   {},
    fallbackMusic: [],
  };
  var _themes = [];  // concrete theme names from themes manifest

  // ── status helpers ──────────────────────────────────────────────────────────

  function setStatus(msg, isError) {
    var el = $('sounds-status');
    if (!el) return;
    el.textContent = msg || '';
    el.classList.toggle('error', Boolean(isError));
  }

  function showError(msg) {
    var el = $('sounds-error');
    if (!el) return;
    if (!msg) { el.classList.add('hidden'); el.textContent = ''; return; }
    el.textContent = msg;
    el.classList.remove('hidden');
  }

  // ── Effects table ───────────────────────────────────────────────────────────

  function render() {
    var tbody = $('sounds-tbody');
    if (!tbody) return;
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

  // ── Background music UI ─────────────────────────────────────────────────────

  var VOLUME_LABELS = { lobby: 'Lobby music', game: 'Game background', effects: 'Sound effects' };

  function renderMusic() {
    _renderVolumes();
    _renderLobbyMusic();
    _renderThemeMusic();
    _renderFallbackMusic();
  }

  function _renderVolumes() {
    var container = $('sounds-volumes');
    if (!container) return;
    var vols = state.volumes || {};
    var html = '<table class="sounds-vol-table">';
    ['lobby', 'game', 'effects'].forEach(function (key) {
      var pct = Math.round((typeof vols[key] === 'number' ? vols[key] : { lobby: 0.6, game: 0.5, effects: 0.9 }[key]) * 100);
      html +=
        '<tr class="volume-row">' +
          '<td class="volume-label">' + esc(VOLUME_LABELS[key]) + '</td>' +
          '<td><input type="range" min="0" max="100" value="' + pct + '" ' +
               'class="volume-slider" data-vol-key="' + esc(key) + '" ' +
               'aria-label="' + esc(VOLUME_LABELS[key]) + ' volume"></td>' +
          '<td class="volume-display" id="vol-display-' + esc(key) + '">' + pct + '%</td>' +
        '</tr>';
    });
    html += '</table>';
    container.innerHTML = html;

    // Live-update display value
    container.querySelectorAll('input.volume-slider').forEach(function (input) {
      input.addEventListener('input', function () {
        var key = input.getAttribute('data-vol-key');
        var display = $('vol-display-' + key);
        if (display) display.textContent = input.value + '%';
      });
    });
  }

  function _renderLobbyMusic() {
    var ta = $('lobby-music-urls');
    if (!ta) return;
    ta.value = (state.lobbyMusic || []).join('\n');
  }

  function _renderThemeMusic() {
    var container = $('theme-music-sections');
    if (!container) return;
    if (!_themes.length) {
      container.innerHTML = '<p class="muted">No themes found.</p>';
      return;
    }
    var html = _themes.map(function (theme) {
      var tracks = (state.themeMusic && state.themeMusic[theme]) ? state.themeMusic[theme] : [];
      return (
        '<div class="sounds-subsection">' +
          '<label class="music-label">🎨 ' + esc(theme) + ' theme</label>' +
          '<textarea class="music-urls theme-music-urls" rows="3" ' +
                    'data-theme="' + esc(theme) + '" ' +
                    'placeholder="https://example.com/' + esc(theme) + '-track.mp3">' +
            esc(tracks.join('\n')) +
          '</textarea>' +
          '<button type="button" class="btn btn-secondary btn-sm music-preview-btn" ' +
                  'data-preview-source="theme" data-preview-theme="' + esc(theme) + '">▶ Preview first</button>' +
        '</div>'
      );
    }).join('');
    container.innerHTML = html;
  }

  function _renderFallbackMusic() {
    var ta = $('fallback-music-urls');
    if (!ta) return;
    ta.value = (state.fallbackMusic || []).join('\n');
  }

  // ── Data collection ─────────────────────────────────────────────────────────

  function _parseUrlList(text) {
    return (text || '').split('\n')
      .map(function (s) { return s.trim(); })
      .filter(function (s) { return s.length > 0; });
  }

  function collectConfig() {
    // Effect URL overrides
    var urls = {};
    var rows = document.querySelectorAll('#sounds-tbody tr[data-event]');
    rows.forEach(function (tr) {
      var ev    = tr.getAttribute('data-event');
      var input = tr.querySelector('input.sound-url');
      var v     = (input && input.value || '').trim();
      if (v) urls[ev] = v;
    });

    // Volumes
    var volumes = { lobby: 0.6, game: 0.5, effects: 0.9 };
    document.querySelectorAll('input.volume-slider').forEach(function (input) {
      var key = input.getAttribute('data-vol-key');
      if (key) volumes[key] = Math.max(0, Math.min(1, Number(input.value) / 100));
    });

    // Lobby music
    var lobbyTa = $('lobby-music-urls');
    var lobbyMusic = lobbyTa ? _parseUrlList(lobbyTa.value) : [];

    // Per-theme music
    var themeMusic = {};
    document.querySelectorAll('textarea.theme-music-urls').forEach(function (ta) {
      var theme  = ta.getAttribute('data-theme');
      var tracks = _parseUrlList(ta.value);
      if (theme && tracks.length) themeMusic[theme] = tracks;
    });

    // Fallback music
    var fallbackTa = $('fallback-music-urls');
    var fallbackMusic = fallbackTa ? _parseUrlList(fallbackTa.value) : [];

    return { urls: urls, volumes: volumes, lobbyMusic: lobbyMusic, themeMusic: themeMusic, fallbackMusic: fallbackMusic };
  }

  // ── Load / Save ─────────────────────────────────────────────────────────────

  function load() {
    setStatus('Loading…');
    return Promise.all([
      fetch('/assets/themes/shared/themes.json', { cache: 'no-cache' })
        .then(function (r) { return r.ok ? r.json() : {}; })
        .catch(function () { return {}; }),
      fetch('/api/sounds/config', { cache: 'no-cache' })
        .then(function (r) {
          if (!r.ok) throw new Error('HTTP ' + r.status);
          return r.json();
        }),
    ]).then(function (results) {
      var themesManifest = results[0];
      var cfg            = results[1];
      _themes = Array.isArray(themesManifest.concreteThemes) ? themesManifest.concreteThemes : [];
      state = {
        events:       cfg.events       || [],
        urls:         cfg.urls         || {},
        overrides:    cfg.overrides    || {},
        defaults:     cfg.defaults     || {},
        volumes:      cfg.volumes      || { lobby: 0.6, game: 0.5, effects: 0.9 },
        lobbyMusic:   cfg.lobbyMusic   || [],
        themeMusic:   cfg.themeMusic   || {},
        fallbackMusic: cfg.fallbackMusic || [],
      };
      render();
      renderMusic();
      setStatus('Loaded.');
      showError('');
    }).catch(function (err) {
      setStatus('Load failed', true);
      showError(err.message);
    });
  }

  function save() {
    var config = collectConfig();
    setStatus('Saving…');
    showError('');
    fetch('/api/sounds/config', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(config),
    }).then(function (r) {
      if (!r.ok) return r.json().then(function (j) { throw new Error(j.error || ('HTTP ' + r.status)); });
      return r.json();
    }).then(function (cfg) {
      state = {
        events:       cfg.events       || [],
        urls:         cfg.urls         || {},
        overrides:    cfg.overrides    || {},
        defaults:     cfg.defaults     || {},
        volumes:      cfg.volumes      || state.volumes,
        lobbyMusic:   cfg.lobbyMusic   || [],
        themeMusic:   cfg.themeMusic   || {},
        fallbackMusic: cfg.fallbackMusic || [],
      };
      render();
      renderMusic();
      setStatus('Saved.');
      if (window.DerbyAudio && typeof window.DerbyAudio.refresh === 'function') {
        window.DerbyAudio.refresh();
      }
    }).catch(function (err) {
      setStatus('Save failed', true);
      showError(err.message);
    });
  }

  // ── Preview helpers ─────────────────────────────────────────────────────────

  function previewRow(tr) {
    var ev    = tr.getAttribute('data-event');
    var input = tr.querySelector('input.sound-url');
    var url   = (input && input.value || '').trim() || state.defaults[ev];
    if (!url) { setStatus('No URL for ' + ev, true); return; }
    try {
      var audio = new Audio(url);
      audio.play().catch(function (err) { setStatus('Playback failed: ' + err.message, true); });
    } catch (err) {
      setStatus('Playback failed: ' + err.message, true);
    }
  }

  function previewFirstTrack(tracks) {
    var url = (tracks || [])[0];
    if (!url) { setStatus('No tracks to preview.', true); return; }
    try {
      var audio = new Audio(url);
      audio.play().catch(function (err) { setStatus('Playback failed: ' + err.message, true); });
    } catch (err) {
      setStatus('Playback failed: ' + err.message, true);
    }
  }

  // ── Wire up ─────────────────────────────────────────────────────────────────

  document.addEventListener('DOMContentLoaded', function () {
    var saveBtn  = $('btn-sounds-save');
    var resetBtn = $('btn-sounds-reset');
    var tbody    = $('sounds-tbody');
    var musicDiv = $('card-sounds-music');

    if (saveBtn)  saveBtn.addEventListener('click', save);
    if (resetBtn) resetBtn.addEventListener('click', function () { render(); renderMusic(); setStatus('Reset.'); });
    if (tbody) {
      tbody.addEventListener('click', function (e) {
        var btn = e.target.closest && e.target.closest('.sound-preview');
        if (!btn) return;
        var tr = btn.closest('tr[data-event]');
        if (tr) previewRow(tr);
      });
    }

    // Preview buttons in the music section (lobby, theme, fallback)
    if (musicDiv) {
      musicDiv.addEventListener('click', function (e) {
        var btn = e.target.closest && e.target.closest('.music-preview-btn');
        if (!btn) return;
        var src   = btn.getAttribute('data-preview-source');
        var theme = btn.getAttribute('data-preview-theme');
        if (src === 'lobby') {
          var lobbyTa = $('lobby-music-urls');
          previewFirstTrack(lobbyTa ? _parseUrlList(lobbyTa.value) : []);
        } else if (src === 'theme' && theme) {
          var ta = musicDiv.querySelector('textarea[data-theme="' + theme + '"]');
          previewFirstTrack(ta ? _parseUrlList(ta.value) : []);
        } else if (src === 'fallback') {
          var fallbackTa = $('fallback-music-urls');
          previewFirstTrack(fallbackTa ? _parseUrlList(fallbackTa.value) : []);
        }
      });
    }

    load();
  });
}());
