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
    solid:        SolidEffect,
    blink:        BlinkEffect,
    pulse:        PulseEffect,
    rainbow:      RainbowEffect,
    chase:        ChaseEffect,
    sparkle:      SparkleEffect,
    countdown:    CountdownEffect,
    text:         TextEffect,
    winner:       WinnerEffect,
    clear:        ClearEffect,
    ballroll:     BallRollEffect,
    camelchew:    CamelChewEffect,
    camelspeedup: CamelSpeedupEffect,
    scorehigh:    ScoreHighEffect,
    tolead:       ToLeadEffect,
    farbehind:    FarBehindEffect,
    gamestart:    GameStartEffect,
    gameend:      GameEndEffect,
    someonewon:   SomeoneWonEffect,
  };

  // ── Shared matrix helper ─────────────────────────────────────────────────────

  function _matrixPixelIdx(row, col, rows, cols, topology) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) return -1;
    if (topology === 'matrix_zigzag') {
      return (row % 2 === 0)
        ? row * cols + col
        : row * cols + (cols - 1 - col);
    }
    return row * cols + col;
  }

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

  // ── Effect: Ball Roll (matrix) ──────────────────────────────────────────────

  function BallRollEffect(params, api) {
    this.api = api;
    this.rows = api.matrixRows || 8;
    this.cols = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';

    this.ballColor = hexToRgb(params.color || '#FFFFFF');
    this.holeRimColor = { r: 50, g: 50, b: 90 };
    this.holeFillColor = { r: 8, g: 8, b: 18 };
    this.celebColor = hexToRgb(params.color || '#00FF88');

    // Three holes in a V shape near the top
    var cx = (this.cols - 1) / 2;
    var topRow = Math.max(1, Math.round(this.rows * 0.15));
    var midRow = Math.min(this.rows - 3, Math.max(topRow + 2, Math.round(this.rows * 0.35)));
    var leftCol = Math.max(1, Math.round(this.cols * 0.15));
    var rightCol = Math.min(this.cols - 2, Math.round(this.cols * 0.85));

    this.holes = [
      { row: topRow, col: leftCol },
      { row: topRow, col: rightCol },
      { row: midRow, col: Math.round(cx) },
    ];

    // Pick random target hole
    this.targetIdx = Math.floor(Math.random() * 3);
    this.target = this.holes[this.targetIdx];

    // Ball starts at bottom center
    this.startPos = { row: this.rows - 1, col: cx };
    this.ballRow = this.startPos.row;
    this.ballCol = this.startPos.col;

    // Phase machine: rolling → dropping → celebrating → done
    this.phase = 'rolling';
    this.elapsed = 0;
    this.rollDuration = params.speed || 2000;
    this.dropDuration = 350;
    this.celebDuration = 900;
  }

  BallRollEffect.prototype._pixelIndex = function (row, col) {
    return _matrixPixelIdx(Math.round(row), Math.round(col), this.rows, this.cols, this.topology);
  };

  BallRollEffect.prototype._setMPixel = function (row, col, color) {
    var idx = this._pixelIndex(row, col);
    if (idx >= 0 && idx < this.api.ledCount) {
      this.api.setPixel(idx, color);
    }
  };

  BallRollEffect.prototype._drawHoles = function () {
    for (var i = 0; i < this.holes.length; i++) {
      var h = this.holes[i];
      // Dark center
      this._setMPixel(h.row, h.col, this.holeFillColor);
      // Rim on cardinal directions
      this._setMPixel(h.row - 1, h.col, this.holeRimColor);
      this._setMPixel(h.row + 1, h.col, this.holeRimColor);
      this._setMPixel(h.row, h.col - 1, this.holeRimColor);
      this._setMPixel(h.row, h.col + 1, this.holeRimColor);
    }
  };

  BallRollEffect.prototype._drawBall = function (row, col, brightness) {
    var b = brightness !== undefined ? brightness : 1;
    var c = {
      r: Math.round(this.ballColor.r * b),
      g: Math.round(this.ballColor.g * b),
      b: Math.round(this.ballColor.b * b),
    };

    var r = Math.round(row);
    var cc = Math.round(col);
    this._setMPixel(r, cc, c);

    // Subtle glow on adjacent pixels
    var dim = { r: Math.round(c.r * 0.25), g: Math.round(c.g * 0.25), b: Math.round(c.b * 0.25) };
    this._setMPixel(r - 1, cc, dim);
    this._setMPixel(r + 1, cc, dim);
    this._setMPixel(r, cc - 1, dim);
    this._setMPixel(r, cc + 1, dim);
  };

  BallRollEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    if (this.phase === 'rolling') {
      var t = Math.min(1, this.elapsed / this.rollDuration);
      // Ease-in (quadratic) — ball accelerates toward target
      var eased = t * t;

      this.ballRow = this.startPos.row + (this.target.row - this.startPos.row) * eased;
      this.ballCol = this.startPos.col + (this.target.col - this.startPos.col) * eased;

      this._drawHoles();
      this._drawBall(this.ballRow, this.ballCol, 1);

      if (t >= 1) {
        this.phase = 'dropping';
        this.elapsed = 0;
      }

    } else if (this.phase === 'dropping') {
      var dt = Math.min(1, this.elapsed / this.dropDuration);

      this._drawHoles();
      // Ball shrinks/fades as it enters the hole
      this._drawBall(this.target.row, this.target.col, 1 - dt * dt);

      if (dt >= 1) {
        this.phase = 'celebrating';
        this.elapsed = 0;
      }

    } else if (this.phase === 'celebrating') {
      var ct = Math.min(1, this.elapsed / this.celebDuration);
      var maxR = Math.max(this.rows, this.cols);
      var ringRadius = ct * maxR;
      var ringWidth = 1.5;

      for (var row = 0; row < this.rows; row++) {
        for (var col = 0; col < this.cols; col++) {
          var dist = Math.sqrt(
            (row - this.target.row) * (row - this.target.row) +
            (col - this.target.col) * (col - this.target.col)
          );

          // Expanding ring
          if (Math.abs(dist - ringRadius) < ringWidth) {
            var fade = 1 - ct;
            this._setMPixel(row, col, {
              r: Math.round(this.celebColor.r * fade),
              g: Math.round(this.celebColor.g * fade),
              b: Math.round(this.celebColor.b * fade),
            });
          }
        }
      }

      // Flash the target hole
      var flash = Math.sin(ct * Math.PI * 8) > 0;
      if (flash) {
        this._setMPixel(this.target.row, this.target.col, this.celebColor);
      }

      if (ct >= 1) {
        // Loop: pick a new random hole and restart
        this.targetIdx = Math.floor(Math.random() * 3);
        this.target    = this.holes[this.targetIdx];
        this.ballRow   = this.startPos.row;
        this.ballCol   = this.startPos.col;
        this.phase     = 'rolling';
        this.elapsed   = 0;
      }
    }
  };  // BallRollEffect.update

  // ── Effect: Countdown (matrix) ──────────────────────────────────────────────

  function CountdownEffect(params, api) {
    this.api           = api;
    this.rows          = api.matrixRows || 8;
    this.cols          = api.matrixCols || 8;
    this.stepDuration  = 900; // ms per number
    this.elapsed       = 0;
    this.step          = 0;   // 0=3, 1=2, 2=1, 3=GO
    this._drawStep();
  }

  // Steps: 0→show "3" (3 rows), 1→"2" (2 rows), 2→"1" (1 row), 3→"GO" (all on green)
  var _COUNTDOWN_COLORS = [
    { r: 255, g: 120, b: 0 },  // 3 — orange
    { r: 255, g: 220, b: 0 },  // 2 — yellow
    { r: 100, g: 255, b: 0 },  // 1 — lime
    { r: 0,   g: 220, b: 0 },  // GO — green
  ];

  CountdownEffect.prototype._drawStep = function () {
    var c    = _COUNTDOWN_COLORS[Math.min(this.step, 3)];
    this.api.setAll({ r: 0, g: 0, b: 0 });
    if (this.step >= 3) {
      this.api.setAll(c);
    } else {
      var rowsOn = 3 - this.step; // 3, 2, 1
      for (var r = 0; r < rowsOn; r++) {
        for (var col = 0; col < this.cols; col++) {
          var idx = _matrixPixelIdx(r, col, this.rows, this.cols, this.api.topology);
          if (idx >= 0) this.api.setPixel(idx, c);
        }
      }
    }
  };

  CountdownEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    if (this.elapsed >= this.stepDuration) {
      this.elapsed -= this.stepDuration;
      this.step    = (this.step + 1) % 4;  // loop
      this._drawStep();
    }
  };

  // ── Effect: Text / Scroll (matrix) ─────────────────────────────────────────
  // Simulator approximation: sweeps a bright column of the chosen color left→right.

  function TextEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.color    = hexToRgb(params.color || '#FFFFFF');
    this.speed    = Math.max(20, params.speed || 80); // ms per column
    this.elapsed  = 0;
    this.col      = 0;
  }

  TextEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    if (this.elapsed >= this.speed) {
      this.elapsed -= this.speed;
      this.api.setAll({ r: 0, g: 0, b: 0 });
      var col = this.col;
      for (var r = 0; r < this.rows; r++) {
        var idx = _matrixPixelIdx(r, col, this.rows, this.cols, this.api.topology);
        if (idx >= 0) this.api.setPixel(idx, this.color);
      }
      // Dim adjacent columns
      var dim = { r: Math.round(this.color.r * 0.2), g: Math.round(this.color.g * 0.2), b: Math.round(this.color.b * 0.2) };
      for (var r2 = 0; r2 < this.rows; r2++) {
        var il = _matrixPixelIdx(r2, col - 1, this.rows, this.cols, this.api.topology);
        var ir = _matrixPixelIdx(r2, col + 1, this.rows, this.cols, this.api.topology);
        if (il >= 0) this.api.setPixel(il, dim);
        if (ir >= 0) this.api.setPixel(ir, dim);
      }
      this.col = (this.col + 1) % this.cols;
    }
  };

  // ── Effect: Winner (matrix) ─────────────────────────────────────────────────
  // Gold sparkle on the matrix, looping.

  function WinnerEffect(params, api) {
    this.api          = api;
    this.ledCount     = api.ledCount;
    this.elapsed      = 0;
    this.updateInterval = 60; // ms
  }

  WinnerEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    if (this.elapsed >= this.updateInterval) {
      this.elapsed -= this.updateInterval;
      this.api.setAll({ r: 0, g: 0, b: 0 });
      var count = Math.max(1, Math.round(this.ledCount * 0.25));
      for (var i = 0; i < count; i++) {
        var idx = Math.floor(Math.random() * this.ledCount);
        // Gold: randomise between orange-gold and bright white
        var v = 150 + Math.floor(Math.random() * 105);
        this.api.setPixel(idx, { r: v, g: Math.round(v * 0.8), b: 0 });
      }
    }
  };

  // ── Effect: Clear (one-shot, then stays off) ────────────────────────────────

  function ClearEffect(params, api) {
    api.setAll({ r: 0, g: 0, b: 0 });
  }

  ClearEffect.prototype.update = function () { /* static — nothing to animate */ };

  // ── Effect: Camel Chew (matrix) ─────────────────────────────────────────────
  // Close-up camel face with animated chewing jaw, hay in mouth, ear flicks, blink.

  function CamelChewEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';

    // Colour palette
    this.C_FUR   = { r: 200, g: 140, b: 55 };   // warm tan fur
    this.C_EAR   = { r: 235, g: 175, b: 70 };   // slightly brighter ear
    this.C_EAR_F = { r: 255, g: 210, b: 110 };  // ear flick highlight
    this.C_LIP   = { r: 155, g: 95,  b: 30 };   // lip / muzzle
    this.C_EYE   = { r: 28,  g: 16,  b: 6  };   // dark eye
    this.C_NOS   = { r: 55,  g: 32,  b: 12 };   // nostril
    this.C_MOUTH = { r: 18,  g: 10,  b: 4  };   // open mouth interior
    this.C_HAY   = hexToRgb(params.color || '#7AC520');  // hay / grass colour
    this.C_CHIN  = { r: 175, g: 120, b: 45 };   // slightly shaded chin

    this.chewCycle = Math.max(800, params.speed || 2400);  // ms per full chew
    this.chewTime  = 0;

    // Blink state
    this._blinkTimer   = 2500 + Math.random() * 2500;
    this._blinkElapsed = 0;
    this._blinking     = false;
    this._BLINK_DUR    = 120;

    // Ear-flick state
    this._earTimer     = 3500 + Math.random() * 3500;
    this._earElapsed   = 0;
    this._earFlicking  = false;
    this._earSide      = 0;   // 0=left  1=right  2=both
    this._EAR_FLICK_DUR = 220;

    // Proportional geometry (adapts to matrix size)
    var r = this.rows, c = this.cols;
    this._eyeRow  = Math.round(r * 0.28);   // row ~2 of 8
    this._eyeColL = Math.round(c * 0.25);   // col ~2 of 8
    this._eyeColR = Math.round(c * 0.625);  // col ~5 of 8
    this._nosRow  = Math.round(r * 0.50);   // row ~4 of 8
    this._lipRow  = Math.round(r * 0.625);  // row ~5 of 8
    this._jawRow  = Math.min(r - 2, Math.round(r * 0.75));  // row ~6 of 8
    this._chinRow = r - 1;
    this._mCL     = 1;      // mouth left col bound
    this._mCR     = c - 2;  // mouth right col bound
  }

  CamelChewEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  CamelChewEffect.prototype._lerp = function (a, b, t) {
    return {
      r: Math.round(a.r + (b.r - a.r) * t),
      g: Math.round(a.g + (b.g - a.g) * t),
      b: Math.round(a.b + (b.b - a.b) * t),
    };
  };

  CamelChewEffect.prototype.update = function (deltaTime) {
    this.chewTime = (this.chewTime + deltaTime) % this.chewCycle;

    // ── Chew curve ─────────────────────────────────────────────────────────────
    // Phases: [0–38%] pause (closed) → [38–56%] open ease-in
    //         → [56–84%] jaw oscillates (mini-chews) → [84–100%] close ease-out
    var rawT = this.chewTime / this.chewCycle;
    var mouthOpen;
    if (rawT < 0.38) {
      mouthOpen = 0;
    } else if (rawT < 0.56) {
      var t0 = (rawT - 0.38) / 0.18;
      mouthOpen = t0 * t0;                                          // ease-in opening
    } else if (rawT < 0.84) {
      var t1 = (rawT - 0.56) / 0.28;
      mouthOpen = 0.85 + Math.sin(t1 * Math.PI * 3) * 0.15;       // 1.5 tiny chews at open
    } else {
      var t2 = (rawT - 0.84) / 0.16;
      mouthOpen = (1 - t2) * (1 - t2) * 0.85;                     // ease-out closing
    }

    // ── Blink ──────────────────────────────────────────────────────────────────
    this._blinkElapsed += deltaTime;
    if (!this._blinking && this._blinkElapsed >= this._blinkTimer) {
      this._blinking     = true;
      this._blinkElapsed = 0;
    }
    if (this._blinking && this._blinkElapsed >= this._BLINK_DUR) {
      this._blinking     = false;
      this._blinkElapsed = 0;
      this._blinkTimer   = 2500 + Math.random() * 3000;
    }

    // ── Ear flick ──────────────────────────────────────────────────────────────
    this._earElapsed += deltaTime;
    if (!this._earFlicking && this._earElapsed >= this._earTimer) {
      this._earFlicking = true;
      this._earElapsed  = 0;
      this._earSide     = Math.floor(Math.random() * 3);
    }
    if (this._earFlicking && this._earElapsed >= this._EAR_FLICK_DUR) {
      this._earFlicking = false;
      this._earElapsed  = 0;
      this._earTimer    = 3500 + Math.random() * 4000;
    }

    this._drawFrame(mouthOpen);
  };

  CamelChewEffect.prototype._drawFrame = function (mouthOpen) {
    var rows = this.rows, cols = this.cols;
    var FUR  = this.C_FUR;
    var mid  = Math.floor(cols / 2);

    // Fill entire matrix with base fur
    for (var r = 0; r < rows; r++) {
      for (var c = 0; c < cols; c++) {
        this._px(r, c, FUR);
      }
    }

    // Ears: rows 0-1, outer two cols each side
    // earSide: 0=left  1=right  2=both — only the matching side flicks
    var cEarL = (this._earFlicking && this._earSide !== 1) ? this.C_EAR_F : this.C_EAR;
    var cEarR = (this._earFlicking && this._earSide !== 0) ? this.C_EAR_F : this.C_EAR;
    for (var er = 0; er <= 1; er++) {
      this._px(er, 0,        cEarL);
      this._px(er, 1,        cEarL);
      this._px(er, cols - 2, cEarR);
      this._px(er, cols - 1, cEarR);
    }

    // Eyes
    var cEye = this._blinking ? FUR : this.C_EYE;
    this._px(this._eyeRow, this._eyeColL, cEye);
    this._px(this._eyeRow, this._eyeColR, cEye);

    // Nostrils (same columns as eyes, lower row)
    this._px(this._nosRow, this._eyeColL, this.C_NOS);
    this._px(this._nosRow, this._eyeColR, this.C_NOS);

    // Upper lip row
    for (var cl = this._mCL; cl <= this._mCR; cl++) {
      this._px(this._lipRow, cl, this.C_LIP);
    }

    // Jaw row — blends from lip→mouth as mouthOpen rises
    var jawColor = (mouthOpen < 0.05)
      ? this.C_LIP
      : this._lerp(this.C_LIP, this.C_MOUTH, mouthOpen);
    for (var cm = this._mCL; cm <= this._mCR; cm++) {
      this._px(this._jawRow, cm, jawColor);
    }

    // Hay/grass: appears in centre pixels once mouth is open enough
    if (mouthOpen > 0.3) {
      var hayAlpha = Math.min(1, (mouthOpen - 0.3) / 0.4);
      var cHay = {
        r: Math.round(this.C_HAY.r * hayAlpha),
        g: Math.round(this.C_HAY.g * hayAlpha),
        b: Math.round(this.C_HAY.b * hayAlpha),
      };
      // Hay in centre 2-3 pixels of jaw row
      this._px(this._jawRow, mid - 1, cHay);
      this._px(this._jawRow, mid,     cHay);
      if (cols > 6) this._px(this._jawRow, mid + 1, cHay);
      // Wisps peeking from upper lip when wide open
      if (mouthOpen > 0.6) {
        var cHaySoft = {
          r: Math.round(cHay.r * 0.5),
          g: Math.round(cHay.g * 0.5),
          b: Math.round(cHay.b * 0.5),
        };
        this._px(this._lipRow, mid - 1, cHaySoft);
        this._px(this._lipRow, mid,     cHaySoft);
      }
    }

    // Chin (bottom row) — slightly shaded
    for (var cc = 0; cc < cols; cc++) {
      this._px(this._chinRow, cc, this.C_CHIN);
    }
  };

  // ── Effect: Camel Speedup (matrix) ──────────────────────────────────────────
  // Humorous camel accelerating with tail up + fart cloud, then burst of speed.

  function CamelSpeedupEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.color    = hexToRgb(params.color || '#FF6B00');
    this.speed    = params.speed || 1500;
    this.elapsed  = 0;
    this.phase    = 'fart';  // 'fart' → 'burst' → 'done'
    this.camelX   = this.cols * 0.25;
  }

  CamelSpeedupEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  CamelSpeedupEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    var fartDur = this.speed * 0.35;
    var burstDur = this.speed * 0.65;

    if (this.phase === 'fart') {
      var t = Math.min(1, this.elapsed / fartDur);
      // Camel at left, tail raised
      var camRow = Math.round(this.rows / 2);
      this._px(camRow, Math.round(this.camelX), this.color);
      this._px(camRow - 1, Math.round(this.camelX) + 1, this.color);
      // Fart cloud appears and fades: orange puffs
      var puffAlpha = 1 - t;
      var puffCol = Math.round(this.camelX + 2 + t * 2);
      this._px(camRow, puffCol, {
        r: Math.round(this.color.r * puffAlpha),
        g: Math.round(this.color.g * puffAlpha * 0.7),
        b: Math.round(this.color.b * puffAlpha * 0.5),
      });
      if (t >= 1) {
        this.phase = 'burst';
        this.elapsed = 0;
      }
    } else if (this.phase === 'burst') {
      var tb = Math.min(1, this.elapsed / burstDur);
      // Camel zooms right with motion blur
      var newX = this.camelX + tb * (this.cols * 0.6);
      this._px(Math.round(this.rows / 2), Math.round(newX), this.color);
      // Motion lines trail behind
      var trailAlpha = 1 - tb;
      for (var i = 0; i < 3; i++) {
        var tX = newX - (i + 1) * 0.8;
        this._px(Math.round(this.rows / 2), Math.round(tX), {
          r: Math.round(this.color.r * trailAlpha * (0.7 - i * 0.2)),
          g: Math.round(this.color.g * trailAlpha * (0.5 - i * 0.15)),
          b: Math.round(this.color.b * trailAlpha * (0.3 - i * 0.1)),
        });
      }
      if (tb >= 1) this.phase = 'done';
    }
  };

  // ── Effect: Score High (matrix) ─────────────────────────────────────────────
  // High score celebration: stars and sparkles radiating from center.

  function ScoreHighEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.color    = hexToRgb(params.color || '#FFD700');
    this.speed    = params.speed || 1200;
    this.elapsed  = 0;
    this.centerRow = Math.round(this.rows / 2);
    this.centerCol = Math.round(this.cols / 2);
  }

  ScoreHighEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  ScoreHighEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    var t = Math.min(1, this.elapsed / this.speed);
    var radius = t * (this.rows / 2 + 2);
    var ringWidth = 1.2;

    // Expanding rings of stars
    for (var row = 0; row < this.rows; row++) {
      for (var col = 0; col < this.cols; col++) {
        var dist = Math.sqrt(
          (row - this.centerRow) * (row - this.centerRow) +
          (col - this.centerCol) * (col - this.centerCol)
        );
        if (Math.abs(dist - radius) < ringWidth) {
          var fade = 1 - t * t;
          this._px(row, col, {
            r: Math.round(this.color.r * fade),
            g: Math.round(this.color.g * fade),
            b: Math.round(this.color.b * fade),
          });
        }
      }
    }

    // Center sparkles
    if (t > 0.2) {
      this._px(this.centerRow, this.centerCol, this.color);
      if (Math.random() < 0.4) {
        this._px(this.centerRow - 1, this.centerCol, {
          r: Math.round(this.color.r * 0.8),
          g: Math.round(this.color.g * 0.8),
          b: Math.round(this.color.b * 0.8),
        });
      }
    }
  };

  // ── Effect: Took Lead (matrix) ──────────────────────────────────────────────
  // Player takes the lead: crown/marker animation with celebratory glow.

  function ToLeadEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.color    = hexToRgb(params.color || '#FFD700');
    this.speed    = params.speed || 1000;
    this.elapsed  = 0;
    this.crownRow = Math.round(this.rows * 0.25);
    this.markerCol = Math.round(this.cols * 0.75);
  }

  ToLeadEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  ToLeadEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    var t = Math.min(1, this.elapsed / this.speed);

    // Crown peaks appear and pulse
    var pulse = Math.sin(t * Math.PI * 4) * 0.5 + 0.5;
    var crownAlpha = Math.min(1, t * 2) * pulse;

    // Draw crown (3 peaks)
    var crown = {
      r: Math.round(this.color.r * crownAlpha),
      g: Math.round(this.color.g * crownAlpha),
      b: Math.round(this.color.b * crownAlpha),
    };

    this._px(this.crownRow, Math.round(this.markerCol - 1), crown);
    this._px(this.crownRow, Math.round(this.markerCol), crown);
    this._px(this.crownRow, Math.round(this.markerCol + 1), crown);
    this._px(this.crownRow - 1, Math.round(this.markerCol), crown);

    // "1st" marker flashes below crown
    if (Math.sin(t * Math.PI * 3) > 0) {
      var marker = this.color;
      this._px(this.crownRow + 2, Math.round(this.markerCol - 1), marker);
      this._px(this.crownRow + 2, Math.round(this.markerCol), marker);
      this._px(this.crownRow + 2, Math.round(this.markerCol + 1), marker);
    }

    // Halo glow expands and fades
    var haloAlpha = Math.max(0, 1 - t);
    var halo = {
      r: Math.round(this.color.r * haloAlpha * 0.5),
      g: Math.round(this.color.g * haloAlpha * 0.5),
      b: Math.round(this.color.b * haloAlpha * 0.5),
    };
    for (var offset = 1; offset <= 3; offset++) {
      var intensity = haloAlpha / (offset + 1);
      if (intensity > 0) {
        this._px(this.crownRow - offset, this.markerCol, {
          r: Math.round(this.color.r * intensity),
          g: Math.round(this.color.g * intensity),
          b: Math.round(this.color.b * intensity),
        });
      }
    }
  };

  // ── Effect: Far Behind (matrix) ─────────────────────────────────────────────
  // Tired camel at the back: slow movement, sad indicators.

  function FarBehindEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.color    = hexToRgb(params.color || '#4488FF');
    this.speed    = params.speed || 1800;
    this.elapsed  = 0;
  }

  FarBehindEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  FarBehindEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    var t = Math.min(1, this.elapsed / this.speed);

    // Tired camel on the left (far behind)
    var camelRow = Math.round(this.rows * 0.5);
    var camelCol = Math.round(this.cols * 0.15);

    // Draw drooping camel (head lowered)
    this._px(camelRow, camelCol, this.color);
    this._px(camelRow - 1, camelCol - 1, this.color);  // drooping head

    // Sweat beads: fade in/out
    var sweatAlpha = Math.sin(t * Math.PI * 3) * 0.5 + 0.35;
    var sweat = {
      r: Math.round(0.8 * this.color.r * sweatAlpha),
      g: Math.round(0.9 * this.color.g * sweatAlpha),
      b: Math.round(this.color.b * sweatAlpha),
    };
    this._px(camelRow - 2, camelCol + 1, sweat);
    this._px(camelRow - 1, camelCol + 2, sweat);

    // Faint "behind" marker on the right (track ahead, unreachable)
    var aheadMarkerAlpha = 0.3;
    var aheadMarker = {
      r: Math.round(this.color.r * aheadMarkerAlpha),
      g: Math.round(this.color.g * aheadMarkerAlpha),
      b: Math.round(this.color.b * aheadMarkerAlpha),
    };
    for (var c = Math.round(this.cols * 0.7); c < this.cols; c++) {
      if ((c - this.cols + 1) % 2 === 0) {
        this._px(camelRow, c, aheadMarker);
      }
    }
  };

  // ── Effect: Game Start (matrix) ─────────────────────────────────────────────
  // Checkered flag and "Ready...Set...Go" sequence.

  function GameStartEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.elapsed  = 0;
    this.speed    = 400;  // ms per phase
    this.phase    = 0;    // 0=checkerboard  1=Ready  2=Set  3=Go
  }

  GameStartEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  GameStartEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    if (this.elapsed >= this.speed) {
      this.elapsed -= this.speed;
      this.phase = (this.phase + 1) % 4;
    }

    var phaseFrac = this.elapsed / this.speed;

    if (this.phase === 0) {
      // Checkered flag pattern (alternating black/white squares)
      var flagColor = { r: 255, g: 255, b: 255 };
      var bgColor = { r: 0, g: 0, b: 0 };
      for (var r = 0; r < this.rows; r++) {
        for (var c = 0; c < this.cols; c++) {
          var isCheck = (r + c) % 2 === 0;
          this._px(r, c, isCheck ? flagColor : bgColor);
        }
      }
    } else if (this.phase === 1) {
      // "Ready" — top half lit in orange
      var readyColor = { r: 255, g: 165, b: 0 };
      for (var rr = 0; rr < Math.ceil(this.rows / 2); rr++) {
        for (var cc = 0; cc < this.cols; cc++) {
          this._px(rr, cc, readyColor);
        }
      }
    } else if (this.phase === 2) {
      // "Set" — middle lit in yellow
      var setColor = { r: 255, g: 255, b: 0 };
      var midRow = Math.round(this.rows / 2);
      for (var c2 = 0; c2 < this.cols; c2++) {
        this._px(midRow - 1, c2, setColor);
        this._px(midRow, c2, setColor);
      }
    } else if (this.phase === 3) {
      // "Go!" — all green, pulsing
      var pulse = Math.sin(phaseFrac * Math.PI * 4) * 0.5 + 0.5;
      var goColor = {
        r: Math.round(0 * pulse),
        g: Math.round(255 * pulse),
        b: Math.round(0 * pulse),
      };
      for (var r3 = 0; r3 < this.rows; r3++) {
        for (var c3 = 0; c3 < this.cols; c3++) {
          this._px(r3, c3, goColor);
        }
      }
    }
  };

  // ── Effect: Game End (matrix) ───────────────────────────────────────────────
  // Finish line and game over: curtain close / fade effect.

  function GameEndEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.elapsed  = 0;
    this.duration = 800;  // total ms
  }

  GameEndEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  GameEndEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    var t = Math.min(1, this.elapsed / this.duration);

    // Curtain closes from left and right
    var curtainWidth = Math.round(t * (this.cols / 2));

    for (var r = 0; r < this.rows; r++) {
      for (var c = 0; c < this.cols; c++) {
        var distLeft = c;
        var distRight = this.cols - 1 - c;

        if (distLeft < curtainWidth || distRight < curtainWidth) {
          // Curtain color: gradient from red to black
          var shade = Math.round(255 * (1 - t * 0.8));
          this._px(r, c, { r: shade, g: Math.round(shade * 0.3), b: Math.round(shade * 0.3) });
        }
      }
    }

    // "Finish" line at center (vertical stripe)
    if (t > 0.3) {
      var finishAlpha = Math.min(1, (t - 0.3) / 0.2);
      var finishColor = {
        r: Math.round(255 * finishAlpha),
        g: Math.round(255 * finishAlpha),
        b: 0,
      };
      var midCol = Math.round(this.cols / 2);
      for (var rf = 0; rf < this.rows; rf++) {
        this._px(rf, midCol - 1, finishColor);
        this._px(rf, midCol, finishColor);
      }
    }
  };

  // ── Effect: Someone Won (matrix) ────────────────────────────────────────────
  // Winner celebration: trophy icon, confetti bursts, celebratory flashing.

  function SomeoneWonEffect(params, api) {
    this.api      = api;
    this.rows     = api.matrixRows || 8;
    this.cols     = api.matrixCols || 8;
    this.topology = api.topology || 'matrix_zigzag';
    this.color    = hexToRgb(params.color || '#FFD700');
    this.elapsed  = 0;
    this.duration = 1500;  // total ms
  }

  SomeoneWonEffect.prototype._px = function (row, col, color) {
    if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
    var idx = _matrixPixelIdx(row, col, this.rows, this.cols, this.topology);
    if (idx >= 0 && idx < this.api.ledCount) this.api.setPixel(idx, color);
  };

  SomeoneWonEffect.prototype.update = function (deltaTime) {
    this.elapsed += deltaTime;
    this.api.setAll({ r: 0, g: 0, b: 0 });

    var t = Math.min(1, this.elapsed / this.duration);

    // Trophy in center: grows and pulses
    var trophyRow = Math.round(this.rows * 0.35);
    var trophyCol = Math.round(this.cols / 2);

    var pulseSize = Math.sin(t * Math.PI * 5) * 0.3 + 0.7;
    var trophyAlpha = pulseSize;

    // Draw simple trophy: cup shape
    var cupColor = {
      r: Math.round(this.color.r * trophyAlpha),
      g: Math.round(this.color.g * trophyAlpha),
      b: Math.round(this.color.b * trophyAlpha),
    };

    this._px(trophyRow, trophyCol - 1, cupColor);
    this._px(trophyRow, trophyCol, cupColor);
    this._px(trophyRow, trophyCol + 1, cupColor);
    this._px(trophyRow + 1, trophyCol, cupColor);
    this._px(trophyRow + 2, trophyCol - 1, cupColor);
    this._px(trophyRow + 2, trophyCol, cupColor);
    this._px(trophyRow + 2, trophyCol + 1, cupColor);

    // Confetti bursts radiating from trophy
    for (var i = 0; i < 8; i++) {
      var angle = (i / 8) * Math.PI * 2 + t * Math.PI * 4;
      var cx = Math.cos(angle);
      var sy = Math.sin(angle);
      var dist = t * 3;
      var cCol = Math.round(trophyCol + cx * dist);
      var cRow = Math.round(trophyRow + sy * dist);

      var confettiAlpha = Math.max(0, 1 - t);
      var confetti = {
        r: Math.round(this.color.r * confettiAlpha * 0.8),
        g: Math.round(this.color.g * confettiAlpha * 0.6),
        b: Math.round(this.color.b * confettiAlpha * 0.4),
      };
      this._px(cRow, cCol, confetti);
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
