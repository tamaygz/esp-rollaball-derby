'use strict';

/**
 * Devices debug page — polls GET /api/clients every 3 s and renders connected
 * WebSocket clients grouped by type. Allows forcibly closing (kicking) any
 * client via DELETE /api/clients/:id.
 */
(function () {
  var POLL_INTERVAL = 3000;
  var _pollTimer = null;

  // 16-color shared palette (mirrors clients/assets/themes/shared/player-colors.json)
  var PLAYER_COLORS = [
    { index: 0,  hex: '#E53E3E', name: 'Scarlet'       },
    { index: 1,  hex: '#3182CE', name: 'Royal Blue'    },
    { index: 2,  hex: '#38A169', name: 'Emerald'       },
    { index: 3,  hex: '#D69E2E', name: 'Gold'          },
    { index: 4,  hex: '#805AD5', name: 'Amethyst'      },
    { index: 5,  hex: '#D53F8C', name: 'Rose'          },
    { index: 6,  hex: '#00B5D8', name: 'Cyan'          },
    { index: 7,  hex: '#68D391', name: 'Mint'          },
    { index: 8,  hex: '#FF6B35', name: 'Tangerine'     },
    { index: 9,  hex: '#B7791F', name: 'Amber'         },
    { index: 10, hex: '#9B2C2C', name: 'Crimson'       },
    { index: 11, hex: '#2C7A7B', name: 'Teal'          },
    { index: 12, hex: '#744210', name: 'Sienna'        },
    { index: 13, hex: '#553C9A', name: 'Indigo'        },
    { index: 14, hex: '#C05621', name: 'Burnt Sienna'  },
    { index: 15, hex: '#276749', name: 'Forest'        },
  ];

  // 16-color shared palette (mirrors clients/assets/themes/shared/player-colors.json)
  var PLAYER_COLORS = [
    { index: 0,  hex: '#E53E3E', name: 'Scarlet'       },
    { index: 1,  hex: '#3182CE', name: 'Royal Blue'    },
    { index: 2,  hex: '#38A169', name: 'Emerald'       },
    { index: 3,  hex: '#D69E2E', name: 'Gold'          },
    { index: 4,  hex: '#805AD5', name: 'Amethyst'      },
    { index: 5,  hex: '#D53F8C', name: 'Rose'          },
    { index: 6,  hex: '#00B5D8', name: 'Cyan'          },
    { index: 7,  hex: '#68D391', name: 'Mint'          },
    { index: 8,  hex: '#FF6B35', name: 'Tangerine'     },
    { index: 9,  hex: '#B7791F', name: 'Amber'         },
    { index: 10, hex: '#9B2C2C', name: 'Crimson'       },
    { index: 11, hex: '#2C7A7B', name: 'Teal'          },
    { index: 12, hex: '#744210', name: 'Sienna'        },
    { index: 13, hex: '#553C9A', name: 'Indigo'        },
    { index: 14, hex: '#C05621', name: 'Burnt Sienna'  },
    { index: 15, hex: '#276749', name: 'Forest'        },
  ];

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
    _renderMotorList(groups.motor);
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

  // ── Motor Device Rows ────────────────────────────────────────────────────────

  function _renderMotorDeviceRow(client) {
    var idShort = client.id ? (client.id.slice(0, 8) + '…') : '—';
    var name = client.playerName
      ? '<span class="device-name">' + _esc(client.playerName) + '</span>'
      : '<span class="device-name muted">—</span>';
    var laneCount = (client.motorCount != null) ? client.motorCount + ' lanes' : '';

    return (
      '<div class="device-row" data-id="' + _esc(client.id) + '">' +
        '<div class="device-info">' +
          '<span class="device-id" title="' + _esc(client.id) + '">' + _esc(idShort) + '</span>' +
          name +
          (laneCount ? '<span class="muted text-sm">' + _esc(laneCount) + '</span>' : '') +
        '</div>' +
        '<div class="device-meta">' +
          '<button class="btn btn-sm btn-primary btn-motor-ctrl" data-id="' + _esc(client.id) + '"' +
            ' data-name="' + _esc(client.playerName || client.id) + '">⚙️ Control</button>' +
          '<button class="btn-kick" data-id="' + _esc(client.id) + '"' +
            ' aria-label="Kick client ' + _esc(idShort) + '">Kick</button>' +
        '</div>' +
      '</div>'
    );
  }

  // ── Motor Control ────────────────────────────────────────────────────────────

  var _motorCtrlId   = null;
  var _motorCtrlName = '';
  var _motorPollTimer = null;
  var _jogSteps = 1;

  function _motorApiBase() {
    return '/api/clients/' + encodeURIComponent(_motorCtrlId);
  }

  function _motorGet(path) {
    return fetch(_motorApiBase() + path).then(function (r) { return r.json(); });
  }

  function _motorPost(path, body) {
    return fetch(_motorApiBase() + path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    }).then(function (r) { return r.json(); });
  }

  function _motorDelete(path) {
    return fetch(_motorApiBase() + path, { method: 'DELETE' }).then(function (r) { return r.json(); });
  }

  function _renderLaneGrid(lanes) {
    var grid = _el('motor-lane-grid');
    if (!grid) return;
    if (!lanes || !lanes.length) {
      grid.innerHTML = '<p class="empty-msg">No lane data.</p>';
      return;
    }
    grid.innerHTML = lanes.map(function (lane) {
      var pct = Math.round((lane.position || 0) * 100);
      var calBadge = lane.calibrated
        ? '<span class="badge badge-ok">cal ✓</span>'
        : '<span class="badge badge-warn">uncal</span>';
      var homedBadge = lane.homed
        ? '<span class="badge badge-ok">homed</span>'
        : '<span class="badge badge-warn">unhomed</span>';
      var movingBadge = lane.moving ? '<span class="badge badge-info">moving</span>' : '';
      return (
        '<div class="lane-card">' +
          '<div class="lane-title">Lane ' + lane.id + '</div>' +
          '<div class="lane-badges">' + calBadge + homedBadge + movingBadge + '</div>' +
          '<div class="lane-pos-bar"><div class="lane-pos-fill" style="width:' + pct + '%"></div></div>' +
          '<div class="lane-pos-label">' + pct + '%</div>' +
        '</div>'
      );
    }).join('');
  }

  function _refreshMotorStatus(silent) {
    if (!_motorCtrlId) return;
    _motorGet('/motor/status')
      .then(function (data) {
        _renderLaneGrid(data.lanes);
        // Update lane selectors
        var sel1 = _el('sel-jog-lane');
        var sel2 = _el('sel-calib-lane');
        [sel1, sel2].forEach(function (sel) {
          if (!sel || !data.lanes) return;
          var prev = sel.value;
          sel.innerHTML = data.lanes.map(function (l) {
            return '<option value="' + l.id + '">' + l.id + '</option>';
          }).join('');
          if (prev !== '') sel.value = prev;
        });
        if (!silent) {
          var msg = _el('motor-status-msg');
          if (msg) msg.textContent = 'Updated ' + _ts();
        }
      })
      .catch(function (e) { console.warn('[MotorCtrl] Status error:', e.message); });
  }

  function _showMotorMsg(elId, text) {
    var el = _el(elId);
    if (el) el.textContent = text;
  }

  function _initMotorJogButtons() {
    // Step size selector
    document.querySelectorAll('.jog-size-btn').forEach(function (btn) {
      btn.addEventListener('click', function () {
        _jogSteps = parseInt(btn.dataset.steps, 10);
        document.querySelectorAll('.jog-size-btn').forEach(function (b) { b.classList.remove('active'); });
        btn.classList.add('active');
      });
    });

    var btnBack = _el('btn-jog-back');
    var btnFwd  = _el('btn-jog-fwd');
    if (btnBack) btnBack.addEventListener('click', function () {
      var lane = parseInt((_el('sel-jog-lane') || {}).value || '0', 10);
      _motorPost('/motor/jog', { lane: lane, steps: -_jogSteps })
        .then(function () { _showMotorMsg('motor-status-msg', 'Jogged ◀'); })
        .catch(function (e) { _showMotorMsg('motor-status-msg', '❌ ' + e.message); });
    });
    if (btnFwd) btnFwd.addEventListener('click', function () {
      var lane = parseInt((_el('sel-jog-lane') || {}).value || '0', 10);
      _motorPost('/motor/jog', { lane: lane, steps: _jogSteps })
        .then(function () { _showMotorMsg('motor-status-msg', 'Jogged ▶'); })
        .catch(function (e) { _showMotorMsg('motor-status-msg', '❌ ' + e.message); });
    });

    var btnHome = _el('btn-motor-home-all');
    if (btnHome) btnHome.addEventListener('click', function () {
      _motorPost('/motor/home', {})
        .then(function () { _showMotorMsg('motor-status-msg', 'Homing all lanes…'); })
        .catch(function (e) { _showMotorMsg('motor-status-msg', '❌ ' + e.message); });
    });
  }

  function _initCalibButtons() {
    function _lane() { return parseInt((_el('sel-calib-lane') || {}).value || '0', 10); }

    var calibActions = [
      { id: 'btn-calib-start',     path: '/motor/calibrate/start',     msg: 'Calibration started — jog to track start position.' },
      { id: 'btn-calib-set-start', path: '/motor/calibrate/set_start', msg: 'Start position captured.' },
      { id: 'btn-calib-set-end',   path: '/motor/calibrate/set_end',   msg: 'End position captured.' },
      { id: 'btn-calib-finish',    path: '/motor/calibrate/finish',    msg: '✅ Calibration saved.' },
      { id: 'btn-calib-reset',     path: '/motor/calibrate/reset',     msg: '🔄 Calibration reset.' },
    ];

    calibActions.forEach(function (action) {
      var btn = _el(action.id);
      if (!btn) return;
      btn.addEventListener('click', function () {
        _motorPost(action.path, { lane: _lane() })
          .then(function (res) {
            if (res.error) {
              _showMotorMsg('calib-status-msg', '❌ ' + _esc(res.error));
            } else {
              _showMotorMsg('calib-status-msg', action.msg);
              _refreshMotorStatus(true);
            }
          })
          .catch(function (e) { _showMotorMsg('calib-status-msg', '❌ ' + e.message); });
      });
    });
  }

  function _refreshBtStatus() {
    if (!_motorCtrlId) return;
    _motorGet('/bt/status')
      .then(function (data) {
        var label       = _el('bt-status-label');
        var btnConnect  = _el('btn-bt-connect');
        var btnDisconn  = _el('btn-bt-disconnect');
        var btnUnpair   = _el('btn-bt-unpair');
        if (label) {
          if (data.connected) {
            label.textContent = '🔵 Connected: ' + _esc(data.pairedDevice || data.pairedAddress);
          } else if (data.pairedAddress) {
            label.textContent = '⚪ Paired (disconnected): ' + _esc(data.pairedDevice || data.pairedAddress);
          } else {
            label.textContent = 'No device paired.';
          }
        }
        // Connect: show only when paired but not connected
        if (btnConnect)  btnConnect.style.display  = (data.pairedAddress && !data.connected) ? '' : 'none';
        // Disconnect: show only when connected
        if (btnDisconn)  btnDisconn.style.display  = data.connected ? '' : 'none';
        // Unpair: show when any paired address exists
        if (btnUnpair)   btnUnpair.style.display   = data.pairedAddress ? '' : 'none';
      })
      .catch(function () {});
  }

  function _initBtButtons() {
    var btnScan    = _el('btn-bt-scan');
    var btnConnect = _el('btn-bt-connect');
    var btnDisconn = _el('btn-bt-disconnect');
    var btnUnpair  = _el('btn-bt-unpair');

    if (btnConnect) btnConnect.addEventListener('click', function () {
      btnConnect.disabled = true;
      _showMotorMsg('bt-status-msg', '🔗 Connecting…');
      _motorPost('/bt/connect', {})
        .then(function () { _showMotorMsg('bt-status-msg', '✅ Connect initiated.'); setTimeout(_refreshBtStatus, 3000); })
        .catch(function (e) { _showMotorMsg('bt-status-msg', '❌ ' + e.message); })
        .finally(function () { btnConnect.disabled = false; });
    });

    if (btnDisconn) btnDisconn.addEventListener('click', function () {
      btnDisconn.disabled = true;
      _motorPost('/bt/disconnect', {})
        .then(function () { _showMotorMsg('bt-status-msg', 'Disconnected.'); _refreshBtStatus(); })
        .catch(function (e) { _showMotorMsg('bt-status-msg', '❌ ' + e.message); })
        .finally(function () { btnDisconn.disabled = false; });
    });

    if (btnScan) btnScan.addEventListener('click', function () {
      _showMotorMsg('bt-status-msg', '🔍 Scanning (~10 s)…');
      btnScan.disabled = true;
      _motorGet('/bt/scan')
        .then(function (data) {
          _showMotorMsg('bt-status-msg', 'Found ' + (data.count || 0) + ' device(s).');
          var list = _el('bt-device-list');
          if (!list) return;
          if (!data.devices || !data.devices.length) {
            list.innerHTML = '<p class="empty-msg muted">No A2DP sinks found.</p>';
            return;
          }
          list.innerHTML = data.devices.map(function (d) {
            return (
              '<div class="device-row">' +
                '<div class="device-info">' +
                  '<span class="device-name">' + _esc(d.name || 'Unknown') + '</span>' +
                  '<span class="muted text-sm">' + _esc(d.address) + ' (RSSI ' + d.rssi + ')</span>' +
                '</div>' +
                '<div class="device-meta">' +
                  '<button class="btn btn-sm btn-primary btn-bt-pair" data-address="' + _esc(d.address) + '">Pair</button>' +
                '</div>' +
              '</div>'
            );
          }).join('');
          list.querySelectorAll('.btn-bt-pair').forEach(function (btn) {
            btn.addEventListener('click', function () {
              btn.disabled = true;
              _showMotorMsg('bt-status-msg', 'Pairing…');
              _motorPost('/bt/pair', { address: btn.dataset.address })
                .then(function (res) {
                  if (res.error) {
                    _showMotorMsg('bt-status-msg', '❌ ' + _esc(res.error));
                  } else {
                    _showMotorMsg('bt-status-msg', '✅ Pairing initiated.');
                    setTimeout(_refreshBtStatus, 3000);
                  }
                  btn.disabled = false;
                })
                .catch(function (e) { _showMotorMsg('bt-status-msg', '❌ ' + e.message); btn.disabled = false; });
            });
          });
        })
        .catch(function (e) { _showMotorMsg('bt-status-msg', '❌ Scan failed: ' + e.message); })
        .finally(function () { btnScan.disabled = false; });
    });
, motorCount, motorColors) {
    _motorCtrlId   = clientId;
    _motorCtrlName = clientName;
    var panel = _el('card-motor-control');
    var label = _el('motor-ctrl-device-label');
    if (panel)  panel.style.display  = '';
    if (label)  label.textContent    = clientName;

    _renderTrackColors(motorCount || 0, motorColors || []);
    var btnPlay  = _el('btn-bt-play');
    var selSound = _el('bt-sound-event');
    if (btnPlay) btnPlay.addEventListener('click', function () {
      var ev = selSound ? selSound.value : 'winner';
      btnPlay.disabled = true;
      _motorPost('/bt/play', { event: ev })
        .then(function () { _showMotorMsg('bt-status-msg', '🔊 Playing: ' + _esc(ev.replace(/_/g, ' '))); })
        .catch(function (e) { _showMotorMsg('bt-status-msg', '❌ ' + e.message); })
        .finally(function () { btnPlay.disabled = false; });
    });
  }

  function _openMotorControl(clientId, clientName, motorCount, motorColors) {
    _motorCtrlId   = clientId;
    _motorCtrlName = clientName;
  // ── Track Colors ──────────────────────────────────────────────────────────────

  function _colorSelectHtml(laneIndex, currentColorIndex) {
    var opts = PLAYER_COLORS.map(function (c) {
      var selected = (c.index === currentColorIndex) ? ' selected' : '';
      return '<option value="' + c.index + '"' + selected + '>Track ' + (c.index + 1) + ' — ' + _esc(c.name) + '</option>';
    }).join('');
    return '<select class="input-sm track-color-select" data-lane="' + laneIndex + '">' + opts + '</select>';
  }

  function _renderTrackColors(motorCount, motorColors) {
    var container = _el('motor-track-colors');
    if (!container) return;
    if (!motorCount || motorCount === 0) {
      container.innerHTML = '<p class="empty-msg muted">No lane data — refresh after connecting.</p>';
      return;
    }
    var rows = '';
    for (var i = 0; i < motorCount; i++) {
      var colorIdx = (motorColors && motorColors[i] != null) ? motorColors[i] : i;
      var hex = (PLAYER_COLORS[colorIdx] || PLAYER_COLORS[0]).hex;
      rows +=
        '<div class="track-color-row">' +
          '<span class="track-color-label">Lane ' + i + '</span>' +
          '<span class="track-color-swatch" id="swatch-lane-' + i + '" style="background:' + _esc(hex) + '"></span>' +
          _colorSelectHtml(i, colorIdx) +
        '</div>';
    }
    container.innerHTML = rows;

    // Live swatch update on change
    container.querySelectorAll('.track-color-select').forEach(function (sel) {
      sel.addEventListener('change', function () {
        var idx = parseInt(sel.value, 10);
        var swatch = _el('swatch-lane-' + sel.dataset.lane);
        if (swatch) swatch.style.background = (PLAYER_COLORS[idx] || PLAYER_COLORS[0]).hex;
      });
    });
  }

  function _initTrackColorsSave() {
    var btn = _el('btn-track-colors-save');
    var status = _el('track-colors-status');
    if (!btn) return;
    btn.addEventListener('click', function () {
      if (!_motorCtrlId) return;
      var selects = document.querySelectorAll('.track-color-select');
      var colors = [];
      selects.forEach(function (sel) {
        colors[parseInt(sel.dataset.lane, 10)] = parseInt(sel.value, 10);
      });
      btn.disabled = true;
      if (status) status.textContent = 'Saving…';
      fetch('/api/clients/' + encodeURIComponent(_motorCtrlId) + '/motor/colors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ colors: colors }),
      })
        .then(function (r) { return r.json(); })
        .then(function (data) {
          if (status) status.textContent = data.error ? ('❌ ' + _esc(data.error)) : '✅ Colors saved.';
        })
        .catch(function (e) {
          if (status) status.textContent = '❌ ' + _esc(e.message);
        })
        .finally(function () { btn.disabled = false; });
    });
  }

    var panel = _el('card-motor-control');
    var label = _el('motor-ctrl-device-label');
    if (panel)  panel.style.display  = '';
    if (label)  label.textContent    = clientName;

    _renderTrackColors(motorCount || 0, motorColors || []);
    _initTrackColorsSave();
    _refreshMotorStatus(false);
    _refreshBtStatus();

    clearInterval(_motorPollTimer);
    _motorPollTimer = setInterval(function () { _refreshMotorStatus(true); _refreshBtStatus(); }, 2000);
  }

  function _closeMotorControl() {
    _motorCtrlId = null;
    clearInterval(_motorPollTimer);
    _motorPollTimer = null;
    var panel = _el('card-motor-control');
    if (panel) panel.style.display = 'none';
  }
