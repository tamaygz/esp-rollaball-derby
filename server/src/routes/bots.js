'use strict';

const { Router } = require('express');

/**
 * Creates a bots router bound to the given botManager.
 */
function createBotsRouter(botManager) {
  const router = Router();

  // GET / — list all active bots
  router.get('/', (req, res) => {
    res.json(botManager.listBots());
  });

  // POST / — create a new server-side bot
  router.post('/', (req, res) => {
    try {
      const bot = botManager.addBot();
      res.status(201).json(bot);
    } catch (err) {
      res.status(400).json({ error: err.message });
    }
  });

  // DELETE /:id — remove a bot
  router.delete('/:id', (req, res) => {
    const removed = botManager.removeBot(req.params.id);
    if (!removed) {
      return res.status(404).json({ error: 'Bot not found' });
    }
    res.json({ removed: true });
  });

  return router;
}

module.exports = createBotsRouter;
