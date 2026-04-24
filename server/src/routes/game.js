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
      console.log('[Game Routes] Start requested');
      res.json({ ok: true });
    } catch (err) {
      console.warn('[Game Routes] Start rejected:', err.message);
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
      console.log(`[Game Routes] Pause toggled: ${newStatus}`);
      connectionManager.broadcastGameEvent(newStatus === 'running' ? 'game_resumed' : 'game_paused');
      connectionManager.broadcastState();
      res.json({ status: newStatus });
    } catch (err) {
      console.warn('[Game Routes] Pause rejected:', err.message);
      res.status(400).json({ error: err.message });
    }
  });

  // POST /reset
  router.post('/reset', (req, res) => {
    connectionManager.cancelCountdown();
    connectionManager.cancelAutoReset();
    if (botManager) botManager.onGameReset();
    gameState.reset();
    console.log('[Game Routes] Reset requested');
    connectionManager.broadcastGameEvent('game_reset');
    connectionManager.broadcastState();
    connectionManager.broadcastPositions();
    res.json({ ok: true });
  });

  // PUT /config
  router.put('/config', (req, res) => {
    try {
      const updated = gameState.updateConfig(req.body);
      console.log('[Game Routes] Config updated:', updated);
      connectionManager.broadcastState();
      res.json(updated);
    } catch (err) {
      console.warn('[Game Routes] Config update rejected:', err.message);
      res.status(400).json({ error: err.message });
    }
  });

  return router;
}

module.exports = createGameRouter;
