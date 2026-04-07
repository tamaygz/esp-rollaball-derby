'use strict';

/**
 * Lane — a single player's racing lane (REQ-004 / TASK-008).
 *
 * A PIXI.Container containing:
 *   • Alternating background band (dark/slightly-lighter per even/odd lane)
 *   • Track strip (turf/sand strip in the middle band)
 *   • Start marker (thin vertical line at left edge of track)
 *   • Finish line (checkered or pennant pattern at right edge)
 *   • Player figure (colored rounded-rectangle silhouette, tinted with player color)
 *   • Player name label (left-aligned, vertically centered)
 *   • Disconnected overlay (semi-transparent black veil when player is offline)
 *
 * Usage:
 *   var lane = new Lane(player, colorIndex, laneIndex, laneWidth, laneHeight, theme);
 *   raceTrack.addChild(lane);
 *   lane.updatePosition(5, 15);   // position=5 out of trackLength=15
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
    this._figureContainer = null; // the tweened figure wrapper
    this._build();
  }

  // ── Build / rebuild ─────────────────────────────────────────────────────────

  _build() {
    var w = this._w;
    var h = this._h;

    // ── Layout metrics ──────────────────────────────────────────────────────
    var namePad     = 8;
    var nameAreaW   = Math.max(90, w * 0.14);
    var trackStartX = nameAreaW + 10;
    var trackEndX   = w - 24;
    var trackH      = h * 0.42;
    var trackY      = (h - trackH) / 2;
    var playerColor = ThemeManager.getPlayerColor(this._colorIndex);

    // ── Background band ─────────────────────────────────────────────────────
    var bgColor = this._laneIndex % 2 === 0 ? 0x111118 : 0x18181f;
    this._bg = new PIXI.Graphics();
    this._bg.rect(0, 0, w, h).fill(bgColor);
    this.addChild(this._bg);

    // Thin separator line at top of lane
    var sep = new PIXI.Graphics();
    sep.rect(0, 0, w, 1).fill({ color: 0x333340, alpha: 0.9 });
    this.addChild(sep);

    // ── Track strip ─────────────────────────────────────────────────────────
    // Use theme palette if available, else defaults
    var palette  = ThemeManager.palette || {};
    var trackCol = palette.track
      ? parseInt(palette.track.replace('#', ''), 16)
      : 0x2a5010;

    var track = new PIXI.Graphics();
    track.roundRect(trackStartX, trackY, trackEndX - trackStartX, trackH, 4).fill(trackCol);
    this.addChild(track);

    // ── Start marker ────────────────────────────────────────────────────────
    var startMark = new PIXI.Graphics();
    startMark.rect(trackStartX, trackY, 3, trackH).fill({ color: 0xffffff, alpha: 0.35 });
    this.addChild(startMark);

    // ── Finish line (checkered) ─────────────────────────────────────────────
    var finCol  = palette.finishLine
      ? parseInt(palette.finishLine.replace('#', ''), 16)
      : 0xffffff;
    var flX  = trackEndX - 14;
    var rows = 8;
    var rh   = trackH / rows;
    var fl   = new PIXI.Graphics();
    for (var i = 0; i < rows; i++) {
      var colA = i % 2 === 0 ? finCol       : 0x000000;
      var colB = i % 2 === 0 ? 0x000000     : finCol;
      fl.rect(flX,     trackY + i * rh, 7, rh).fill(colA);
      fl.rect(flX + 7, trackY + i * rh, 7, rh).fill(colB);
    }
    this.addChild(fl);

    // ── Player figure ────────────────────────────────────────────────────────
    // Figure is a container so we can gsap.to(container) for x-tweening and
    // scale-bounce without interfering with Pixi's internal transform.
    this._figureContainer = new PIXI.Container();
    this._figureContainer.y = h / 2;

    var figureH   = Math.max(20, trackH * 0.88);
    var aspect    = ThemeManager.getSpriteAspect();
    var figureW   = figureH * aspect;
    var figure    = this._drawFigure(figureW, figureH, playerColor);
    figure.pivot.set(figureW / 2, figureH / 2);
    this._figureContainer.addChild(figure);
    this._figureContainer.x = trackStartX; // start position
    this.addChild(this._figureContainer);

    // ── Player name label ───────────────────────────────────────────────────
    var fontSize = Math.max(11, Math.min(h * 0.32, 28));
    this._label = new PIXI.Text({
      text: this._player.name,
      style: new PIXI.TextStyle({
        fontFamily: 'Arial, sans-serif',
        fontWeight: 'bold',
        fontSize: fontSize,
        fill: '#ffffff',
        stroke: { color: '#000000', width: Math.max(2, fontSize * 0.12) },
        wordWrap: false,
      }),
    });
    this._label.x = namePad;
    this._label.y = (h - this._label.height) / 2;
    // Clip label to name area width
    if (this._label.width > nameAreaW - namePad * 2) {
      this._label.scale.x = (nameAreaW - namePad * 2) / this._label.width;
    }
    this.addChild(this._label);

    // ── Disconnected veil ───────────────────────────────────────────────────
    this._veil = new PIXI.Graphics();
    this._veil.rect(0, 0, w, h).fill({ color: 0x000000, alpha: 0.55 });
    this._veil.visible = !this._player.connected;
    this.addChild(this._veil);
  }

  // ── Figure drawing ───────────────────────────────────────────────────────────

  _drawFigure(fw, fh, color) {
    var g   = new PIXI.Graphics();
    var themeId = ThemeManager.id;

    if (themeId === 'camel') {
      this._drawCamelFigure(g, fw, fh, color);
    } else {
      this._drawHorseFigure(g, fw, fh, color);
    }

    return g;
  }

  _drawHorseFigure(g, fw, fh, color) {
    // Body
    g.ellipse(fw * 0.42, fh * 0.55, fw * 0.35, fh * 0.22).fill(color);
    // Neck
    g.ellipse(fw * 0.68, fh * 0.42, fw * 0.1, fh * 0.18).fill(color);
    // Head
    g.ellipse(fw * 0.82, fh * 0.32, fw * 0.13, fh * 0.14).fill(color);
    // Tail
    g.ellipse(fw * 0.1, fh * 0.5, fw * 0.08, fh * 0.16).fill(color);
    // Legs — front pair
    g.roundRect(fw * 0.6, fh * 0.68, fw * 0.07, fh * 0.32, 3).fill(color);
    g.roundRect(fw * 0.72, fh * 0.68, fw * 0.07, fh * 0.32, 3).fill(color);
    // Legs — rear pair
    g.roundRect(fw * 0.2, fh * 0.68, fw * 0.07, fh * 0.32, 3).fill(color);
    g.roundRect(fw * 0.32, fh * 0.68, fw * 0.07, fh * 0.32, 3).fill(color);
    // Jockey body
    g.ellipse(fw * 0.72, fh * 0.3, fw * 0.12, fh * 0.14).fill({ color: color, alpha: 0.85 });
    // Jockey head
    g.circle(fw * 0.82, fh * 0.16, fh * 0.1).fill(color);
  }

  _drawCamelFigure(g, fw, fh, color) {
    // Body
    g.ellipse(fw * 0.44, fh * 0.58, fw * 0.38, fh * 0.22).fill(color);
    // Hump 1
    g.ellipse(fw * 0.32, fh * 0.36, fw * 0.14, fh * 0.2).fill(color);
    // Hump 2
    g.ellipse(fw * 0.55, fh * 0.33, fw * 0.12, fh * 0.18).fill(color);
    // Neck
    g.ellipse(fw * 0.74, fh * 0.45, fw * 0.09, fh * 0.2).fill(color);
    // Head
    g.ellipse(fw * 0.84, fh * 0.32, fw * 0.12, fh * 0.13).fill(color);
    // Tail
    g.ellipse(fw * 0.1, fh * 0.55, fw * 0.07, fh * 0.14).fill(color);
    // Four legs
    g.roundRect(fw * 0.2, fh * 0.7, fw * 0.07, fh * 0.3, 3).fill(color);
    g.roundRect(fw * 0.33, fh * 0.7, fw * 0.07, fh * 0.3, 3).fill(color);
    g.roundRect(fw * 0.57, fh * 0.7, fw * 0.07, fh * 0.3, 3).fill(color);
    g.roundRect(fw * 0.7, fh * 0.7, fw * 0.07, fh * 0.3, 3).fill(color);
    // Rider
    g.circle(fw * 0.44, fh * 0.14, fh * 0.09).fill(color);
    g.ellipse(fw * 0.44, fh * 0.25, fw * 0.1, fh * 0.12).fill({ color: color, alpha: 0.85 });
  }

  // ── Public API ───────────────────────────────────────────────────────────────

  /**
   * Tween the player figure to its new proportional position.
   * @param {number} position     Current player position (0 … trackLength)
   * @param {number} trackLength  Total track length from game config
   */
  updatePosition(position, trackLength) {
    if (!this._figureContainer) return;
    var nameAreaW  = Math.max(90, this._w * 0.14);
    var trackStartX = nameAreaW + 10;
    var trackEndX   = this._w - 24;
    var usable      = trackEndX - trackStartX;
    var ratio       = trackLength > 0 ? Math.min(position / trackLength, 1) : 0;
    var targetX     = trackStartX + ratio * usable;

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

  /** Rebuild after screen resize. Preserves current tween position. */
  resize(laneWidth, laneHeight) {
    // Capture current x ratio before rebuild
    var nameAreaW   = Math.max(90, this._w * 0.14);
    var trackStartX = nameAreaW + 10;
    var trackEndX   = this._w - 24;
    var usable      = trackEndX - trackStartX;
    var curRatio    = this._figureContainer
      ? (this._figureContainer.x - trackStartX) / (usable || 1)
      : 0;

    this._w = laneWidth;
    this._h = laneHeight;
    this.removeChildren();
    this._figureContainer = null;
    this._build();

    // Restore position without tween
    if (this._figureContainer) {
      var naw   = Math.max(90, laneWidth * 0.14);
      var nStart = naw + 10;
      var nEnd   = laneWidth - 24;
      this._figureContainer.x = nStart + curRatio * (nEnd - nStart);
    }
  }

  get playerId() { return this._player.id; }
}
