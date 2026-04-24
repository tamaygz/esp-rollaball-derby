'use strict';

const { randomUUID } = require('crypto');

/**
 * BotManager — creates and manages server-side bot players.
 *
 * Each bot is an autonomous player that scores at random intervals
 * (345 ms – 6.3 s) while the game is running.  Bots are created via REST API and live
 * entirely on the server — no WebSocket client needed.
 *
 * Roll probability table (0–100 inclusive):
 *   0–10  → +3 points  (~10.9 %)
 *   11–25 → +2 points  (~14.9 %)
 *   26–55 → +1 point   (~29.7 %)
 *   56–100 → 0 points  (~44.6 %)
 */

const MIN_DELAY_MS     = 345;
const RANDOM_RANGE_MS  = 6000;
const STAGGER_BASE_MS  = 500;
const STAGGER_RANGE_MS = 1466;

class BotManager {
  /**
   * @param {import('./GameState')} gameState
   * @param {import('../ws/ConnectionManager')} connectionManager
   */
  constructor(gameState, connectionManager) {
    this.gameState = gameState;
    this.connectionManager = connectionManager;
    this._bots = new Map(); // botId → { id, playerId, timer }
  }

  // ── Roll probability ──────────────────────────────────────────────────────

  static calcPoints(roll) {
    if (roll <= 10) return 3;
    if (roll <= 25) return 2;
    if (roll <= 55) return 1;
    return 0;
  }

  // ── Bot lifecycle ─────────────────────────────────────────────────────────

  /**
   * Create a new server-side bot player.
   * @returns {{ id: string, playerId: string, playerName: string }}
   */
  addBot() {
    const botId     = randomUUID();
    const name      = this.gameState.assignName();
    const colorIndex = this.gameState.nextFreeColorIndex();
    const player    = this.gameState.addPlayer(botId, name, 'bot', colorIndex);
    const bot      = { id: botId, playerId: botId, playerName: player.name, timer: null };

    this._bots.set(botId, bot);

    // Auto-start if game is already running
    if (this.gameState.getStatus() === 'running') {
      this._startBot(bot);
    }

    this.connectionManager.broadcastState();
    this.connectionManager.broadcastPositions();
    return { id: bot.id, playerId: bot.playerId, playerName: bot.playerName };
  }

  /**
   * Remove a bot and its player from the game.
   * @param {string} botId
   * @returns {boolean} true if the bot existed
   */
  removeBot(botId) {
    const bot = this._bots.get(botId);
    if (!bot) return false;

    this._stopBot(bot);
    this._bots.delete(botId);

    // Delegate to GameState.removePlayer() for standard semantics first.
    // In idle state this fully deletes; in running/paused it only disconnects.
    // Bots have no real WS connection, so if only disconnected, force-delete.
    this.gameState.removePlayer(bot.playerId);
    if (this.gameState.players.has(bot.playerId)) {
      this.gameState.players.delete(bot.playerId);
    }

    this.connectionManager.broadcastState();
    this.connectionManager.broadcastPositions();
    return true;
  }

  /**
   * List all active bots.
   * @returns {Array<{ id: string, playerId: string, playerName: string }>}
   */
  listBots() {
    const result = [];
    for (const bot of this._bots.values()) {
      result.push({ id: bot.id, playerId: bot.playerId, playerName: bot.playerName });
    }
    return result;
  }

  // ── Game state hooks (called by game routes) ──────────────────────────────

  /** Start all bot timers (called when game transitions to 'running'). */
  onGameStart() {
    for (const bot of this._bots.values()) {
      this._startBot(bot);
    }
  }

  /** Stop all bot timers (called on pause / finish / reset). */
  onGameStop() {
    for (const bot of this._bots.values()) {
      this._stopBot(bot);
    }
  }

  /** Stop timers and reset bot players' streaks (called on game reset). */
  onGameReset() {
    this.onGameStop();
    // Player positions + streaks are already reset by GameState.reset().
    // Just make sure bots are re-connected (reset may have kept them).
    for (const bot of this._bots.values()) {
      const player = this.gameState.players.get(bot.playerId);
      if (player) player.connected = true;
    }
  }

  /** Remove all bots and their players. */
  removeAll() {
    for (const bot of this._bots.values()) {
      this._stopBot(bot);
      this.gameState.removePlayer(bot.playerId);
      if (this.gameState.players.has(bot.playerId)) {
        this.gameState.players.delete(bot.playerId);
      }
    }
    this._bots.clear();
    this.connectionManager.broadcastState();
    this.connectionManager.broadcastPositions();
  }

  // ── Internal timer logic ──────────────────────────────────────────────────

  _startBot(bot) {
    if (bot.timer) return;
    const delay = STAGGER_BASE_MS + Math.floor(Math.random() * STAGGER_RANGE_MS);
    bot.timer = setTimeout(() => {
      bot.timer = null;
      this._rollFor(bot);
    }, delay);
  }

  _stopBot(bot) {
    if (bot.timer) {
      clearTimeout(bot.timer);
      bot.timer = null;
    }
  }

  _scheduleNext(bot) {
    this._stopBot(bot);
    const delay = MIN_DELAY_MS + Math.floor(Math.random() * RANDOM_RANGE_MS);
    bot.timer = setTimeout(() => {
      bot.timer = null;
      this._rollFor(bot);
    }, delay);
  }

  _rollFor(bot) {
    if (this.gameState.getStatus() !== 'running') return;

    const roll   = Math.floor(Math.random() * 101); // 0–100 inclusive
    const points = BotManager.calcPoints(roll);

    try {
      const result = this.gameState.score(bot.playerId, points);
      const { player, winner, events } = result;

      this.connectionManager.broadcastScored(player, points, events);
      this.connectionManager.broadcastPositions();
      this.connectionManager.broadcastState();

      if (winner) {
        this.connectionManager.broadcastWinner(player);
        return; // game over — don't schedule more rolls
      }
    } catch {
      // rate limited or game ended between schedule and fire — silently skip
    }

    this._scheduleNext(bot);
  }
}

module.exports = BotManager;
