'use strict';

const express = require('express');

/**
 * Sound configuration routes.
 * GET  /api/sounds/config  → current resolved config for browser clients
 * POST /api/sounds/config  → replace the URL override map (admin only, thin)
 *
 * @param {import('../sound/SoundConfigManager')} soundConfigManager
 */
function createSoundsRouter(soundConfigManager) {
  const router = express.Router();

  router.get('/config', (req, res) => {
    try {
      res.json(soundConfigManager.getClientConfig());
    } catch (err) {
      res.status(500).json({ error: err.message });
    }
  });

  router.post('/config', async (req, res) => {
    try {
      const body = req.body || {};
      // Accept either the full config body or legacy { urls } shape
      await soundConfigManager.saveConfig(body);
      res.json(soundConfigManager.getClientConfig());
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  return router;
}

module.exports = createSoundsRouter;
