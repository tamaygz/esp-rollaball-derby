'use strict';

/**
 * Devices debug page — polls GET /api/clients every 3 s and renders connected
 * WebSocket clients grouped by type. Allows forcibly closing (kicking) any
 * client via DELETE /api/clients/:id.
 */
(function () {
  var POLL_INTERVAL = 3000;
  var _pollTimer = null;

  function _el(id) { return document.getElementById(id); }

  function _esc(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function _ts() {
    return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

  // ── REST ────────────────────────────────────────────────────────────────────

  function _fetchClients() {
    return fetch('/api/clients').then(function (res) { return res.json(); });
  }

  function _kickClient(id) {
    return fetch('/api/clients/' + encodeURIComponent(id), { method: 'DELETE' })
      .then(function (res) { return res.json(); });
  }

  // ── Render ──────────────────────────────────────────────────────────────────

  function _renderDeviceRow(client) {
    var idShort = client.id ? (client.id.slice(0, 8) + '…') : '—';
    var name = client.playerName
      ? '<span class="device-name">' + _esc(client.playerName) + '</span>'
      : '<span class="device-name muted">—</span>';
    var pos = client.playerPosition != null
      ? '<span class="device-pos">pos&nbsp;' + client.playerPosition + '</span>'
      : '';

    return (
      '<div class="device-row" data-id="' + _esc(client.id) + '">' +
        '<div class="device-info">' +
          '<span class="device-id" title="' + _esc(client.id) + '">' + _esc(idShort) + '</span>' +
          name +
        '</div>' +
        '<div class="device-meta">' +
          pos +
          '<button class="btn-kick" data-id="' + _esc(client.id) + '"' +
            ' aria-label="Kick client ' + _esc(idShort) + '">Kick</button>' +
        '</div>' +
      '</div>'
    );
  }

  function _renderList(containerId, clients) {
    var el = _el(containerId);
    if (!el) return;
    if (!clients || clients.length === 0) {
      el.innerHTML = '<p class="empty-msg">None connected.</p>';
      return;
    }
    el.innerHTML = clients.map(_renderDeviceRow).join('');
    el.querySelectorAll('.btn-kick').forEach(function (btn) {
      btn.addEventListener('click', function () {
        if (!window.confirm('Kick this client? Their connection will be closed.')) return;
        _kickClient(btn.dataset.id)
          .then(function () { _refresh(); })
          .catch(function (e) { console.error('[Devices] Kick failed:', e); });
      });
    });
  }

  function _render(clients) {
    var groups = { sensor: [], motor: [], display: [], web: [], unregistered: [] };
    clients.forEach(function (c) {
      var key = c.type || 'unregistered';
      if (!groups[key]) groups[key] = [];
      groups[key].push(c);
    });

    _renderList('sensor-list',      groups.sensor);
    _renderList('motor-list',       groups.motor);
    _renderList('display-list',     groups.display);
    _renderList('web-list',         groups.web);
    _renderList('unregistered-list', groups.unregistered);

    // Summary badges
    var summary = _el('devices-summary');
    if (summary) {
      var parts = [];
      if (groups.sensor.length)
        parts.push('<span class="dev-badge dev-sensor">' + groups.sensor.length + ' sensor</span>');
      if (groups.motor.length)
        parts.push('<span class="dev-badge dev-motor">' + groups.motor.length + ' motor</span>');
      if (groups.display.length)
        parts.push('<span class="dev-badge dev-display">' + groups.display.length + ' display</span>');
      if (groups.web.length)
        parts.push('<span class="dev-badge dev-web">' + groups.web.length + ' web</span>');
      if (groups.unregistered.length)
        parts.push('<span class="dev-badge dev-unknown">' + groups.unregistered.length + ' unregistered</span>');
      summary.innerHTML = parts.length
        ? parts.join('')
        : '<span class="muted">No clients connected.</span>';
    }

    var rs = _el('refresh-status');
    if (rs) rs.textContent = 'Last updated: ' + _ts();
  }

  // ── Refresh ─────────────────────────────────────────────────────────────────

  function _refresh() {
    _fetchClients()
      .then(function (clients) { _render(clients); })
      .catch(function (e) {
        console.error('[Devices] Fetch failed:', e);
        var rs = _el('refresh-status');
        if (rs) rs.textContent = 'Error: ' + e.message;
      });
  }

  function _startPolling() {
    _refresh();
    _pollTimer = setInterval(_refresh, POLL_INTERVAL);
  }

  // ── Init ────────────────────────────────────────────────────────────────────

  var btnRefresh = _el('btn-refresh');
  if (btnRefresh) btnRefresh.addEventListener('click', _refresh);

  // Pause polling when the tab is hidden to save server resources
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      clearInterval(_pollTimer);
      _pollTimer = null;
    } else {
      _startPolling();
    }
  });

  _startPolling();
}());
