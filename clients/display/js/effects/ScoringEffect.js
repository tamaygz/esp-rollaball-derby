'use strict';

/**
 * ScoringEffect — visual feedback when a player scores (REQ-006 / TASK-020).
 *
 * scoringFlash(container)
 *   Applies a scale-bounce (grow then spring back) and a brief white flash
 *   to any Pixi DisplayObject via gsap.
 *
 * Relies on the global `gsap` object loaded from CDN.
 */
var ScoringEffect = (function () {

  /**
   * @param {PIXI.Container} container  The player figure container to animate.
   */
  function scoringFlash(container) {
    if (!container) return;

    // Kill any in-progress tween on this container's scale to avoid conflicts
    gsap.killTweensOf(container.scale);

    // Scale-bounce: grow quickly then spring back elastically
    gsap.timeline()
      .to(container.scale, {
        x: 1.45,
        y: 1.45,
        duration: 0.12,
        ease: 'power2.out',
      })
      .to(container.scale, {
        x: 1,
        y: 1,
        duration: 0.45,
        ease: 'elastic.out(1, 0.35)',
      });

    // Brief white flash by temporarily forcing full tint then restoring
    var originalTint = container.tint !== undefined ? container.tint : 0xffffff;
    container.tint = 0xffffff;
    setTimeout(function () {
      if (container && !container.destroyed) {
        container.tint = originalTint;
      }
    }, 130);
  }

  return { scoringFlash: scoringFlash };
}());
