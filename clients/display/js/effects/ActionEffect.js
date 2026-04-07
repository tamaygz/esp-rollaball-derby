'use strict';

/**
 * ActionEffect — visual effects for all game action events (TASK-030).
 *
 * showEffect(lane, events)
 *   Triggers Pixi animations on a Lane for the given server-emitted events.
 *
 * Event types and their effects:
 *   zero_roll       — rolled 0 pts  → 😢 popup + red flash + shrink-bounce
 *   score_1         — scored +1     → scale bounce + white flash
 *   score_3         — scored +3     → bigger bounce + gold flash + ⭐ "+3!" popup
 *   streak_zero_3x  — 0 × 3 in row → 😭 popup + dark pulse + "3× ZERO" label
 *   streak_three_2x — +3 × 2 in row→ 🔥 popup + orange glow + "HOT!" label
 *   took_lead       — overtook all  → 👑 popup + gold aura + "LEAD!" label
 *   became_last     — dropped last  → 👎 popup + dark-red flash + "LAST!" label
 *
 * Lane interface used:
 *   lane._figureContainer  — PIXI.Container for the player sprite
 *   lane._flashOverlay     — full-lane PIXI.Graphics for color flashes
 *   lane._popupContainer   — PIXI.Container for floating emoji/text
 *   lane._h                — lane height (px)
 *
 * Depends on: PIXI (global), gsap (global)
 */

/* global PIXI, gsap */

