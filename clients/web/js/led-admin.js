'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.LED — LED configuration and management module.
 *
 * Public API:
 *   Derby.LED.init()                     — initialize LED admin panel
 *   Derby.LED.updateDeviceList(devices)  — update device cards from state
 *   Derby.LED.selectDevice(deviceId)     — select device for configuration
 */
Derby.LED = (function () {
  var _selectedDevice = null;
  var _devices = [];
  var _playerColors = [];
  var _simulator = null;

  function _el(id) { return document.getElementById(id); }
  function _esc(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  // ── Initialization ──────────────────────────────────────────────────────────

  function init() {
    _loadPlayerColors();
    _initSimulator();
    _attachEventListeners();
    _fetchCurrentConfig();
  }

  function _initSimulator() {
    if (!Derby.LEDSimulator) {
      console.warn('[LED] LEDSimulator not loaded');
      return;
    }
    // LEDSimulator is a singleton, not a constructor
    _simulator = Derby.LEDSimulator;
    _simulator.init('led-simulator');
    _simulator.setConfig({ ledCount: 60, topology: 'strip' });
    _simulator.start();
    
    // Start with a default rainbow effect
    setTimeout(function () {
      if (Derby.LEDEffects) {
        _simulator.playEffect('rainbow', { speed: 1000 });
      }
    }, 100);
    
    console.log('[LED] Simulator initialized');
  }

  function _loadPlayerColors() {
    fetch('/assets/themes/shared/player-colors.json')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        _playerColors = data.colors;
        _populateColorPicker();
      })
      .catch(function (err) {
        console.error('[LED] Failed to load player colors:', err);
      });
  }

  function _attachEventListeners() {
    var saveBtn = _el('led-save-config');
    if (saveBtn) {
      saveBtn.addEventListener('click', _handleSaveConfig);
    }

    var testBtn = _el('led-test-effect');
    if (testBtn) {
      testBtn.addEventListener('click', _handleTestEffect);
    }

    var topologySelect = _el('led-topology');
    if (topologySelect) {
      topologySelect.addEventListener('change', _handleTopologyChange);
    }

    var syncAllBtn = _el('led-sync-all');
    if (syncAllBtn) {
      syncAllBtn.addEventListener('click', _handleSyncAll);
    }

    // Effect preview controls
    var effectSelect = _el('led-effect-select');
    if (effectSelect) {
      effectSelect.addEventListener('change', _handleEffectChange);
    }

    var colorInput = _el('led-color');
    if (colorInput) {
      colorInput.addEventListener('input', _handleEffectChange);
    }

    var speedSlider = _el('led-speed');
    if (speedSlider) {
      speedSlider.addEventListener('input', function () {
        var valueEl = _el('led-speed-value');
        if (valueEl) valueEl.textContent = speedSlider.value + 'ms';
        _handleEffectChange();
      });
    }

    var brightnessSlider = _el('led-brightness');
    if (brightnessSlider) {
      brightnessSlider.addEventListener('input', function () {
        var valueEl = _el('led-brightness-value');
        var percentValue = parseInt(brightnessSlider.value, 10);
        if (valueEl) valueEl.textContent = percentValue + '%';
        if (_simulator) {
          // Convert 0-100 to 0-255
          var byteValue = Math.round((percentValue / 100) * 255);
          _simulator.setBrightness(byteValue);
        }
      });
    }

    var ledCountInput = _el('led-count');
    if (ledCountInput) {
      ledCountInput.addEventListener('input', function () {
        if (_simulator) {
          var count = parseInt(ledCountInput.value, 10) || 60;
          var topology = _el('led-topology').value || 'strip';
          _simulator.setConfig({ ledCount: count, topology: topology });
        }
      });
    }
  }

  function _fetchCurrentConfig() {
    fetch('/api/leds/config')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        console.log('[LED] Current config:', data);
      })
      .catch(function (err) {
        console.error('[LED] Failed to fetch config:', err);
      });
  }

  // ── Device List Management ──────────────────────────────────────────────────

  function updateDeviceList(devices) {
    _devices = devices.filter(function (d) {
      return d.type === 'sensor' || d.type === 'motor';
    });

    var listEl = _el('led-device-list');
    if (!listEl) return;

    if (_devices.length === 0) {
      listEl.innerHTML = '<p class="empty-msg">No LED-capable devices connected.</p>';
      return;
    }

    listEl.innerHTML = '';
    _devices.forEach(function (device) {
      var card = _createDeviceCard(device);
      listEl.appendChild(card);
    });
  }

  function _createDeviceCard(device) {
    var card = document.createElement('div');
    card.className = 'led-device-card';
    card.dataset.deviceId = device.id;

    var typeIcon = device.type === 'sensor' ? '📡' : '🎛️';
    var detectedLEDs = device.ledCount || 0;
    var connected = device.connected ? 'connected' : 'disconnected';

    card.innerHTML = 
      '<div class="led-device-header">' +
        '<span class="led-device-icon">' + typeIcon + '</span>' +
        '<div class="led-device-info">' +
          '<div class="led-device-name">' + _esc(device.name || 'Unknown') + '</div>' +
          '<div class="led-device-meta">' + device.type + ' • ' + detectedLEDs + ' LEDs</div>' +
        '</div>' +
        '<span class="led-status-badge led-status-' + connected + '"></span>' +
      '</div>' +
      '<canvas class="led-mini-preview" width="100" height="20"></canvas>';

    card.addEventListener('click', function () {
      selectDevice(device.id);
    });

    // Mini LED preview — the simulator is a singleton so we can't create
    // multiple instances.  Skipping mini-preview on device cards for now.
    // TODO: refactor LEDSimulator into a constructor to support multiple canvases.

    return card;
  }

  function selectDevice(deviceId) {
    // Clear previous selection
    document.querySelectorAll('.led-device-card').forEach(function (card) {
      card.classList.remove('led-device-selected');
    });

    // Highlight selected
    var card = document.querySelector('[data-device-id="' + deviceId + '"]');
    if (card) {
      card.classList.add('led-device-selected');
    }

    _selectedDevice = _devices.find(function (d) { return d.id === deviceId; });
    if (_selectedDevice) {
      _populateConfigForm(_selectedDevice);
    }
  }

  // ── Configuration Form ──────────────────────────────────────────────────────

  function _populateConfigForm(device) {
    var ledCount = _el('led-count');
    var pin = _el('led-pin');
    var topology = _el('led-topology');
    var brightness = _el('led-brightness');

    if (ledCount) ledCount.value = device.ledCount || 50;
    if (pin) pin.value = device.ledPin || 'D4';
    if (topology) topology.value = device.ledTopology || 'strip';
    if (brightness) brightness.value = device.ledBrightness || 80;

    _handleTopologyChange();
    _updateSimulator();
  }

  function _handleTopologyChange() {
    var topology = _el('led-topology');
    var matrixDims = _el('led-matrix-dims');

    if (!topology || !matrixDims) return;

    if (topology.value.startsWith('matrix')) {
      matrixDims.classList.remove('hidden');
    } else {
      matrixDims.classList.add('hidden');
    }

    _updateSimulator();
  }

  function _updateSimulator() {
    if (!_simulator) return;

    var ledCount = parseInt(_el('led-count').value, 10) || 60;
    var topology = _el('led-topology').value || 'strip';
    var brightnessPercent = parseInt(_el('led-brightness').value, 10) || 80;
    var brightnessByte = Math.round((brightnessPercent / 100) * 255);

    _simulator.setConfig({ ledCount: ledCount, topology: topology });
    _simulator.setBrightness(brightnessByte);
    _handleEffectChange();
  }

  function _handleEffectChange() {
    if (!_simulator || typeof Derby.LEDEffects === 'undefined') return;

    var effectName = _el('led-effect-select').value;
    if (!effectName) return;

    var colorHex = _el('led-color').value || '#FFFFFF';
    var speed = parseInt(_el('led-speed').value, 10) || 50;

    _simulator.playEffect(effectName, { color: colorHex, speed: speed });
  }

  function _populateColorPicker() {
    var select = _el('led-color');
    if (!select) return;

    select.innerHTML = '';
    _playerColors.forEach(function (color) {
      var option = document.createElement('option');
      option.value = color.hex;
      option.textContent = color.name;
      option.style.color = color.hex;
      select.appendChild(option);
    });
  }

  // ── Save Configuration ──────────────────────────────────────────────────────

  function _handleSaveConfig() {
    if (!_selectedDevice) {
      _showError('No device selected');
      return;
    }

    var config = {
      ledCount: parseInt(_el('led-count').value, 10),
      pin: _el('led-pin').value,
      topology: _el('led-topology').value,
      brightness: parseInt(_el('led-brightness').value, 10),
    };

    if (config.topology.startsWith('matrix')) {
      config.matrixRows = parseInt(_el('led-matrix-rows').value, 10);
      config.matrixCols = parseInt(_el('led-matrix-cols').value, 10);
    }

    var deviceType = _selectedDevice.type;
    fetch('/api/leds/config/' + deviceType, {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) {
          _showError(data.error);
        } else {
          _showSuccess('Configuration saved and broadcast to devices');
        }
      })
      .catch(function (err) {
        _showError('Save failed: ' + err.message);
      });
  }

  // ── Test Effect ─────────────────────────────────────────────────────────────

  function _handleTestEffect() {
    if (!_selectedDevice) {
      _showError('No device selected');
      return;
    }

    var effectName = _el('led-effect-select').value;
    if (!effectName) {
      _showError('No effect selected');
      return;
    }

    var testBtn = _el('led-test-effect');
    testBtn.disabled = true;
    testBtn.textContent = 'Testing...';

    var payload = {
      effectName: effectName,
      params: _getEffectParams(effectName),
    };

    fetch('/api/leds/test', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        deviceId: _selectedDevice.id,
        effect: payload,
      }),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) {
          _showError(data.error);
        } else {
          _showSuccess('Test effect sent to device');
        }
      })
      .catch(function (err) {
        _showError('Test failed: ' + err.message);
      })
      .finally(function () {
        setTimeout(function () {
          testBtn.disabled = false;
          testBtn.textContent = 'Test on Device';
        }, 1000);
      });
  }

  function _getEffectParams(effectName) {
    var params = {};
    var colorEl = _el('led-color');
    var speedEl = _el('led-speed');

    if (colorEl) params.color = colorEl.value;
    if (speedEl) params.speed = parseInt(speedEl.value, 10);

    return params;
  }

  // ── Sync All ────────────────────────────────────────────────────────────────

  function _handleSyncAll() {
    fetch('/api/leds/config/sync-all', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        _showSuccess('Configuration synced to all devices');
      })
      .catch(function (err) {
        _showError('Sync failed: ' + err.message);
      });
  }

  // ── UI Helpers ──────────────────────────────────────────────────────────────

  function _showError(msg) {
    var errorEl = _el('led-error');
    if (!errorEl) return;
    errorEl.textContent = msg;
    errorEl.classList.remove('hidden');
    setTimeout(function () { errorEl.classList.add('hidden'); }, 5000);
  }

  function _showSuccess(msg) {
    var successEl = _el('led-success');
    if (!successEl) return;
    successEl.textContent = msg;
    successEl.classList.remove('hidden');
    setTimeout(function () { successEl.classList.add('hidden'); }, 3000);
  }

  function _esc(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  return {
    init: init,
    updateDeviceList: updateDeviceList,
    selectDevice: selectDevice,
  };
})();
