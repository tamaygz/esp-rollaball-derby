'use strict';

const { Router } = require('express');

const HTML_TAG_PATTERN = /<[^>]*>/g;

/**
 * Creates a players router bound to the given gameState and connectionManager.
 */
function createPlayersRouter(gameState, connectionManager) {
  const router = Router();

  // GET / — all players
  router.get('/', (req, res) => {
    res.json([...gameState.players.values()]);
  });

  // PUT /:id — rename player
  router.put('/:id', (req, res) => {
    const { id } = req.params;
    const { name } = req.body;

    if (typeof name !== 'string') {
      return res.status(400).json({ error: 'name must be a string' });
    }

    const sanitized = name.replace(HTML_TAG_PATTERN, '').trim().slice(0, 20);

    try {
      const player = gameState.renamePlayer(id, sanitized);
      connectionManager.broadcastState();
      res.json(player);
    } catch {
      res.status(404).json({ error: 'Player not found' });
    }
  });

  return router;
}

module.exports = createPlayersRouter;
