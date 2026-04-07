'use strict';

/* global Derby */
window.Derby = window.Derby || {};

/**
 * Derby.ledMain — entry point for the LED admin page (leds.html).
 * Minimal bootstrap: connects WebSocket and routes only LED-relevant messages.
 */
(function () {
  function _el(id) { return document.getElementById(id); }

  // ── Message router (LED page only) ──────────────────────────────────────

  Derby.Connection.onMessage(function (msg) {
    switch (msg.type) {
      case 'registered':
        break;

      case 'state':
        if (Derby.LED) Derby.LED.updateDeviceList(msg.payload.devices || []);
        break;

      default:
        break;
    }
  });

  // ── Init LED module ───────────────────────────────────────────────────────

  try {
    if (Derby.LED) Derby.LED.init();
  } catch (e) {
    console.error('[Derby] LED init failed:', e);
  }

  // ── Connect (no player name needed — LED page is admin-only) ──────────────

  Derby.Connection.connect('');
}());
