'use strict';

const { Router } = require('express');

/**
 * Creates a game router bound to the given gameState, connectionManager, and botManager.
 */
function createGameRouter(gameState, connectionManager, botManager) {
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
      connectionManager.startWithCountdown(botManager);
      res.json({ ok: true });
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  // POST /pause
  router.post('/pause', (req, res) => {
    try {
      const newStatus = gameState.pause();
      if (botManager) {
        if (newStatus === 'running') botManager.onGameStart();
        else botManager.onGameStop();
      }
      connectionManager.broadcastGameEvent(newStatus === 'running' ? 'game_resumed' : 'game_paused');
      connectionManager.broadcastState();
      res.json({ status: newStatus });
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  // POST /reset
  router.post('/reset', (req, res) => {
    connectionManager.cancelCountdown();
    connectionManager.cancelAutoReset();
    if (botManager) botManager.onGameReset();
    gameState.reset();
    connectionManager.broadcastGameEvent('game_reset');
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
