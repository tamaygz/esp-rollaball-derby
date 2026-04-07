'use strict';

const { Router } = require('express');

/**
 * Creates a clients router that exposes information about connected WebSocket
 * clients and allows forcibly closing (kicking) a client connection.
 */
function createClientsRouter(gameState, connectionManager) {
  const router = Router();

  // GET / — all currently connected WS clients, enriched with player context
  router.get('/', (req, res) => {
    const raw = connectionManager.getClientsList();
    const enriched = raw.map((c) => {
      const player = c.playerId ? gameState.players.get(c.playerId) : null;
      return {
        id: c.id,
        type: c.type,
        playerId: c.playerId,
        playerName: player ? player.name : null,
        playerPosition: player != null ? player.position : null,
      };
    });
    res.json(enriched);
  });

  // DELETE /:id — forcibly close a client's WebSocket connection
  router.delete('/:id', (req, res) => {
    const kicked = connectionManager.kickClient(req.params.id);
    if (!kicked) return res.status(404).json({ error: 'Client not found' });
    res.json({ kicked: true });
  });

  return router;
}

module.exports = createClientsRouter;
