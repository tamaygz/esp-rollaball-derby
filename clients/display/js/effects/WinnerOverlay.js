'use strict';

/**
 * WinnerOverlay — full-screen winner celebration (REQ-007 / TASK-021).
 *
 * A Pixi Container that fills the screen with:
 *   • Semi-transparent dark veil
 *   • Animated "WINNER!" title (scale-in)
 *   • Winner player name subtitle
 *   • Confetti particle rain (gsap looping tweens)
 *
 * Usage:
 *   var overlay = new WinnerOverlay(screenW, screenH);
 *   app.stage.addChild(overlay);
 *   overlay.show('Alice');
 *   overlay.hide();
 *   overlay.resize(newW, newH);   // call on window resize
 */

/* global PIXI, gsap */

class WinnerOverlay extends PIXI.Container {
  constructor(screenW, screenH) {
    super();
    this._w        = screenW;
    this._h        = screenH;
    this._confetti = [];
    this.visible   = false;
    this.alpha     = 0;
    this._build();
  }

  // ── Build scene ─────────────────────────────────────────────────────────────

  _build() {
    // Dark veil
    this._veil = new PIXI.Graphics();
    this._veil.rect(0, 0, this._w, this._h).fill({ color: 0x000000, alpha: 0.75 });
    this.addChild(this._veil);

    // Confetti container (behind text)
    this._confettiContainer = new PIXI.Container();
    this.addChild(this._confettiContainer);

    // "WINNER!" text
    var titleSize = Math.max(48, Math.min(this._w * 0.12, 140));
    this._titleText = new PIXI.Text({
      text: 'WINNER!',
      style: new PIXI.TextStyle({
        fontFamily: '"Arial Black", Arial, sans-serif',
        fontWeight: '900',
        fontSize: titleSize,
        fill: '#FFD700',
        stroke: { color: '#000000', width: Math.max(4, titleSize * 0.06) },
        dropShadow: {
          color: '#000000',
          blur: 12,
          distance: 4,
          alpha: 0.7,
        },
      }),
    });
    this._titleText.anchor.set(0.5);
    this._titleText.x = this._w / 2;
    this._titleText.y = this._h / 2 - this._h * 0.1;
    this.addChild(this._titleText);

    // Player name subtitle
    var nameSize = Math.max(24, Math.min(this._w * 0.055, 72));
    this._nameText = new PIXI.Text({
      text: '',
      style: new PIXI.TextStyle({
        fontFamily: 'Arial, sans-serif',
        fontWeight: 'bold',
        fontSize: nameSize,
        fill: '#ffffff',
        stroke: { color: '#000000', width: Math.max(3, nameSize * 0.06) },
      }),
    });
    this._nameText.anchor.set(0.5);
    this._nameText.x = this._w / 2;
    this._nameText.y = this._h / 2 + this._h * 0.08;
    this.addChild(this._nameText);
  }

  // ── Public API ───────────────────────────────────────────────────────────────

  show(playerName) {
    this._nameText.text = playerName || '';
    this.visible = true;

    // Reset scale for title bounce-in
    this._titleText.scale.set(0.2);
    gsap.killTweensOf(this);
    gsap.killTweensOf(this._titleText.scale);

    gsap.to(this, { alpha: 1, duration: 0.4, ease: 'power2.out' });
    gsap.to(this._titleText.scale, {
      x: 1, y: 1,
      duration: 0.6,
      ease: 'elastic.out(1, 0.5)',
      delay: 0.1,
    });

    this._startConfetti();
  }

  hide() {
    gsap.killTweensOf(this);
    gsap.to(this, {
      alpha: 0,
      duration: 0.35,
      ease: 'power2.in',
      onComplete: () => { this.visible = false; },
    });
    this._stopConfetti();
  }

  resize(w, h) {
    this._w = w;
    this._h = h;
    this._stopConfetti();
    this.removeChildren();
    this._confetti = [];
    this._build();
  }

  // ── Confetti ─────────────────────────────────────────────────────────────────

  _startConfetti() {
    var COLORS = [0xFFD700, 0xFF4081, 0x00BCD4, 0x69F0AE, 0xFF6D00, 0xD500F9, 0xFF1744, 0x00E676];
    var COUNT  = 70;

    for (var i = 0; i < COUNT; i++) {
      var g     = new PIXI.Graphics();
      var color = COLORS[Math.floor(Math.random() * COLORS.length)];
      var cw    = 6 + Math.random() * 10;
      var ch    = 6 + Math.random() * 10;
      g.rect(0, 0, cw, ch).fill(color);
      g.pivot.set(cw / 2, ch / 2);
      g.x = Math.random() * this._w;
      g.y = -30 - Math.random() * this._h;

      this._confettiContainer.addChild(g);
      this._confetti.push(g);

      var startX    = g.x;
      var drift     = (Math.random() - 0.5) * 400;
      var duration  = 2.2 + Math.random() * 2.2;
      var delay     = Math.random() * 1.4;

      gsap.to(g, {
        y:        this._h + 50,
        x:        startX + drift,
        rotation: Math.random() * Math.PI * 6,
        duration: duration,
        delay:    delay,
        ease:     'none',
        repeat:   -1,
        onRepeat: (function (piece, screenW) {
          return function () {
            piece.x = Math.random() * screenW;
            piece.y = -30;
          };
        }(g, this._w)),
      });
    }
  }

  _stopConfetti() {
    for (var i = 0; i < this._confetti.length; i++) {
      gsap.killTweensOf(this._confetti[i]);
    }
    this._confetti = [];
    if (this._confettiContainer) this._confettiContainer.removeChildren();
  }
}
