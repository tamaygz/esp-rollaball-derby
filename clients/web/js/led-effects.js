'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.LEDEffects — Client-side LED effect implementations.
 *
 * Public API:
 *   Derby.LEDEffects.create(name, params, api) — create effect instance
 *   Derby.LEDEffects.update(effect, deltaTime) — update effect animation
 *   Derby.LEDEffects.getAllEffects()           — get list of available effects
 */
Derby.LEDEffects = (function () {

  // ── Effect Registry ─────────────────────────────────────────────────────────

  var _effects = {
    solid: SolidEffect,
    blink: BlinkEffect,
    pulse: PulseEffect,
    rainbow: RainbowEffect,
    chase: ChaseEffect,
    sparkle: SparkleEffect,
  };

  // ── Color Utilities ─────────────────────────────────────────────────────────

  function hexToRgb(hex) {
    var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16),
    } : { r: 255, g: 255, b: 255 };
  }

  function hsvToRgb(h, s, v) {
    var r, g, b;
    var i = Math.floor(h * 6);
    var f = h * 6 - i;
    var p = v * (1 - s);
    var q = v * (1 - f * s);
    var t = v * (1 - (1 - f) * s);

    switch (i % 6) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
    }

    return {
      r: Math.round(r * 255),
      g: Math.round(g * 255),
      b: Math.round(b * 255),
    };
  }

  // ── Effect: Solid ───────────────────────────────────────────────────────────

  function SolidEffect(params, api) {
    this.color = hexToRgb(params.color || '#FFFFFF');
    this.api = api;
    this.initialized = false;
  }

  SolidEffect.prototype.update = function (deltaTime) {
    if (!this.initialized) {
      this.api.setAll(this.color);
      this.initialized = true;
    }
  };

  // ── Effect: Blink ───────────────────────────────────────────────────────────

  function BlinkEffect(params, api) {
    this.color = hexToRgb(params.color || '#FFFFFF');
    this.speed = params.speed || 500; // ms
    this.api = api;
    this.elapsed = 0;
    this.state = true;
  }

  BlinkEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;

    if (this.elapsed >= this.speed) {
      this.elapsed -= this.speed;
      this.state = !this.state;

      if (this.state) {
        this.api.setAll(this.color);
      } else {
        this.api.setAll({ r: 0, g: 0, b: 0 });
      }
    }
  };

  // ── Effect: Pulse ───────────────────────────────────────────────────────────

  function PulseEffect(params, api) {
    this.color = hexToRgb(params.color || '#FFFFFF');
    this.speed = params.speed || 1000; // ms for full cycle
    this.api = api;
    this.time = 0;
  }

  PulseEffect.prototype.update = function (deltaTime) {
    this.time += deltaTime;
    var cycle = this.time % this.speed;
    var brightness = (Math.sin((cycle / this.speed) * Math.PI * 2) + 1) / 2;

    var color = {
      r: Math.round(this.color.r * brightness),
      g: Math.round(this.color.g * brightness),
      b: Math.round(this.color.b * brightness),
    };

    this.api.setAll(color);
  };

  // ── Effect: Rainbow ─────────────────────────────────────────────────────────

  function RainbowEffect(params, api) {
    this.speed = params.speed || 2000; // ms for full rotation
    this.api = api;
    this.time = 0;
  }

  RainbowEffect.prototype.update = function (deltaTime) {
    this.time += deltaTime;
    var hueOffset = (this.time / this.speed) % 1.0;

    for (var i = 0; i < this.api.ledCount; i++) {
      var hue = (hueOffset + (i / this.api.ledCount)) % 1.0;
      var color = hsvToRgb(hue, 1.0, 1.0);
      this.api.setPixel(i, color);
    }
  };

  // ── Effect: Chase ───────────────────────────────────────────────────────────

  function ChaseEffect(params, api) {
    this.color = hexToRgb(params.color || '#FFFFFF');
    this.speed = params.speed || 100; // ms per LED
    this.size = params.size || 3; // number of lit LEDs
    this.api = api;
    this.elapsed = 0;
    this.position = 0;
  }

  ChaseEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;

    if (this.elapsed >= this.speed) {
      this.elapsed -= this.speed;
      this.position = (this.position + 1) % this.api.ledCount;

      // Clear all
      this.api.setAll({ r: 0, g: 0, b: 0 });

      // Light chase window
      for (var i = 0; i < this.size; i++) {
        var index = (this.position + i) % this.api.ledCount;
        this.api.setPixel(index, this.color);
      }
    }
  };

  // ── Effect: Sparkle ─────────────────────────────────────────────────────────

  function SparkleEffect(params, api) {
    this.color = hexToRgb(params.color || '#FFFFFF');
    this.density = params.density || 0.1; // 0-1
    this.api = api;
    this.elapsed = 0;
    this.updateInterval = 50;
  }

  SparkleEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;

    if (this.elapsed >= this.updateInterval) {
      this.elapsed -= this.updateInterval;

      // Dim all LEDs
      for (var i = 0; i < this.api.ledCount; i++) {
        // Fade to black
        this.api.setPixel(i, { r: 0, g: 0, b: 0 });
      }

      // Add random sparkles
      var sparkleCount = Math.ceil(this.api.ledCount * this.density);
      for (var j = 0; j < sparkleCount; j++) {
        var index = Math.floor(Math.random() * this.api.ledCount);
        this.api.setPixel(index, this.color);
      }
    }
  };

  // ── Public API ──────────────────────────────────────────────────────────────

  function create(name, params, api) {
    var EffectClass = _effects[name];
    if (!EffectClass) {
      console.error('[LEDEffects] Unknown effect:', name);
      return null;
    }

    return new EffectClass(params || {}, api);
  }

  function update(effect, deltaTime) {
    if (effect && effect.update) {
      effect.update(deltaTime);
    }
  }

  function getAllEffects() {
    return Object.keys(_effects).map(function (name) {
      return {
        name: name,
        label: _formatLabel(name),
      };
    });
  }

  function _formatLabel(name) {
    return name.charAt(0).toUpperCase() + name.slice(1);
  }

  return {
    create: create,
    update: update,
    getAllEffects: getAllEffects,
  };
})();
