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
  var _allConfigs = {};   // { sensor: {...}, motor: {...} } cached from server
  var _manifest = null;  // populated from /api/leds/effects (single source of truth)

  function _el(id) { return document.getElementById(id); }
  function _esc(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }

  // ── Platform constants ──────────────────────────────────────────────────────

  // Chip-level capabilities used to drive the pin selector and LED count limits.
  // Keyed by normalised chipType string (uppercase) as reported by the device.
  var CHIP = {
    'ESP8266': { chipName: 'ESP8266', maxLeds: 300,  defaultPin: 2, defaultTopology: 'strip', pinGroup: 'esp8266',
                 pinHint: 'ESP8266: GPIO2 = UART1 (leaves Serial free); GPIO3 = DMA (uses RX pin).' },
    'ESP32':   { chipName: 'ESP32',   maxLeds: 1000, defaultPin: 4, defaultTopology: 'strip', pinGroup: 'esp32',
                 pinHint: 'ESP32: GPIO4 is the default strip pin (RMT ch0, hardware-timed). GPIO2 is reserved for the onboard status LED.' },
  };

  // Legacy per-type defaults used as fallback when chipType is not yet known.
  var PLATFORM = {
    sensor: CHIP['ESP8266'],
    motor:  CHIP['ESP32'],
  };

  /**
   * Return the CHIP entry for a device, using chipType when available and
   * falling back to the type-based default (sensor→ESP8266, motor→ESP32).
   */
  function _chipFor(device) {
    if (device && device.chipType) {
      var key = String(device.chipType).toUpperCase();
      if (CHIP[key]) return CHIP[key];
    }
    return PLATFORM[device && device.type] || CHIP['ESP8266'];
  }

  // ── Initialization ──────────────────────────────────────────────────────────

  function init() {
    _loadPlayerColors();
    _fetchEffectsManifest();
    _initSimulator();
    _attachEventListeners();
    _fetchAllConfigs();
  }

  function _initSimulator() {
    if (!Derby.LEDSimulator) {
      console.warn('[LED] LEDSimulator not loaded');
      return;
    }
    _simulator = Derby.LEDSimulator;
    _simulator.init('led-simulator');
    _simulator.setConfig({ ledCount: 60, topology: 'strip' });
    _simulator.start();
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
        // Re-render current effect now that we have real colors
        _handleEffectChange();
      })
      .catch(function (err) { console.error('[LED] Failed to load player colors:', err); });
  }

  function _fetchEffectsManifest() {
    fetch('/api/leds/effects')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        _manifest = data;
        // Repopulate dropdown if a device is already selected
        if (_selectedDevice) {
          _populateEffectDropdown(_selectedDevice.type);
        } else {
          _populateEffectDropdown('sensor'); // default: show strip effects
        }
      })
      .catch(function (err) { console.error('[LED] Failed to fetch effects manifest:', err); });
  }

  function _fetchAllConfigs() {
    fetch('/api/leds/config')
      .then(function (res) { return res.json(); })
      .then(function (data) {
        _allConfigs = data || {};
        console.log('[LED] Server configs loaded:', _allConfigs);
      })
      .catch(function (err) { console.error('[LED] Failed to fetch config:', err); });
  }

  function _attachEventListeners() {
    var saveBtn = _el('led-save-config');
    if (saveBtn) saveBtn.addEventListener('click', _handleSaveConfig);

    var testBtn = _el('led-test-effect');
    if (testBtn) testBtn.addEventListener('click', _handleTestEffect);

    var topologySelect = _el('led-topology');
    if (topologySelect) topologySelect.addEventListener('change', _handleTopologyChange);

    var effectSelect = _el('led-effect-select');
    if (effectSelect) effectSelect.addEventListener('change', _handleEffectChange);

    var colorInput = _el('led-color');
    if (colorInput) colorInput.addEventListener('input', _handleEffectChange);

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
        var pct = parseInt(brightnessSlider.value, 10);
        if (valueEl) valueEl.textContent = pct + '%';
        if (_simulator) _simulator.setBrightness(Math.round((pct / 100) * 255));
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

    // Preserve selection across re-renders
    var prevSelected = _selectedDevice ? _selectedDevice.id : null;
    listEl.innerHTML = '';
    _devices.forEach(function (device) {
      var card = _createDeviceCard(device);
      listEl.appendChild(card);
    });

    if (prevSelected) {
      var stillHere = _devices.find(function (d) { return d.id === prevSelected; });
      if (stillHere) {
        var card = listEl.querySelector('[data-device-id="' + prevSelected + '"]');
        if (card) card.classList.add('led-device-selected');
        _selectedDevice = stillHere;
        _populateEffectDropdown(_selectedDevice.type);
      }
    }

    _renderDeviceOverrides();
  }

  function _createDeviceCard(device) {
    var card = document.createElement('div');
    card.className = 'led-device-card';
    card.dataset.deviceId = device.id;

    var typeIcon   = device.type === 'sensor' ? '📡' : '🎛️';
    var connected  = device.connected ? 'connected' : 'disconnected';
    var detectedLEDs = device.ledCount || 0;

    var colorHex  = '#888888';
    var colorName = 'Unassigned';
    if (typeof device.colorIndex === 'number' && _playerColors[device.colorIndex]) {
      colorHex  = _playerColors[device.colorIndex].hex;
      colorName = _playerColors[device.colorIndex].name;
    }

    // Build capability pills for motor devices
    var capsHtml = '';
    if (device.type === 'motor') {
      var caps = device.capabilities || {};
      var pills = [];
      var motorCount = device.motorCount || 0;
      if (motorCount > 0) pills.push(motorCount + ' motor' + (motorCount !== 1 ? 's' : ''));
      if (caps.sound) pills.push('🔊 sound');
      if (caps.bt)    pills.push('🔵 BT');
      if (caps.buttons) pills.push('🔘 buttons');
      if (pills.length > 0) {
        capsHtml = '<div class="led-device-caps">' +
          pills.map(function (p) { return '<span class="led-cap-pill">' + _esc(p) + '</span>'; }).join('') +
          '</div>';
      }
    }

    card.innerHTML =
      '<div class="led-device-header">' +
        '<span class="led-device-color-swatch" style="background:' + _esc(colorHex) + '"></span>' +
        '<span class="led-device-icon">' + typeIcon + '</span>' +
        '<div class="led-device-info">' +
          '<div class="led-device-name">' + _esc(device.name || 'Unknown') + '</div>' +
          '<div class="led-device-meta">' +
            _esc(device.type) +
            (device.chipType ? ' (' + _esc(device.chipType) + ')' : '') +
            ' · ' + detectedLEDs + ' LEDs · ' + _esc(colorName) +
          '</div>' +
        '</div>' +
        '<span class="led-status-badge led-status-' + connected + '"></span>' +
      '</div>' +
      capsHtml +
      (device.chipId
        ? '<div class="led-device-color-picker">' +
            '<label>Device Color:</label>' +
            '<select class="led-device-color-select" data-chip-id="' + _esc(device.chipId) + '">' +
              _buildColorOptions(device.colorIndex) +
            '</select>' +
          '</div>'
        : '') +
      '<canvas class="led-mini-preview" width="100" height="20"></canvas>';

    card.addEventListener('click', function (e) {
      if (e.target.classList.contains('led-device-color-select')) return;
      selectDevice(device.id);
    });

    var colorSelect = card.querySelector('.led-device-color-select');
    if (colorSelect) {
      colorSelect.addEventListener('change', function (e) {
        e.stopPropagation();
        _handleDeviceColorChange(device.chipId, parseInt(this.value, 10));
      });
    }

    return card;
  }

  function _buildColorOptions(selectedIndex) {
    var html = '';
    _playerColors.forEach(function (color) {
      var selected = (color.index === selectedIndex) ? ' selected' : '';
      html += '<option value="' + color.index + '"' + selected + ' style="color:' + color.hex + '">' +
              _esc(color.name) + '</option>';
    });
    return html;
  }

  function _handleDeviceColorChange(chipId, colorIndex) {
    fetch('/api/leds/device-colors/' + encodeURIComponent(chipId), {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ colorIndex: colorIndex }),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) { _showError(data.error); }
        else { _showSuccess('Device color updated'); }
      })
      .catch(function (err) { _showError('Color update failed: ' + err.message); });
  }

  function selectDevice(deviceId) {
    document.querySelectorAll('.led-device-card').forEach(function (card) {
      card.classList.remove('led-device-selected');
    });

    var card = document.querySelector('[data-device-id="' + deviceId + '"]');
    if (card) card.classList.add('led-device-selected');

    _selectedDevice = _devices.find(function (d) { return d.id === deviceId; });
    if (_selectedDevice) {
      _populateConfigForm(_selectedDevice);
      _populateEffectDropdown(_selectedDevice.type);
    }
  }

  /**
   * Populate the effect dropdown from the shared manifest.
   * Shows all effects whose platforms array includes the given deviceType.
   * Replaces all options — no hardcoded lists anywhere else.
   */
  function _populateEffectDropdown(deviceType) {
    var select = _el('led-effect-select');
    if (!select) return;

    // Preserve current selection if still valid after repopulation
    var prevValue = select.value;

    select.innerHTML = '';
    var placeholder = document.createElement('option');
    placeholder.value = '';
    placeholder.textContent = '\u2014 Select Effect \u2014';
    select.appendChild(placeholder);

    if (!_manifest || !_manifest.effects) {
      // Manifest not loaded yet — show a loading note and bail
      var loading = document.createElement('option');
      loading.disabled = true;
      loading.textContent = 'Loading effects\u2026';
      select.appendChild(loading);
      return;
    }

    // Group by category so matrix effects appear below strip effects
    var categories = {};
    _manifest.effects.forEach(function (effect) {
      if (effect.platforms.indexOf(deviceType) === -1) return;
      var cat = effect.category || 'strip';
      if (!categories[cat]) categories[cat] = [];
      categories[cat].push(effect);
    });

    var catOrder = ['strip', 'matrix'];
    catOrder.forEach(function (cat) {
      if (!categories[cat] || categories[cat].length === 0) return;
      var group = document.createElement('optgroup');
      group.label = cat === 'matrix' ? 'Matrix' : 'Strip';
      categories[cat].forEach(function (effect) {
        var opt = document.createElement('option');
        opt.value       = effect.name;
        opt.textContent = effect.label;
        group.appendChild(opt);
      });
      select.appendChild(group);
    });

    // Restore selection if still available
    if (prevValue && select.querySelector('option[value="' + prevValue + '"]')) {
      select.value = prevValue;
    }
  }

  // ── Configuration Form ──────────────────────────────────────────────────────

  /**
   * Populate the config form from the stored server config for this device type.
   * Falls back to platform defaults if the server config hasn't loaded yet.
   */
  function _populateConfigForm(device) {
    var chip   = _chipFor(device);
    var stored = _allConfigs[device.type] || {};

    var gpioPin    = stored.gpioPin    || chip.defaultPin;
    var topology   = stored.topology   || chip.defaultTopology || 'strip';
    var brightness = stored.brightness !== undefined ? stored.brightness : 80;
    var matrixRows = stored.matrixRows || 8;
    var matrixCols = stored.matrixCols || 8;
    var mirrorH    = stored.mirrorH    || false;
    var mirrorV    = stored.mirrorV    || false;
    var defEffect  = stored.defaultEffect;

    // ── LED count ──
    var ledCountEl = _el('led-count');
    if (ledCountEl) {
      ledCountEl.max   = chip.maxLeds;
      ledCountEl.value = stored.ledCount || device.ledCount || (device.type === 'motor' ? 64 : 30);
    }
    var hintEl = _el('led-count-hint');
    if (hintEl) hintEl.textContent = chip.chipName + ' · max ' + chip.maxLeds + ' LEDs';

    // ── GPIO pin ── show only the relevant platform's optgroup, select the right pin
    _updatePinSelector(chip, gpioPin);

    // ── Topology ──
    var topologyEl = _el('led-topology');
    if (topologyEl) topologyEl.value = topology;

    // ── Matrix dims ──
    var rowsEl = _el('led-matrix-rows');
    var colsEl = _el('led-matrix-cols');
    if (rowsEl) rowsEl.value = matrixRows;
    if (colsEl) colsEl.value = matrixCols;
    var mirrorHEl = _el('led-mirror-h');
    var mirrorVEl = _el('led-mirror-v');
    if (mirrorHEl) mirrorHEl.checked = mirrorH;
    if (mirrorVEl) mirrorVEl.checked = mirrorV;

    // ── Brightness (0-100%) ──
    var brightnessEl = _el('led-brightness');
    if (brightnessEl) {
      brightnessEl.value = brightness;
      var bValEl = _el('led-brightness-value');
      if (bValEl) bValEl.textContent = brightness + '%';
    }

    // ── Default effect ──
    if (defEffect) {
      var effectEl = _el('led-effect-select');
      if (effectEl) effectEl.value = defEffect;
    }

    _handleTopologyChange();
    _updateSimulator();
  }

  /**
   * Show only the GPIO optgroup relevant to the chip and select the right pin.
   * @param {object} chip  - Entry from CHIP (has .pinGroup and .pinHint).
   * @param {number} pinValue - GPIO pin number to pre-select.
   */
  function _updatePinSelector(chip, pinValue) {
    var esp8266Group = _el('led-pin-esp8266');
    var esp32Group   = _el('led-pin-esp32');
    var hintEl       = _el('led-pin-hint');
    var pinEl        = _el('led-pin');

    if (!pinEl) return;

    var isEsp32 = chip.pinGroup === 'esp32';
    if (esp8266Group) esp8266Group.style.display = isEsp32 ? 'none' : '';
    if (esp32Group)   esp32Group.style.display   = isEsp32 ? '' : 'none';
    if (hintEl) hintEl.textContent = chip.pinHint || '';

    // Select the right option; if nothing matches, browser stays on first enabled option
    pinEl.value = String(pinValue);
  }

  function _handleTopologyChange() {
    var topology  = _el('led-topology');
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

    var ledCount       = parseInt((_el('led-count') || {}).value, 10) || 60;
    var topology       = (_el('led-topology') || {}).value || 'strip';
    var brightnessPercent = parseInt((_el('led-brightness') || {}).value, 10) || 80;

    var matrixRows = parseInt((_el('led-matrix-rows') || {}).value, 10) || 8;
    var matrixCols = parseInt((_el('led-matrix-cols') || {}).value, 10) || 8;
    _simulator.setConfig({ ledCount: ledCount, topology: topology, matrixRows: matrixRows, matrixCols: matrixCols });
    _simulator.setBrightness(Math.round((brightnessPercent / 100) * 255));
    _handleEffectChange();
  }

  function _handleEffectChange() {
    if (!_simulator || typeof Derby.LEDEffects === 'undefined') return;

    var effectName = (_el('led-effect-select') || {}).value;
    if (!effectName) return;

    var colorVal = (_el('led-color') || {}).value || '#FFFFFF';
    if (colorVal === 'device') {
      colorVal = (_selectedDevice && typeof _selectedDevice.colorIndex === 'number' && _playerColors[_selectedDevice.colorIndex])
        ? _playerColors[_selectedDevice.colorIndex].hex
        : (_playerColors.length > 0 ? _playerColors[0].hex : '#FF4400');
    }
    var speed = parseInt((_el('led-speed') || {}).value, 10) || 50;

    _simulator.playEffect(effectName, { color: colorVal, speed: speed });
  }

  function _populateColorPicker() {
    var select = _el('led-color');
    if (!select) return;

    select.innerHTML = '';
    var deviceOption = document.createElement('option');
    deviceOption.value       = 'device';
    deviceOption.textContent = '🎨 Device Color';
    deviceOption.selected    = true;
    select.appendChild(deviceOption);

    _playerColors.forEach(function (color) {
      var option      = document.createElement('option');
      option.value    = color.hex;
      option.textContent = color.name;
      option.style.color = color.hex;
      select.appendChild(option);
    });
  }

  // ── Save Configuration ──────────────────────────────────────────────────────

  function _handleSaveConfig() {
    if (!_selectedDevice) { _showError('No device selected'); return; }

    var config = {
      ledCount:      parseInt(_el('led-count').value, 10),
      gpioPin:       parseInt(_el('led-pin').value, 10),
      topology:      _el('led-topology').value,
      brightness:    parseInt(_el('led-brightness').value, 10),
      defaultEffect: _el('led-effect-select').value || 'solid',
    };

    // Validate ledCount and gpioPin against the chip's known hardware limits.
    var chip = _chipFor(_selectedDevice);
    if (isNaN(config.ledCount) || config.ledCount < 1 || config.ledCount > chip.maxLeds) {
      _showError('LED count must be 1–' + chip.maxLeds + ' for ' + chip.chipName);
      return;
    }
    if (chip.pinGroup === 'esp8266' && config.gpioPin !== 2 && config.gpioPin !== 3) {
      _showError(chip.chipName + ': only GPIO2 (UART1) and GPIO3 (DMA) are valid LED pins');
      return;
    }
    if (chip.pinGroup === 'esp32' && (isNaN(config.gpioPin) || config.gpioPin < 0 || config.gpioPin > 39)) {
      _showError(chip.chipName + ': LED pin must be GPIO 0–39');
      return;
    }

    if (config.topology.startsWith('matrix')) {
      config.matrixRows = parseInt(_el('led-matrix-rows').value, 10);
      config.matrixCols = parseInt(_el('led-matrix-cols').value, 10);
      config.mirrorH    = !!(_el('led-mirror-h') && _el('led-mirror-h').checked);
      config.mirrorV    = !!(_el('led-mirror-v') && _el('led-mirror-v').checked);
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
          // Update local cache so a re-select shows the new values
          _allConfigs[deviceType] = config;
          _showSuccess('Configuration saved and broadcast to devices');
        }
      })
      .catch(function (err) { _showError('Save failed: ' + err.message); });
  }

  // ── Test Effect ─────────────────────────────────────────────────────────────

  function _handleTestEffect() {
    if (!_selectedDevice) { _showError('No device selected'); return; }

    var effectName = _el('led-effect-select').value;
    if (!effectName) { _showError('No effect selected'); return; }

    var extraParams = {};
    if (effectName === 'text') {
      var text = window.prompt('Enter text to display on the device:');
      if (text === null) return; // User cancelled
      if (text.trim() === '') { _showError('Text cannot be empty'); return; }
      extraParams.text = text.trim();
    }

    var testBtn = _el('led-test-effect');
    testBtn.disabled    = true;
    testBtn.textContent = 'Testing…';

    var params = Object.assign(_getEffectParams(), extraParams);

    fetch('/api/leds/effects/test', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        deviceId:   _selectedDevice.id,
        effectName: effectName,
        params:     params,
      }),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) { _showError(data.error); }
        else { _showSuccess('Test effect sent to ' + (_selectedDevice.name || 'device')); }
      })
      .catch(function (err) { _showError('Test failed: ' + err.message); })
      .finally(function () {
        setTimeout(function () {
          testBtn.disabled    = false;
          testBtn.textContent = 'Test on Device';
        }, 1000);
      });
  }

  function _getEffectParams() {
    var params   = {};
    var colorEl  = _el('led-color');
    var speedEl  = _el('led-speed');

    if (colorEl) {
      var colorVal = colorEl.value;
      if (colorVal === 'device') {
        params.color = (_selectedDevice && typeof _selectedDevice.colorIndex === 'number' && _playerColors[_selectedDevice.colorIndex])
          ? _playerColors[_selectedDevice.colorIndex].hex
          : '#FFFFFF';
      } else {
        params.color = colorVal;
      }
    }
    if (speedEl) params.speed = parseInt(speedEl.value, 10);
    return params;
  }

  // ── Per-device Override Panel ───────────────────────────────────────────────

  /**
   * Re-render the per-device overrides section from the current _devices list.
   * Each device row shows effective config fetched from the server and provides
   * [Edit override] and [Reset to default] buttons.
   * XSS: all chipId / chipType strings go through textContent or _esc().
   */
  function _renderDeviceOverrides() {
    var section = _el('led-device-overrides-section');
    if (!section) return;

    var hardwareDevices = _devices.filter(function (d) {
      return (d.type === 'sensor' || d.type === 'motor') && d.chipId;
    });

    if (hardwareDevices.length === 0) {
      section.innerHTML = '<p class="empty-msg">No LED-capable devices connected.</p>';
      return;
    }

    section.innerHTML = '';
    hardwareDevices.forEach(function (device) {
      var rowEl = _createOverrideRow(device, null, false);
      section.appendChild(rowEl);

      // Fetch effective config for this device asynchronously
      fetch('/api/leds/config/' + encodeURIComponent(device.type) + '/' + encodeURIComponent(device.chipId))
        .then(function (res) { return res.json(); })
        .then(function (data) {
          var newRow = _createOverrideRow(device, data.config || null, !!data.hasOverride);
          section.replaceChild(newRow, rowEl);
        })
        .catch(function (err) {
          console.error('[LED] Failed to fetch device config for', device.chipId, err);
        });
    });
  }

  /**
   * Build a single device override row element.
   * @param {object} device        - device object from _devices
   * @param {object|null} config   - effective LED config from server (null while loading)
   * @param {boolean} hasOverride  - whether a per-device override is active
   */
  function _createOverrideRow(device, config, hasOverride) {
    var row = document.createElement('div');
    row.className = 'led-override-row';

    var header = document.createElement('div');
    header.className = 'led-override-header';

    var idSpan = document.createElement('span');
    idSpan.className = 'led-override-chipid';
    idSpan.textContent = device.chipId;

    var typeSpan = document.createElement('span');
    typeSpan.className = 'led-override-chiptype';
    typeSpan.textContent = device.chipType || device.type;

    var statusSpan = document.createElement('span');
    statusSpan.className = 'led-override-status ' + (hasOverride ? 'led-override-active' : 'led-override-default');
    statusSpan.textContent = hasOverride ? 'override' : 'default';

    var configSpan = document.createElement('span');
    configSpan.className = 'led-override-config';
    if (config) {
      configSpan.textContent = config.ledCount + ' LEDs · GPIO' + config.gpioPin + ' · ' + config.topology;
    } else {
      configSpan.textContent = 'Loading…';
    }

    var editBtn = document.createElement('button');
    editBtn.className = 'btn btn-sm';
    editBtn.textContent = 'Edit override';
    editBtn.addEventListener('click', function () {
      _openOverrideForm(device, config || {}, row);
    });

    var resetBtn = document.createElement('button');
    resetBtn.className = 'btn btn-sm btn-danger';
    resetBtn.textContent = 'Reset to default';
    resetBtn.disabled = !hasOverride;
    if (!hasOverride) resetBtn.title = 'No override active';
    resetBtn.addEventListener('click', function () {
      _handleResetDeviceOverride(device, row);
    });

    header.appendChild(idSpan);
    header.appendChild(typeSpan);
    header.appendChild(statusSpan);
    header.appendChild(configSpan);
    header.appendChild(editBtn);
    header.appendChild(resetBtn);
    row.appendChild(header);

    return row;
  }

  /**
   * Open an inline override form below the given row.
   * Submitting performs PUT /api/leds/config/:deviceType/:chipId and refreshes the row.
   */
  function _openOverrideForm(device, currentConfig, rowEl) {
    // Remove any existing form (toggle)
    var existing = rowEl.querySelector('.led-override-form');
    if (existing) { existing.remove(); return; }

    var chip = _chipFor(device);

    var form = document.createElement('div');
    form.className = 'led-override-form';

    // LED count
    var countLabel = document.createElement('label');
    countLabel.textContent = 'LED Count:';
    var countInput = document.createElement('input');
    countInput.type = 'number';
    countInput.min = '1';
    countInput.max = String(chip.maxLeds);
    countInput.value = String(currentConfig.ledCount || chip.maxLeds === 300 ? 30 : 64);
    if (currentConfig.ledCount) countInput.value = String(currentConfig.ledCount);

    // GPIO pin
    var pinLabel = document.createElement('label');
    pinLabel.textContent = 'GPIO Pin:';
    var pinSelect = document.createElement('select');
    var pinOptions = chip.pinGroup === 'esp8266'
      ? [{ val: '2', txt: 'GPIO2 (UART1)' }, { val: '3', txt: 'GPIO3 (DMA)' }]
      : [{ val: '4', txt: 'GPIO4' }, { val: '13', txt: 'GPIO13' }, { val: '14', txt: 'GPIO14' },
         { val: '16', txt: 'GPIO16' }, { val: '17', txt: 'GPIO17' }, { val: '18', txt: 'GPIO18' },
         { val: '19', txt: 'GPIO19' }, { val: '21', txt: 'GPIO21' }];
    pinOptions.forEach(function (o) {
      var opt = document.createElement('option');
      opt.value = o.val;
      opt.textContent = o.txt;
      if (String(currentConfig.gpioPin) === o.val) opt.selected = true;
      pinSelect.appendChild(opt);
    });

    // Topology
    var topoLabel = document.createElement('label');
    topoLabel.textContent = 'Topology:';
    var topoSelect = document.createElement('select');
    [['strip', 'Strip'], ['matrix_zigzag', 'Matrix Zigzag'], ['matrix_progressive', 'Matrix Progressive'], ['ring', 'Ring']].forEach(function (pair) {
      var opt = document.createElement('option');
      opt.value = pair[0];
      opt.textContent = pair[1];
      if (currentConfig.topology === pair[0]) opt.selected = true;
      topoSelect.appendChild(opt);
    });

    // Brightness
    var brightnessLabel = document.createElement('label');
    brightnessLabel.textContent = 'Brightness (%):';
    var brightnessInput = document.createElement('input');
    brightnessInput.type = 'number';
    brightnessInput.min = '0';
    brightnessInput.max = '100';
    brightnessInput.value = String(currentConfig.brightness !== undefined ? currentConfig.brightness : 80);

    // Default effect
    var effectLabel = document.createElement('label');
    effectLabel.textContent = 'Default Effect:';
    var effectInput = document.createElement('input');
    effectInput.type = 'text';
    effectInput.value = currentConfig.defaultEffect || 'solid';
    effectInput.placeholder = 'solid';

    // Buttons
    var saveBtn = document.createElement('button');
    saveBtn.className = 'btn btn-success btn-sm';
    saveBtn.textContent = 'Save override';

    var cancelBtn = document.createElement('button');
    cancelBtn.className = 'btn btn-sm';
    cancelBtn.textContent = 'Cancel';
    cancelBtn.addEventListener('click', function () { form.remove(); });

    var formMsg = document.createElement('span');
    formMsg.className = 'led-override-form-msg';

    saveBtn.addEventListener('click', function () {
      var count = parseInt(countInput.value, 10);
      var pin   = parseInt(pinSelect.value, 10);
      var brt   = parseInt(brightnessInput.value, 10);

      if (isNaN(count) || count < 1 || count > chip.maxLeds) {
        formMsg.textContent = 'LED count must be 1–' + chip.maxLeds;
        return;
      }
      if (isNaN(brt) || brt < 0 || brt > 100) {
        formMsg.textContent = 'Brightness must be 0–100';
        return;
      }

      var config = {
        ledCount:      count,
        gpioPin:       pin,
        topology:      topoSelect.value,
        brightness:    brt,
        defaultEffect: effectInput.value.trim() || 'solid',
      };

      _handleSaveDeviceOverride(device, config, rowEl, form, formMsg);
    });

    form.appendChild(countLabel);
    form.appendChild(countInput);
    form.appendChild(pinLabel);
    form.appendChild(pinSelect);
    form.appendChild(topoLabel);
    form.appendChild(topoSelect);
    form.appendChild(brightnessLabel);
    form.appendChild(brightnessInput);
    form.appendChild(effectLabel);
    form.appendChild(effectInput);
    form.appendChild(saveBtn);
    form.appendChild(cancelBtn);
    form.appendChild(formMsg);
    rowEl.appendChild(form);
  }

  function _handleSaveDeviceOverride(device, config, rowEl, formEl, msgEl) {
    fetch('/api/leds/config/' + encodeURIComponent(device.type) + '/' + encodeURIComponent(device.chipId), {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config),
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) {
          if (msgEl) msgEl.textContent = data.error;
        } else {
          if (formEl) formEl.remove();
          // Refresh row with new config
          var newRow = _createOverrideRow(device, config, true);
          rowEl.parentNode.replaceChild(newRow, rowEl);
          _showSuccess('Override saved for ' + device.chipId);
        }
      })
      .catch(function (err) {
        if (msgEl) msgEl.textContent = 'Save failed: ' + err.message;
      });
  }

  function _handleResetDeviceOverride(device, rowEl) {
    fetch('/api/leds/config/' + encodeURIComponent(device.type) + '/' + encodeURIComponent(device.chipId), {
      method: 'DELETE',
    })
      .then(function (res) { return res.json(); })
      .then(function (data) {
        if (data.error) {
          _showError(data.error);
        } else {
          // Re-fetch effective config and update row
          fetch('/api/leds/config/' + encodeURIComponent(device.type) + '/' + encodeURIComponent(device.chipId))
            .then(function (r) { return r.json(); })
            .then(function (d) {
              var newRow = _createOverrideRow(device, d.config || null, false);
              rowEl.parentNode.replaceChild(newRow, rowEl);
              _showSuccess('Override removed for ' + device.chipId);
            });
        }
      })
      .catch(function (err) { _showError('Reset failed: ' + err.message); });
  }

  // ── UI Helpers ──────────────────────────────────────────────────────────────

  function _showError(msg) {
    var el = _el('led-error');
    if (!el) return;
    el.textContent = msg;
    el.classList.remove('hidden');
    setTimeout(function () { el.classList.add('hidden'); }, 5000);
  }

  function _showSuccess(msg) {
    var el = _el('led-success');
    if (!el) return;
    el.textContent = msg;
    el.classList.remove('hidden');
    setTimeout(function () { el.classList.add('hidden'); }, 3000);
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  return {
    init: init,
    updateDeviceList: updateDeviceList,
    selectDevice: selectDevice,
  };
})();

