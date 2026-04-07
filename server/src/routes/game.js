'use strict';

const { Router } = require('express');

/**
 * Creates a game router bound to the given gameState and connectionManager.
 */
function createGameRouter(gameState, connectionManager) {
  const router = Router();

  // GET / — full game state with connected client counts
  router.get('/', (req, res) => {
    const state = gameState.toJSON();
    state.connectedClients = connectionManager.getConnectedCounts();
    res.json(state);
  });

  // POST /start
  router.post('/start', (req, res) => {
    try {
      gameState.start();
      connectionManager.broadcastState();
      res.json({ ok: true });
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  // POST /pause
  router.post('/pause', (req, res) => {
    try {
      gameState.pause();
      connectionManager.broadcastState();
      res.json({ status: gameState.getStatus() });
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  // POST /reset
  router.post('/reset', (req, res) => {
    gameState.reset();
    connectionManager.broadcastState();
    connectionManager.broadcastPositions();
    res.json({ ok: true });
  });

  // PUT /config
  router.put('/config', (req, res) => {
    try {
      const updated = gameState.updateConfig(req.body);
      connectionManager.broadcastState();
      res.json(updated);
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  return router;
}

module.exports = createGameRouter;
