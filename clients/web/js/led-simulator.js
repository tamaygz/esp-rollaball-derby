'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.LEDSimulator — Canvas-based LED strip/matrix/ring simulator.
 *
 * Public API:
 *   Derby.LEDSimulator.init(canvasId)           — initialize simulator on canvas element
 *   Derby.LEDSimulator.setConfig({ledCount, topology}) — update simulator configuration
 *   Derby.LEDSimulator.setPixel(index, {r, g, b})      — set single LED color
 *   Derby.LEDSimulator.setAll({r, g, b})               — set all LEDs to same color
 *   Derby.LEDSimulator.setBrightness(0-255)            — set global brightness
 *   Derby.LEDSimulator.show()                          — commit changes to screen
 *   Derby.LEDSimulator.clear()                         — turn off all LEDs
 *   Derby.LEDSimulator.start()                         — start animation loop
 *   Derby.LEDSimulator.stop()                          — stop animation loop
 */
Derby.LEDSimulator = (function () {
  var _canvas = null;
  var _ctx = null;
  var _config = {
    ledCount: 50,
    topology: 'strip', // strip | matrix_zigzag | matrix_progressive | ring
    matrixRows: 8,
    matrixCols: 8,
  };
  var _pixels = [];
  var _brightness = 1.0;
  var _animationId = null;
  var _currentEffect = null;
  var _lastFrameTime = 0;

  // ── Initialization ──────────────────────────────────────────────────────────

  function init(canvasId) {
    _canvas = document.getElementById(canvasId);
    if (!_canvas) {
      console.error('[LEDSimulator] Canvas not found:', canvasId);
      return false;
    }

    _ctx = _canvas.getContext('2d');
    _pixels = _initPixels(_config.ledCount);
    _resizeCanvas();
    return true;
  }

  function _initPixels(count) {
    var pixels = [];
    for (var i = 0; i < count; i++) {
      pixels.push({ r: 0, g: 0, b: 0 });
    }
    return pixels;
  }

  function _resizeCanvas() {
    if (!_canvas) return;

    // Set display size (CSS pixels)
    var container = _canvas.parentElement;
    if (container) {
      _canvas.style.width = '100%';
      _canvas.style.height = 'auto';
    }

    // Set actual size in memory (scaled for retina)
    var scale = window.devicePixelRatio || 1;
    var width = _canvas.clientWidth;
    var height = _calculateCanvasHeight();

    _canvas.width = width * scale;
    _canvas.height = height * scale;
    _ctx.scale(scale, scale);

    _render();
  }

  function _calculateCanvasHeight() {
    if (_config.topology === 'ring') return 200;
    if (_config.topology.startsWith('matrix')) return 200;
    return 60; // strip
  }

  // ── Configuration ───────────────────────────────────────────────────────────

  function setConfig(config) {
    if (config.ledCount) {
      _config.ledCount = Math.min(config.ledCount, 300);
      _pixels = _initPixels(_config.ledCount);
    }
    if (config.topology) _config.topology = config.topology;
    if (config.matrixRows) _config.matrixRows = config.matrixRows;
    if (config.matrixCols) _config.matrixCols = config.matrixCols;

    _resizeCanvas();
  }

  function setBrightness(value) {
    _brightness = Math.max(0, Math.min(255, value)) / 255;
    _render();
  }

  // ── Pixel Control ───────────────────────────────────────────────────────────

  function setPixel(index, color) {
    if (index < 0 || index >= _pixels.length) return;
    _pixels[index] = {
      r: color.r || 0,
      g: color.g || 0,
      b: color.b || 0,
    };
  }

  function setAll(color) {
    for (var i = 0; i < _pixels.length; i++) {
      _pixels[i] = {
        r: color.r || 0,
        g: color.g || 0,
        b: color.b || 0,
      };
    }
  }

  function clear() {
    setAll({ r: 0, g: 0, b: 0 });
    _render();
  }

  function show() {
    _render();
  }

  // ── Rendering ───────────────────────────────────────────────────────────────

  function _render() {
    if (!_ctx || !_canvas) return;

    var width = _canvas.clientWidth;
    var height = _canvas.clientHeight;

    _ctx.clearRect(0, 0, width, height);

    if (_config.topology === 'strip') {
      _renderStrip(width, height);
    } else if (_config.topology.startsWith('matrix')) {
      _renderMatrix(width, height);
    } else if (_config.topology === 'ring') {
      _renderRing(width, height);
    }
  }

  function _renderStrip(width, height) {
    var ledCount = _config.ledCount;
    var ledSpacing = width / ledCount;
    var ledRadius = Math.min(ledSpacing / 2.5, 12);
    var y = height / 2;

    for (var i = 0; i < ledCount; i++) {
      var x = (i + 0.5) * ledSpacing;
      var pixel = _pixels[i];
      var color = _applyBrightness(pixel);

      _drawLED(x, y, ledRadius, color);
    }
  }

  function _renderMatrix(width, height) {
    var rows = _config.matrixRows;
    var cols = _config.matrixCols;
    var cellWidth = width / cols;
    var cellHeight = height / rows;
    var ledRadius = Math.min(cellWidth, cellHeight) / 2.5;

    for (var row = 0; row < rows; row++) {
      for (var col = 0; col < cols; col++) {
        var index = _matrixIndexToPixel(row, col);
        if (index >= _pixels.length) continue;

        var x = (col + 0.5) * cellWidth;
        var y = (row + 0.5) * cellHeight;
        var pixel = _pixels[index];
        var color = _applyBrightness(pixel);

        _drawLED(x, y, ledRadius, color);
      }
    }
  }

  function _renderRing(width, height) {
    var centerX = width / 2;
    var centerY = height / 2;
    var radius = Math.min(width, height) * 0.35;
    var ledRadius = 8;
    var ledCount = _config.ledCount;

    for (var i = 0; i < ledCount; i++) {
      var angle = (i / ledCount) * Math.PI * 2 - Math.PI / 2;
      var x = centerX + Math.cos(angle) * radius;
      var y = centerY + Math.sin(angle) * radius;
      var pixel = _pixels[i];
      var color = _applyBrightness(pixel);

      _drawLED(x, y, ledRadius, color);
    }
  }

  function _matrixIndexToPixel(row, col) {
    if (_config.topology === 'matrix_zigzag') {
      if (row % 2 === 0) {
        return row * _config.matrixCols + col;
      } else {
        return row * _config.matrixCols + (_config.matrixCols - 1 - col);
      }
    } else {
      return row * _config.matrixCols + col;
    }
  }

  function _drawLED(x, y, radius, color) {
    var brightness = (color.r + color.g + color.b) / (255 * 3);
    var rgbStr = 'rgb(' + color.r + ',' + color.g + ',' + color.b + ')';

    // Draw glow gradient if LED is lit
    if (brightness > 0.05) {
      var gradient = _ctx.createRadialGradient(x, y, 0, x, y, radius * 2);
      gradient.addColorStop(0, rgbStr);
      gradient.addColorStop(0.5, 'rgba(' + color.r + ',' + color.g + ',' + color.b + ',0.3)');
      gradient.addColorStop(1, 'rgba(' + color.r + ',' + color.g + ',' + color.b + ',0)');
      _ctx.fillStyle = gradient;
      _ctx.fillRect(x - radius * 2, y - radius * 2, radius * 4, radius * 4);
    }

    // Draw LED circle
    _ctx.beginPath();
    _ctx.arc(x, y, radius, 0, Math.PI * 2);
    _ctx.fillStyle = brightness > 0.05 ? rgbStr : '#111';
    _ctx.fill();
    _ctx.strokeStyle = '#333';
    _ctx.lineWidth = 1;
    _ctx.stroke();
  }

  function _applyBrightness(pixel) {
    return {
      r: Math.round(pixel.r * _brightness),
      g: Math.round(pixel.g * _brightness),
      b: Math.round(pixel.b * _brightness),
    };
  }

  // ── Animation Loop ──────────────────────────────────────────────────────────

  function start() {
    if (_animationId) return;
    _lastFrameTime = performance.now();
    _animate();
  }

  function stop() {
    if (_animationId) {
      cancelAnimationFrame(_animationId);
      _animationId = null;
    }
  }

  function _animate() {
    var now = performance.now();
    var deltaTime = now - _lastFrameTime;
    _lastFrameTime = now;

    if (_currentEffect && Derby.LEDEffects) {
      Derby.LEDEffects.update(_currentEffect, deltaTime);
    }

    _render();
    _animationId = requestAnimationFrame(_animate);
  }

  function playEffect(effectName, params) {
    if (!Derby.LEDEffects) {
      console.error('[LEDSimulator] LEDEffects not loaded');
      return;
    }

    _currentEffect = Derby.LEDEffects.create(effectName, params, {
      setPixel: setPixel,
      setAll: setAll,
      ledCount: _config.ledCount,
    });

    if (!_animationId) {
      start();
    }
  }

  function stopEffect() {
    _currentEffect = null;
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  return {
    init: init,
    setConfig: setConfig,
    setPixel: setPixel,
    setAll: setAll,
    setBrightness: setBrightness,
    show: show,
    clear: clear,
    start: start,
    stop: stop,
    playEffect: playEffect,
    stopEffect: stopEffect,
  };
})();
