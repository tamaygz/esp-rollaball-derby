'use strict';

/**
 * CountdownEffect — full-screen pre-race countdown (3, 2, 1, GO!).
 *
 * A Pixi Container that takes over the screen with:
 *   • Semi-transparent dark veil
 *   • Spotlight circle behind the number
 *   • Big animated digit (slam-in from oversized scale)
 *   • Expanding ring pulse on each number change
 *   • "GO!" in green with a punch-in effect on count = 0
 *
 * Usage:
 *   var effect = new CountdownEffect(screenW, screenH);
 *   app.stage.addChild(effect);
 *   effect.show(3);   // show "3"
 *   effect.show(0);   // show "GO!"
 *   effect.hide();    // fade out
 *   effect.resize(w, h);
 */

/* global PIXI, gsap */

class CountdownEffect extends PIXI.Container {
  constructor(screenW, screenH) {
    super();
    this._w = screenW;
    this._h = screenH;
    this.visible = false;
    this.alpha = 0;
    this._build();
  }

  // ── Build scene ─────────────────────────────────────────────────────────────

  _build() {
    var cx = this._w / 2;
    var cy = this._h / 2;
    var r  = Math.min(this._w, this._h) * 0.30;
    this._baseRadius = r;

    // Dark veil
    this._veil = new PIXI.Graphics();
    this._veil.rect(0, 0, this._w, this._h).fill({ color: 0x000000, alpha: 0.88 });
    this.addChild(this._veil);

    // Spotlight backing circle
    this._spot = new PIXI.Graphics();
    this._spot.circle(cx, cy, r).fill({ color: 0x0d0d0d, alpha: 1 });
    this._spot.circle(cx, cy, r).stroke({ color: 0x333333, width: 3, alpha: 1 });
    this.addChild(this._spot);

    // Expanding ring (cleared & redrawn on each count)
    this._ring = new PIXI.Graphics();
    this._ring.x = cx;
    this._ring.y = cy;
    this.addChild(this._ring);

    // Number text for digits 1–N — gold
    var fs = Math.max(100, Math.min(this._w, this._h) * 0.34);
    this._numText = new PIXI.Text({
      text: '',
      style: new PIXI.TextStyle({
        fontFamily: '"Arial Black", "Impact", Arial, sans-serif',
        fontWeight: '900',
        fontSize: fs,
        fill: '#FFD700',
        stroke: { color: '#000000', width: Math.max(6, fs * 0.05) },
        dropShadow: { color: '#000000', blur: 18, distance: 0, alpha: 0.85 },
      }),
    });
    this._numText.anchor.set(0.5);
    this._numText.position.set(cx, cy);
    this.addChild(this._numText);

    // "GO!" text — separate object for reliable green color
    var goFs = Math.max(80, Math.min(this._w, this._h) * 0.26);
    this._goText = new PIXI.Text({
      text: 'GO!',
      style: new PIXI.TextStyle({
        fontFamily: '"Arial Black", "Impact", Arial, sans-serif',
        fontWeight: '900',
        fontSize: goFs,
        fill: '#00E676',
        stroke: { color: '#003300', width: Math.max(5, goFs * 0.05) },
        dropShadow: { color: '#00E676', blur: 28, distance: 0, alpha: 0.55 },
      }),
    });
    this._goText.anchor.set(0.5);
    this._goText.position.set(cx, cy);
    this._goText.visible = false;
    this.addChild(this._goText);
  }

  // ── Public API ───────────────────────────────────────────────────────────────

  show(count) {
    var isGo = (count === 0);

    // Kill any in-flight tweens
    gsap.killTweensOf(this._numText);
    gsap.killTweensOf(this._numText.scale);
    gsap.killTweensOf(this._goText);
    gsap.killTweensOf(this._goText.scale);
    gsap.killTweensOf(this._ring);
    gsap.killTweensOf(this._ring.scale);

    // Toggle which text is active
    this._numText.visible = !isGo;
    this._goText.visible  = isGo;

    if (!isGo) {
      this._numText.text = String(count);
    }

    // Fade the overlay in the first time it appears
    if (!this.visible) {
      this.alpha = 0;
      this.visible = true;
      gsap.to(this, { alpha: 1, duration: 0.2, ease: 'power2.out' });
    }

    var target = isGo ? this._goText : this._numText;

    if (isGo) {
      // "GO!" punches in from tiny scale with a big back-bounce
      target.scale.set(0.15);
      target.alpha = 1;
      gsap.to(target.scale, { x: 1, y: 1, duration: 0.55, ease: 'back.out(3.5)' });
    } else {
      // Number slams in from oversized scale
      target.scale.set(2.4);
      target.alpha = 1;
      gsap.to(target.scale, { x: 1, y: 1, duration: 0.32, ease: 'back.out(1.4)' });

      // Expanding ring radiates outward and fades
      this._ring.clear();
      this._ring.circle(0, 0, this._baseRadius).stroke({ color: 0xFFD700, width: 5, alpha: 1 });
      this._ring.scale.set(1);
      this._ring.alpha = 1;
      gsap.to(this._ring.scale, { x: 3.0, y: 3.0, duration: 0.95, ease: 'power1.out' });
      gsap.to(this._ring, { alpha: 0, duration: 0.95, ease: 'power1.out' });
    }
  }

  hide() {
    gsap.killTweensOf(this);
    gsap.killTweensOf(this._numText);
    gsap.killTweensOf(this._numText.scale);
    gsap.killTweensOf(this._goText);
    gsap.killTweensOf(this._goText.scale);
    gsap.to(this, {
      alpha: 0,
      duration: 0.4,
      ease: 'power2.in',
      onComplete: () => { this.visible = false; },
    });
  }

  resize(w, h) {
    this._w = w;
    this._h = h;
    gsap.killTweensOf(this._numText);
    gsap.killTweensOf(this._numText.scale);
    gsap.killTweensOf(this._goText);
    gsap.killTweensOf(this._goText.scale);
    gsap.killTweensOf(this._ring);
    this.removeChildren();
    this._build();
  }
}
