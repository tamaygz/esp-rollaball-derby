'use strict';

const { describe, test, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert/strict');

const GameState = require('../src/game/GameState');
const BotManager = require('../src/game/BotManager');

// ── Helpers ─────────────────────────────────────────────────────────────────

function makeMockCM() {
  return {
    _scored: [],
    _states: 0,
    _positions: 0,
    _winners: [],
    broadcastScored(player, points, events) {
      this._scored.push({ player, points, events });
    },
    broadcastState() { this._states += 1; },
    broadcastPositions() { this._positions += 1; },
    broadcastWinner(player) { this._winners.push(player); },
  };
}

function makeSetup() {
  const gs = new GameState();
  const cm = makeMockCM();
  const bm = new BotManager(gs, cm);
  return { gs, cm, bm };
}

// ── Tests ───────────────────────────────────────────────────────────────────

describe('BotManager — addBot / removeBot / listBots', () => {
  let bm, gs, cm;
  beforeEach(() => { ({ bm, gs, cm } = makeSetup()); });

  test('addBot creates a player and returns bot info', () => {
    const bot = bm.addBot();
    assert.ok(bot.id);
    assert.ok(bot.playerId);
    assert.ok(bot.playerName);
    assert.equal(gs.players.size, 1);
    const player = gs.players.get(bot.playerId);
    assert.equal(player.type, 'bot');
    assert.equal(player.connected, true);
  });

  test('addBot broadcasts state', () => {
    bm.addBot();
    assert.ok(cm._states >= 1);
  });

  test('addBot can create multiple bots', () => {
    bm.addBot();
    bm.addBot();
    bm.addBot();
    assert.equal(bm.listBots().length, 3);
    assert.equal(gs.players.size, 3);
  });

  test('listBots returns all bots', () => {
    bm.addBot();
    bm.addBot();
    const list = bm.listBots();
    assert.equal(list.length, 2);
    assert.ok(list[0].id);
    assert.ok(list[0].playerName);
  });

  test('removeBot stops and deletes the bot', () => {
    const bot = bm.addBot();
    assert.equal(bm.removeBot(bot.id), true);
    assert.equal(bm.listBots().length, 0);
    assert.equal(gs.players.size, 0);
  });

  test('removeBot returns false for unknown id', () => {
    assert.equal(bm.removeBot('nonexistent'), false);
  });

  test('removeBot force-removes player even during running game', () => {
    const bot = bm.addBot();
    gs.start();
    assert.equal(bm.removeBot(bot.id), true);
    assert.equal(gs.players.size, 0);
  });

  test('removeAll clears all bots', () => {
    bm.addBot();
    bm.addBot();
    bm.removeAll();
    assert.equal(bm.listBots().length, 0);
    assert.equal(gs.players.size, 0);
  });
});

describe('BotManager — calcPoints', () => {
  test('roll 0-10 gives 3 points', () => {
    assert.equal(BotManager.calcPoints(0), 3);
    assert.equal(BotManager.calcPoints(10), 3);
  });
  test('roll 11-25 gives 2 points', () => {
    assert.equal(BotManager.calcPoints(11), 2);
    assert.equal(BotManager.calcPoints(25), 2);
  });
  test('roll 26-55 gives 1 point', () => {
    assert.equal(BotManager.calcPoints(26), 1);
    assert.equal(BotManager.calcPoints(55), 1);
  });
  test('roll 56-100 gives 0 points', () => {
    assert.equal(BotManager.calcPoints(56), 0);
    assert.equal(BotManager.calcPoints(100), 0);
  });
});

describe('BotManager — game lifecycle hooks', () => {
  let bm, gs, cm;
  beforeEach(() => { ({ bm, gs, cm } = makeSetup()); });
  afterEach(() => { bm.removeAll(); });

  test('onGameStart starts bot timers (no immediate scoring)', (t) => {
    const bot = bm.addBot();
    gs.start();
    bm.onGameStart();
    // Timers are running but haven't fired yet
    const internal = bm._bots.get(bot.id);
    assert.ok(internal.timer !== null);
    bm.onGameStop();
  });

  test('onGameStop clears bot timers', () => {
    const bot = bm.addBot();
    gs.start();
    bm.onGameStart();
    bm.onGameStop();
    const internal = bm._bots.get(bot.id);
    assert.equal(internal.timer, null);
  });

  test('onGameReset stops timers and re-connects bot players', () => {
    const bot = bm.addBot();
    gs.start();
    bm.onGameStart();
    // Disconnect a bot player manually
    const player = gs.players.get(bot.playerId);
    player.connected = false;
    bm.onGameReset();
    assert.equal(player.connected, true);
    const internal = bm._bots.get(bot.id);
    assert.equal(internal.timer, null);
  });

  test('bot auto-starts when added during running game', () => {
    // Need at least one connected player to start
    gs.addPlayer('real-1', 'RealPlayer', 'web');
    gs.start();
    const bot = bm.addBot();
    const internal = bm._bots.get(bot.id);
    assert.ok(internal.timer !== null);
    bm.onGameStop();
  });
});

describe('BotManager — _rollFor scoring', () => {
  let bm, gs, cm;
  beforeEach(() => { ({ bm, gs, cm } = makeSetup()); });
  afterEach(() => { bm.removeAll(); });

  test('_rollFor scores and broadcasts when game is running', () => {
    const bot = bm.addBot();
    gs.start();
    const internal = bm._bots.get(bot.id);
    // Stop any auto-started timer
    bm._stopBot(internal);
    // Force a roll
    bm._rollFor(internal);
    // Should have broadcast scored + state + positions
    assert.ok(cm._scored.length >= 1, 'should broadcast scored');
    assert.ok(cm._states >= 2, 'should broadcast state (addBot + roll)');
    assert.ok(cm._positions >= 1, 'should broadcast positions');
    // Clean up timer
    bm._stopBot(internal);
  });

  test('_rollFor does nothing when game is not running', () => {
    const bot = bm.addBot();
    const internal = bm._bots.get(bot.id);
    const scoredBefore = cm._scored.length;
    bm._rollFor(internal);
    assert.equal(cm._scored.length, scoredBefore, 'should not score when idle');
  });

  test('_rollFor can score 0 points (zero_roll event)', () => {
    const bot = bm.addBot();
    gs.start();
    const internal = bm._bots.get(bot.id);
    bm._stopBot(internal);
    // Monkey-patch Math.random to always return high (→ 0 points)
    const orig = Math.random;
    Math.random = () => 0.99;
    try {
      bm._rollFor(internal);
      assert.equal(cm._scored.length, 1);
      assert.equal(cm._scored[0].points, 0);
      assert.ok(cm._scored[0].events.includes('zero_roll'));
    } finally {
      Math.random = orig;
      bm._stopBot(internal);
    }
  });
});