var colors = [];
        try { colors = JSON.parse(btn.dataset.motorColors || '[]'); } catch (_) {}
        _openMotorControl(
          btn.dataset.id,
          btn.dataset.name,
          parseInt(btn.dataset.motorCount || '0', 10),
          colors
        
  // ── Track Colors ──────────────────────────────────────────────────────────────

  function _colorSelectHtml(laneIndex, currentColorIndex) {
    var opts = PLAYER_COLORS.map(function (c) {
      var selected = (c.index === currentColorIndex) ? ' selected' : '';
      return '<option value="' + c.index + '"' + selected + '>Track ' + (c.index + 1) + ' — ' + _esc(c.name) + '</option>';
    }).join('');
    return '<select class="input-sm track-color-select" data-lane="' + laneIndex + '">' + opts + '</select>';
  }

  function _renderTrackColors(motorCount, motorColors) {
    var container = _el('motor-track-colors');
    if (!container) return;
    if (!motorCount || motorCount === 0) {
      container.innerHTML = '<p class="empty-msg muted">No lane data — refresh after connecting.</p>';
      return;
    }
    var rows = '';
    for (var i = 0; i < motorCount; i++) {
      var colorIdx = (motorColors && motorColors[i] != null) ? motorColors[i] : i;
      var hex = (PLAYER_COLORS[colorIdx] || PLAYER_COLORS[0]).hex;
      rows +=
        '<div class="track-color-row">' +
          '<span class="track-color-label">Lane ' + i + '</span>' +
          '<span class="track-color-swatch" id="swatch-lane-' + i + '" style="background:' + _esc(hex) + '"></span>' +
          _colorSelectHtml(i, colorIdx) +
        '</div>';
    }
    container.innerHTML = rows;

    // Live swatch update on change
    container.querySelectorAll('.track-color-select').forEach(function (sel) {
      sel.addEventListener('change', function () {
        var idx = parseInt(sel.value, 10);
        var swatch = _el('swatch-lane-' + sel.dataset.lane);
        if (swatch) swatch.style.background = (PLAYER_COLORS[idx] || PLAYER_COLORS[0]).hex;
      });
    });
  }

  function _initTrackColorsSave() {
    var btn = _el('btn-track-colors-save');
    var status = _el('track-colors-status');
    if (!btn) return;
    btn.addEventListener('click', function () {
      if (!_motorCtrlId) return;
      var selects = document.querySelectorAll('.track-color-select');
      var colors = [];
      selects.forEach(function (sel) {
        colors[parseInt(sel.dataset.lane, 10)] = parseInt(sel.value, 10);
      });
      btn.disabled = true;
      if (status) status.textContent = 'Saving…';
      fetch('/api/clients/' + encodeURIComponent(_motorCtrlId) + '/motor/colors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ colors: colors }),
      })
        .then(function (r) { return r.json(); })
        .then(function (data) {
          if (status) status.textContent = data.error ? ('❌ ' + _esc(data.error)) : '✅ Colors saved.';
        })
        .catch(function (e) {
          if (status) status.textContent = '❌ ' + _esc(e.message);
        })
        .finally(function () { btn.disabled = false; });
    });
  }

  function _initMotorControlPanel() {
    var btnClose = _el('btn-motor-ctrl-close');
    if (btnClose) btnClose.addEventListener('click', _closeMotorControl);
    _initMotorJogButtons();
    _initCalibButtons();
    _initBtButtons();
    _initTrackColorsSave();
  }

  // ── Render (override motor list) ─────────────────────────────────────────────

  function _renderMotorList(clients) {
    var el = _el('motor-list');
    if (!el) return;
    if (!clients || clients.length === 0) {
      el.innerHTML = '<p class="empty-msg">No motor clients connected.</p>';
      return;
    }
    el.innerHTML = clients.map(_renderMotorDeviceRow).join('');
    el.querySelectorAll('.btn-motor-ctrl').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var colors = [];
        try { colors = JSON.parse(btn.dataset.motorColors || '[]'); } catch (_) {}
        _openMotorControl(
          btn.dataset.id,
          btn.dataset.name,
          parseInt(btn.dataset.motorCount || '0', 10),
          colors
        );
      });
    });
    el.querySelectorAll('.btn-kick').forEach(function (btn) {
      btn.addEventListener('click', function () {
        if (!window.confirm('Kick this client? Their connection will be closed.')) return;
        _kickClient(btn.dataset.id)
          .then(function () { _refresh(); })
          .catch(function (e) { console.error('[Devices] Kick failed:', e); });
      });
    });
  }

  function _configureSensor(sensorIp, serverIp, serverPort, playerName) {
    return fetch('/api/sensors/configure', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ sensorIp: sensorIp, serverIp: serverIp, serverPort: serverPort, playerName: playerName }),
    }).then(function (res) { return res.json().then(function (body) { return { ok: res.ok, body: body }; }); });
  }

  function _initSensorConfigForm() {
    var form       = _el('sensor-config-form');
    var btnSubmit  = _el('btn-sensor-config');
    var statusEl   = _el('sensor-config-status');
    var inputServerIp   = _el('input-server-ip');
    var inputServerPort = _el('input-server-port');

    if (!form) return;

    // Pre-fill server IP and port from the current page origin.
    if (inputServerIp && !inputServerIp.value) {
      inputServerIp.value = window.location.hostname;
    }
    if (inputServerPort && !inputServerPort.value) {
      inputServerPort.value = window.location.port || '3000';
    }

    form.addEventListener('submit', function (e) {
      e.preventDefault();

      var sensorIp   = (_el('input-sensor-ip')   || {}).value || '';
      var serverIp   = (inputServerIp             || {}).value || '';
      var serverPort = (inputServerPort           || {}).value || '';
      var playerName = (_el('input-player-name')  || {}).value || '';

      if (!sensorIp.trim()) {
        if (statusEl) statusEl.textContent = 'Enter the sensor IP first.';
        return;
      }

      if (btnSubmit) btnSubmit.disabled = true;
      if (statusEl) statusEl.textContent = 'Sending…';

      _configureSensor(sensorIp.trim(), serverIp.trim(), serverPort.trim(), playerName.trim())
        .then(function (result) {
          if (result.ok) {
            if (statusEl) statusEl.textContent = '✅ Config sent — sensor is rebooting.';
          } else {
            var msg = (result.body && result.body.error) ? result.body.error : 'Unknown error';
            if (statusEl) statusEl.textContent = '❌ ' + _esc(msg);
          }
        })
        .catch(function (e) {
          if (statusEl) statusEl.textContent = '❌ Request failed: ' + _esc(e.message);
        })
        .finally(function () {
          if (btnSubmit) btnSubmit.disabled = false;
        });
    });
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

  // ── mDNS Info ──────────────────────────────────────────────────────────────

  function _fetchMdnsInfo() {
    fetch('/api/health')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        var el = _el('mdns-info');
        if (!el || !data.mdns) return;
        el.textContent = '📡 mDNS: advertising as ' + _esc(data.mdns.serviceType) +
          ' — sensors auto-discover this server on the LAN (' + _esc(data.mdns.hostname) + ')';
      })
      .catch(function () { /* ignore — non-critical */ });
  }

  // ── Init ────────────────────────────────────────────────────────────────────

  var btnRefresh = _el('btn-refresh');
  if (btnRefresh) btnRefresh.addEventListener('click', _refresh);

  _initSensorConfigForm();
  _initMotorControlPanel();
  _fetchMdnsInfo();

  // Pause polling when the tab is hidden to save server resources
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      clearInterval(_pollTimer);
      _pollTimer = null;
      clearInterval(_motorPollTimer);
      _motorPollTimer = null;
    } else {
      _startPolling();
      if (_motorCtrlId) {
        _motorPollTimer = setInterval(function () { _refreshMotorStatus(true); _refreshBtStatus(); }, 2000);
      }
    }
  });

  _startPolling();
}());