var ActionEffect = (function () {

  var POPUP_RISE     = 80;   // px to float upward
  var POPUP_DURATION = 1.4;  // seconds for full pop-up animation

  // ── Internal helpers ──────────────────────────────────────────────────────

  /**
   * Float an emoji + optional label above the player figure.
   * @param {object} lane
   * @param {string} emoji
   * @param {string} label  short text below emoji (may be '')
   * @param {string} color  CSS hex color string, e.g. '#FF4444'
   * @param {number} sizeScale  1 = normal
   */
  function _popup(lane, emoji, label, color, sizeScale) {
    if (!lane._popupContainer || !lane._figureContainer) return;

    var figX  = lane._figureContainer.x;
    var figY  = lane._figureContainer.y;
    var size  = Math.max(24, lane._h * 0.46) * (sizeScale || 1);

    var t = new PIXI.Text({
      text: label ? emoji + '\n' + label : emoji,
      style: new PIXI.TextStyle({
        fontFamily:  '"Segoe UI Emoji", "Apple Color Emoji", Arial, sans-serif',
        fontSize:    size,
        align:       'center',
        fill:        color,
        stroke:      { color: '#000000', width: Math.max(2, size * 0.06) },
        lineHeight:  size * 1.05,
        dropShadow:  {
          color:    '#000000',
          blur:     6,
          distance: 2,
          alpha:    0.65,
        },
      }),
    });

    t.anchor.set(0.5, 1);
    t.x     = figX;
    t.y     = figY - lane._h * 0.05;
    t.alpha = 0;

    lane._popupContainer.addChild(t);

    gsap.timeline({
      onComplete: function () {
        if (t && !t.destroyed) t.destroy();
      },
    })
      .to(t, { alpha: 1, y: t.y - POPUP_RISE * 0.15, duration: 0.12, ease: 'power2.out' })
      .to(t, { alpha: 0, y: t.y - POPUP_RISE, duration: POPUP_DURATION, ease: 'power1.out' }, 0.05);
  }

  /**
   * Flash the full-lane color overlay.
   * @param {object} lane
   * @param {number} color     hex color  (e.g. 0xAA0000)
   * @param {number} maxAlpha  0–1
   * @param {number} duration  fade-out seconds
   */
  function _laneFlash(lane, color, maxAlpha, duration) {
    if (!lane._flashOverlay) return;
    var flash = lane._flashOverlay;
    gsap.killTweensOf(flash);
    flash.tint    = color;
    flash.alpha   = 0;
    flash.visible = true;
    gsap.timeline({
      onComplete: function () { flash.visible = false; },
    })
      .to(flash, { alpha: maxAlpha,  duration: 0.10,            ease: 'power2.out' })
      .to(flash, { alpha: 0,         duration: duration || 0.55, ease: 'power2.in' });
  }

  /**
   * Scale-bounce the figure container.
   * @param {object} lane
   * @param {number} peak  peak scale (1.0 = no change)
   */
  function _scaleBounce(lane, peak) {
    if (!lane._figureContainer) return;
    var c = lane._figureContainer;
    gsap.killTweensOf(c.scale);
    gsap.timeline()
      .to(c.scale, { x: peak, y: peak, duration: 0.12, ease: 'power2.out' })
      .to(c.scale, { x: 1,    y: 1,    duration: 0.50, ease: 'elastic.out(1, 0.35)' });
  }

  /**
   * Brief tint override on the sprite inside the figure container.
   * @param {object} lane
   * @param {number} flashColor  hex color
   * @param {number} ms          duration
   */
  function _tintFlash(lane, flashColor, ms) {
    if (!lane._figureContainer || !lane._figureContainer.children[0]) return;
    var sprite = lane._figureContainer.children[0];
    var orig   = sprite.tint;
    sprite.tint = flashColor;
    setTimeout(function () {
      if (sprite && !sprite.destroyed) sprite.tint = orig;
    }, ms || 160);
  }

  // ── Per-event effect functions ─────────────────────────────────────────────

  function _fxZeroRoll(lane) {
    _scaleBounce(lane, 0.72);                      // shrink-dejection
    _tintFlash(lane, 0xFF4444, 220);
    _laneFlash(lane, 0xAA0000, 0.35, 0.65);
    _popup(lane, '😢', '', '#FF4444', 1.0);
  }

  function _fxScore1(lane) {
    _scaleBounce(lane, 1.45);
    _tintFlash(lane, 0xFFFFFF, 130);
  }

  function _fxScore2(lane) {
    _scaleBounce(lane, 1.62);
    _tintFlash(lane, 0xFFFFFF, 150);
    _laneFlash(lane, 0x88CCFF, 0.18, 0.5);
  }

  function _fxScore3(lane) {
    _scaleBounce(lane, 1.85);
    _tintFlash(lane, 0xFFD700, 200);
    _laneFlash(lane, 0xFFD700, 0.28, 0.65);
    _popup(lane, '⭐', '+3!', '#FFD700', 1.0);
  }

  function _fxStreakZero3x(lane) {
    _laneFlash(lane, 0x440000, 0.72, 1.1);
    _popup(lane, '😭', '3× ZERO', '#FF2222', 1.4);
  }

  function _fxStreakThree2x(lane) {
    _laneFlash(lane, 0xFF6D00, 0.38, 0.85);
    _popup(lane, '🔥', 'HOT!', '#FF6D00', 1.2);
  }

  function _fxTookLead(lane) {
    _laneFlash(lane, 0xFFD700, 0.32, 0.90);
    _popup(lane, '👑', 'LEAD!', '#FFD700', 1.2);
  }

  function _fxBecameLast(lane) {
    _laneFlash(lane, 0x880000, 0.45, 0.80);
    _popup(lane, '👎', 'LAST!', '#FF2222', 1.0);
  }

  // ── Public API ─────────────────────────────────────────────────────────────

  /**
   * Dispatch all visual effects for the given event array onto the given lane.
   * Streak / rank events are delayed slightly so their pop-ups layer on top of
   * the base score effect.
   *
   * @param {object}   lane    Lane instance (exposes _figureContainer etc.)
   * @param {string[]} events  Event strings from the server scored message.
   */
  function showEffect(lane, events) {
    if (!lane || !events || !events.length) return;

    var hasZeroRoll      = events.indexOf('zero_roll')       >= 0;
    var hasScore1        = events.indexOf('score_1')         >= 0;
    var hasScore2        = events.indexOf('score_2')         >= 0;
    var hasScore3        = events.indexOf('score_3')         >= 0;
    var hasStreakZero3x  = events.indexOf('streak_zero_3x')  >= 0;
    var hasStreakThree2x = events.indexOf('streak_three_2x') >= 0;
    var hasTookLead      = events.indexOf('took_lead')       >= 0;
    var hasBecameLast    = events.indexOf('became_last')     >= 0;

    // Base effects fire immediately
    if (hasZeroRoll) _fxZeroRoll(lane);
    if (hasScore1)   _fxScore1(lane);
    if (hasScore2)   _fxScore2(lane);
    if (hasScore3)   _fxScore3(lane);

    // Streak effects fire after base (so their pop-ups appear on top)
    if (hasStreakZero3x)  setTimeout(function () { _fxStreakZero3x(lane); },  150);
    if (hasStreakThree2x) setTimeout(function () { _fxStreakThree2x(lane); }, 200);

    // Rank effects fire last (most visually prominent)
    if (hasTookLead)   setTimeout(function () { _fxTookLead(lane); },   300);
    if (hasBecameLast) setTimeout(function () { _fxBecameLast(lane); }, 300);
  }

  return { showEffect: showEffect };

}());
