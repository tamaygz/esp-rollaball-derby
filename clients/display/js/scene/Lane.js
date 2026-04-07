'use strict';

/**
 * Lane — a single player's racing lane (REQ-004 / TASK-008).
 *
 * A PIXI.Container containing (bottom → top):
 *   0. Track background — full-lane themed SVG (sky, landscape, turf)
 *   1. Name panel — semi-transparent dark overlay + player color swatch
 *   2. Start marker line
 *   3. Finish flag — SVG sprite at right edge
 *   4. Player figure — tinted white SVG sprite, tweened along track
 *   5. Name text label
 *   6. Lane separator
 *   7. Disconnected veil
 *
 * Usage:
 *   var lane = new Lane(player, colorIndex, laneIndex, laneWidth, laneHeight);
 *   raceTrack.addChild(lane);
 *   lane.updatePosition(5, 15);
 *   lane.setConnected(false);
 *   lane.resize(newW, newH);
 */

/* global PIXI, gsap, ThemeManager, ScoringEffect */

class Lane extends PIXI.Container {
  constructor(player, colorIndex, laneIndex, laneWidth, laneHeight) {
    super();
    this._player     = player;
    this._colorIndex = colorIndex;
    this._laneIndex  = laneIndex;
    this._w          = laneWidth;
    this._h          = laneHeight;
    this._figureContainer = null;
    this._build();
  }

  // ── Layout metrics (shared by _build, updatePosition, resize) ───────────────

  _metrics() {
    var w          = this._w;
    var h          = this._h;
    var nameAreaW  = Math.max(110, w * 0.13);
    // Finish flag aspect ratio — derived from the loaded texture once available,
    // falling back to 52/84 (the horse finish-flag.svg viewBox: 0 0 52 84).
    var DEFAULT_FLAG_ASPECT = 52 / 84; // ≈ 0.619
    var flagTex    = ThemeManager.getTexture('finishFlag');
    var flagAspect = (flagTex && flagTex.width && flagTex.height)
      ? flagTex.width / flagTex.height
      : DEFAULT_FLAG_ASPECT;
    var flagH      = h * 0.92;
    var flagW      = Math.max(24, flagH * flagAspect);
    var trackStartX = nameAreaW + 6;
    var trackEndX   = w - flagW - 8;
    var groundY    = h * 0.90;   // where figure bottom anchors (on the turf surface)
    return { nameAreaW, flagW, flagH, trackStartX, trackEndX, groundY };
  }

  // ── Build / rebuild ─────────────────────────────────────────────────────────

