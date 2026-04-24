'use strict';

const express = require('express');
const router = express.Router();

// ─── Redirects for old .html bookmarks (source files no longer exist, EJS templates serve these routes) ───
router.get('/index.html',        (req, res) => res.redirect(301, '/admin'));
router.get('/devices.html',      (req, res) => res.redirect(301, '/admin/devices'));
router.get('/leds.html',         (req, res) => res.redirect(301, '/admin/leds'));
router.get('/debug-player.html', (req, res) => res.redirect(301, '/admin/debug-player'));

// ─── Admin page routes ─────────────────────────────────────────────────────────
router.get('/', (req, res) => {
  res.render('admin/index', {
    title: 'Roll-a-Ball Derby — Admin',
    active: 'admin',
    wsStatus: true,
  });
});

router.get('/devices', (req, res) => {
  res.render('admin/devices', {
    title: 'Roll-a-Ball Derby — Devices',
    active: 'devices',
    wsStatus: false,
  });
});

router.get('/leds', (req, res) => {
  res.render('admin/leds', {
    title: 'Roll-a-Ball Derby — LEDs',
    active: 'leds',
    wsStatus: true,
  });
});

router.get('/debug-player', (req, res) => {
  res.render('admin/debug-player', {
    title: 'Roll-a-Ball Derby — Debug Player',
    active: 'debug-player',
    wsStatus: true,
  });
});

router.get('/logs', (req, res) => {
  res.render('admin/logs', {
    title: 'Roll-a-Ball Derby — Live Log',
    active: 'logs',
    wsStatus: true,
  });
});

router.get('/sounds', (req, res) => {
  res.render('admin/sounds', {
    title: 'Roll-a-Ball Derby — Sounds',
    active: 'sounds',
    wsStatus: false,
  });
});

module.exports = router;