  _build() {
    var w           = this._w;
    var h           = this._h;
    var m           = this._metrics();
    var playerColor = ThemeManager.getPlayerColor(this._colorIndex);

    // ── 0. Full-lane track background SVG ────────────────────────────────────
    var trackBgTex     = ThemeManager.getTexture('trackBg');
    this._bgSprite     = new PIXI.Sprite(trackBgTex);
    this._bgSprite.width  = w;
    this._bgSprite.height = h;
    this.addChild(this._bgSprite);

    // ── 1. Name panel — dark semi-transparent overlay + color swatch ─────────
    var namePanel = new PIXI.Graphics();
    namePanel.rect(0, 0, m.nameAreaW, h).fill({ color: 0x000000, alpha: 0.62 });
    this.addChild(namePanel);

    // Subtle player-color wash behind name panel
    var colorWash = new PIXI.Graphics();
    colorWash.rect(0, 0, m.nameAreaW, h).fill({ color: playerColor, alpha: 0.18 });
    this.addChild(colorWash);

    // Color swatch bar — right edge of name panel (clear player identity at a glance)
    var swatch = new PIXI.Graphics();
    swatch.rect(m.nameAreaW - 7, 0, 7, h).fill({ color: playerColor, alpha: 1.0 });
    this.addChild(swatch);

    // ── 2. Start marker ───────────────────────────────────────────────────────
    var startMark = new PIXI.Graphics();
    startMark.rect(m.trackStartX, 0, 2, h).fill({ color: 0xffffff, alpha: 0.22 });
    this.addChild(startMark);

    // ── 3. Finish flag SVG sprite ─────────────────────────────────────────────
    var flagTex    = ThemeManager.getTexture('finishFlag');
    var flagSprite = new PIXI.Sprite(flagTex);
    flagSprite.width  = m.flagW;
    flagSprite.height = m.flagH;
    flagSprite.x = w - m.flagW - 4;
    flagSprite.y = h - m.flagH;   // pole base sits at lane bottom
    this.addChild(flagSprite);

    // ── 4. Player figure — tinted white SVG sprite ────────────────────────────
    // Aspect ratio comes from the loaded texture (consistent with flagAspect approach above).
    // Falls back to ThemeManager.getSpriteAspect() (parsed from theme.json viewBox) when
    // the texture is not yet available — both methods yield the same ratio.
    var spriteTex  = ThemeManager.getTexture('sprite');
    var aspect     = (spriteTex && spriteTex.width && spriteTex.height)
      ? spriteTex.width / spriteTex.height
      : ThemeManager.getSpriteAspect();
    // Figure is tall — 65% of lane height.  Bottom anchored on the turf ground line.
    var figureH    = Math.max(40, h * 0.65);
    var figureW    = figureH * aspect;

    var figureSprite  = new PIXI.Sprite(spriteTex);
    figureSprite.width  = figureW;
    figureSprite.height = figureH;
    figureSprite.tint   = playerColor;
    figureSprite.anchor.set(0.5, 1.0);   // pivot at bottom-center

    this._figureContainer = new PIXI.Container();
    this._figureContainer.addChild(figureSprite);
    this._figureContainer.x = m.trackStartX;
    this._figureContainer.y = m.groundY;
    this.addChild(this._figureContainer);

    // ── 5. Player name label ──────────────────────────────────────────────────
    var fontSize = Math.max(12, Math.min(h * 0.24, 30));
    this._label = new PIXI.Text({
      text: this._player.name,
      style: new PIXI.TextStyle({
        fontFamily: '"Arial Black", Arial, sans-serif',
        fontWeight: '900',
        fontSize: fontSize,
        fill: '#ffffff',
        stroke: { color: '#000000', width: Math.max(2, fontSize * 0.1) },
        wordWrap: false,
      }),
    });
    this._label.x = 8;
    this._label.y = (h - this._label.height) / 2;
    if (this._label.width > m.nameAreaW - 20) {
      this._label.scale.x = (m.nameAreaW - 20) / this._label.width;
    }
    this.addChild(this._label);

    // ── 6. Lane separator line ────────────────────────────────────────────────
    var sep = new PIXI.Graphics();
    sep.rect(0, 0, w, 2).fill({ color: 0x000000, alpha: 0.55 });
    this.addChild(sep);

    // ── 7. Disconnected veil ──────────────────────────────────────────────────
    this._veil = new PIXI.Graphics();
    this._veil.rect(0, 0, w, h).fill({ color: 0x000000, alpha: 0.55 });
    this._veil.visible = !this._player.connected;
    this.addChild(this._veil);
  }

  // ── Public API ───────────────────────────────────────────────────────────────

  /**
   * Tween the player figure to its new proportional position.
   * @param {number} position     Current player position (0 … trackLength)
   * @param {number} trackLength  Total track length from game config
   */
  updatePosition(position, trackLength) {
    if (!this._figureContainer) return;
    var m      = this._metrics();
    var usable = m.trackEndX - m.trackStartX;
    var ratio  = trackLength > 0 ? Math.min(position / trackLength, 1) : 0;
    var targetX = m.trackStartX + ratio * usable;

    gsap.to(this._figureContainer, {
      x: targetX,
      duration: 0.45,
      ease: 'power2.out',
    });
  }

  /** Flash + bounce the figure (called on scored event). */
  triggerScoringEffect() {
    ScoringEffect.scoringFlash(this._figureContainer);
  }

  /** Show/hide the disconnected veil. */
  setConnected(connected) {
    if (this._veil) this._veil.visible = !connected;
    if (this._figureContainer) this._figureContainer.alpha = connected ? 1 : 0.45;
  }

  /** Rebuild after screen resize. Preserves current proportional position. */
  resize(laneWidth, laneHeight) {
    var m      = this._metrics();
    var usable = m.trackEndX - m.trackStartX;
    var curRatio = (this._figureContainer && usable > 0)
      ? (this._figureContainer.x - m.trackStartX) / usable
      : 0;

    this._w = laneWidth;
    this._h = laneHeight;
    this.removeChildren();
    this._figureContainer = null;
    this._build();

    if (this._figureContainer) {
      var nm      = this._metrics();
      var nUsable = nm.trackEndX - nm.trackStartX;
      this._figureContainer.x = nm.trackStartX + curRatio * nUsable;
    }
  }

  get playerId() { return this._player.id; }
}
