'use strict';

const { test, describe } = require('node:test');
const assert = require('node:assert/strict');

const GameState = require('../src/game/GameState');

// ─── Helpers ──────────────────────────────────────────────────────────────────

function makeGame() {
  return new GameState();
}

function addConnectedPlayer(game, id = 'p1', name = 'Tester', type = 'sensor') {
  return game.addPlayer(id, name, type);
}

// ─── Lifecycle transitions ────────────────────────────────────────────────────

describe('GameState — lifecycle', () => {
  test('idle → running (valid)', () => {
    const game = makeGame();
    addConnectedPlayer(game);
    game.start();
    assert.equal(game.getStatus(), 'running');
    assert.ok(game.startedAt !== null);
  });

  test('idle → paused (invalid — should throw)', () => {
    const game = makeGame();
    assert.throws(() => game.pause(), /Cannot pause/);
  });

  test('cannot start with no players', () => {
    const game = makeGame();
    assert.throws(() => game.start(), /no players connected/);
  });

  test('cannot start from running state', () => {
    const game = makeGame();
    addConnectedPlayer(game);
    game.start();
    assert.throws(() => game.start(), /Cannot start/);
  });

  test('running → paused → running toggle', () => {
    const game = makeGame();
    addConnectedPlayer(game);
    game.start();
    const paused = game.pause();
    assert.equal(paused, 'paused');
    const resumed = game.pause();
    assert.equal(resumed, 'running');
  });

  test('running → finished on winner', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    // Score enough to win (trackLength default 15).
    // Reset lastScoredAt before each call to bypass the 300ms rate limit in tests.
    for (let i = 0; i < 5; i++) {
      game.players.get(player.id).lastScoredAt = null;
      game.score(player.id, 3);
    }
    assert.equal(game.getStatus(), 'finished');
  });
});

// ─── score() ─────────────────────────────────────────────────────────────────

describe('GameState — score()', () => {
  test('increments position correctly (+1)', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    const { player: updated } = game.score(player.id, 1);
    assert.equal(updated.position, 1);
  });

  test('increments position correctly (+3)', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    const { player: updated } = game.score(player.id, 3);
    assert.equal(updated.position, 3);
  });

  test('increments position correctly (+2)', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    const { player: updated } = game.score(player.id, 2);
    assert.equal(updated.position, 2);
  });

  test('rejects invalid points — -1', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    assert.throws(() => game.score(player.id, -1), /Points must be 0, 1, 2, or 3/);
  });

  test('accepts zero points (zero roll) — position unchanged, event zero_roll', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    const { player: updated, winner, events } = game.score(player.id, 0);
    assert.equal(updated.position, 0);
    assert.equal(winner, false);
    assert.ok(events.includes('zero_roll'));
  });

  test('rate limits two rapid successive scores', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    game.score(player.id, 1);
    assert.throws(() => game.score(player.id, 1), /rate limited/);
  });

  test('rejects score when game is not running', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    assert.throws(() => game.score(player.id, 1), /Cannot score/);
  });
});

// ─── Winner detection ─────────────────────────────────────────────────────────

describe('GameState — winner detection', () => {
  test('winner: true when position >= trackLength', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.updateConfig({ trackLength: 6 }); // 6 is within valid range (5–50)
    game.start();
    // Score +3 twice (total 6 >= 6); reset rate limit between calls
    game.score(player.id, 3);
    game.players.get(player.id).lastScoredAt = null;
    const { winner } = game.score(player.id, 3);
    assert.equal(winner, true);
    assert.equal(game.getStatus(), 'finished');
  });

  test('winner: false while under track length', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.updateConfig({ trackLength: 15 });
    game.start();
    const { winner } = game.score(player.id, 1);
    assert.equal(winner, false);
    assert.equal(game.getStatus(), 'running');
  });
});

// ─── reset() ─────────────────────────────────────────────────────────────────

describe('GameState — reset()', () => {
  test('resets all positions to 0 and status to idle', () => {
    const game = makeGame();
    const p1 = addConnectedPlayer(game, 'p1', 'Alice', 'sensor');
    const p2 = addConnectedPlayer(game, 'p2', 'Bob', 'sensor');
    game.start();
    game.score(p1.id, 1);

    const state = game.reset();
    assert.equal(state.status, 'idle');
    assert.equal(state.startedAt, null);
    for (const p of state.players) {
      assert.equal(p.position, 0);
    }
  });
});

// ─── updateConfig() ──────────────────────────────────────────────────────────

describe('GameState — updateConfig()', () => {
  test('blocked when game is running', () => {
    const game = makeGame();
    addConnectedPlayer(game);
    game.start();
    assert.throws(() => game.updateConfig({ trackLength: 10 }), /Cannot update config/);
  });

  test('rejects trackLength < 5', () => {
    const game = makeGame();
    assert.throws(() => game.updateConfig({ trackLength: 4 }), /trackLength/);
  });

  test('rejects trackLength > 50', () => {
    const game = makeGame();
    assert.throws(() => game.updateConfig({ trackLength: 51 }), /trackLength/);
  });

  test('rejects maxPlayers > 16', () => {
    const game = makeGame();
    assert.throws(() => game.updateConfig({ maxPlayers: 17 }), /maxPlayers/);
  });

  test('rejects maxPlayers < 1', () => {
    const game = makeGame();
    assert.throws(() => game.updateConfig({ maxPlayers: 0 }), /maxPlayers/);
  });

  test('rejects non-string theme', () => {
    const game = makeGame();
    assert.throws(() => game.updateConfig({ theme: 42 }), /theme/);
  });

  test('applies valid config update', () => {
    const game = makeGame();
    const cfg = game.updateConfig({ trackLength: 20, maxPlayers: 8, theme: 'camel' });
    assert.equal(cfg.trackLength, 20);
    assert.equal(cfg.maxPlayers, 8);
    assert.equal(cfg.theme, 'camel');
  });

  test('accepts "auto" as a valid theme', () => {
    const game = makeGame();
    const cfg = game.updateConfig({ theme: 'auto' });
    assert.equal(cfg.theme, 'auto');
  });
});

// ─── auto-theme resolution on start() ────────────────────────────────────────

describe('GameState — auto-theme resolution', () => {
  test('start() resolves "auto" theme to a concrete theme', () => {
    const game = makeGame();
    game.updateConfig({ theme: 'auto' });
    addConnectedPlayer(game);
    game.start();
    assert.notEqual(game.config.theme, 'auto');
    assert.ok(['horse', 'camel', 'reef'].includes(game.config.theme));
  });

  test('start() preserves explicit theme unchanged', () => {
    const game = makeGame();
    game.updateConfig({ theme: 'camel' });
    addConnectedPlayer(game);
    game.start();
    assert.equal(game.config.theme, 'camel');
  });
});

// ─── reconnectPlayer() ───────────────────────────────────────────────────────

describe('GameState — reconnectPlayer()', () => {
  test('returns null for unknown player', () => {
    const game = makeGame();
    assert.equal(game.reconnectPlayer('nonexistent'), null);
  });

  test('marks a disconnected player as connected again', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game, 'p1', 'Alice', 'sensor');
    game.start(); // must be running so disconnectPlayer marks as !connected rather than deleting
    game.disconnectPlayer('p1');
    assert.equal(game.players.get('p1').connected, false);
    const result = game.reconnectPlayer('p1');
    assert.ok(result);
    assert.equal(result.connected, true);
  });

  test('is idempotent for an already-connected player', () => {
    const game = makeGame();
    addConnectedPlayer(game, 'p1', 'Alice', 'sensor');
    const result = game.reconnectPlayer('p1');
    assert.ok(result);
    assert.equal(result.connected, true);
  });

  test('preserves the existing player entry (no duplicate created)', () => {
    const game = makeGame();
    addConnectedPlayer(game, 'p1', 'Alice', 'sensor');
    game.start(); // running state so disconnectPlayer keeps the entry
    game.disconnectPlayer('p1');
    game.reconnectPlayer('p1');
    assert.equal(game.players.size, 1);
    assert.equal(game.players.get('p1').name, 'Alice');
  });
});

describe('GameState — assignName()', () => {
  test('returns a non-empty string', () => {
    const game = makeGame();
    const name = game.assignName();
    assert.ok(typeof name === 'string' && name.length > 0);
  });

  test('returns unique names across calls', () => {
    const game = makeGame();
    const names = new Set();
    for (let i = 0; i < 10; i++) {
      names.add(game.assignName());
    }
    assert.equal(names.size, 10);
  });

  test('names reset after reset()', () => {
    const game = makeGame();
    const first = game.assignName();
    addConnectedPlayer(game, 'p1', first, 'sensor');
    game.reset();
    // After reset, the used set is cleared; first name can be reassigned
    const pool = [];
    for (let i = 0; i < 60; i++) {
      pool.push(game.assignName());
    }
    assert.ok(pool.includes(first));
  });
});

// ─── Streak tracking ──────────────────────────────────────────────────────────

describe('GameState — streak tracking', () => {
  function noRateLimit(game, id) {
    game.players.get(id).lastScoredAt = null;
  }

  test('streak_zero_3x fires after 3 consecutive zero rolls', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 0);
    noRateLimit(game, player.id);
    game.score(player.id, 0);
    noRateLimit(game, player.id);
    const { events } = game.score(player.id, 0);
    assert.ok(events.includes('streak_zero_3x'));
    assert.ok(events.includes('zero_roll'));
  });

  test('streak_zero_3x does NOT fire after only 2 consecutive zeros', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 0);
    noRateLimit(game, player.id);
    const { events } = game.score(player.id, 0);
    assert.ok(!events.includes('streak_zero_3x'));
  });

  test('consecutiveZeros resets after a non-zero score', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 0);
    noRateLimit(game, player.id);
    game.score(player.id, 0);
    // Breaks the streak
    noRateLimit(game, player.id);
    game.score(player.id, 1);
    noRateLimit(game, player.id);
    game.score(player.id, 0);

    assert.equal(game.players.get(player.id).consecutiveZeros, 1);
  });

  test('streak_three_2x fires after 2 consecutive +3 rolls', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 3);
    noRateLimit(game, player.id);
    const { events } = game.score(player.id, 3);
    assert.ok(events.includes('streak_three_2x'));
    assert.ok(events.includes('score_3'));
  });

  test('streak_three_2x does NOT fire after only 1 +3 roll', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    const { events } = game.score(player.id, 3);
    assert.ok(!events.includes('streak_three_2x'));
  });

  test('consecutivePlusThrees resets after a non-3 score', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 3);
    noRateLimit(game, player.id);
    game.score(player.id, 1);
    noRateLimit(game, player.id);
    game.score(player.id, 3);

    assert.equal(game.players.get(player.id).consecutivePlusThrees, 1);
  });

  test('streak counters reset after game reset()', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    noRateLimit(game, player.id);
    game.score(player.id, 0);
    noRateLimit(game, player.id);
    game.score(player.id, 0);

    game.reset();
    assert.equal(game.players.get(player.id).consecutiveZeros, 0);
    assert.equal(game.players.get(player.id).consecutivePlusThrees, 0);
  });
});

// ─── Rank-change events ────────────────────────────────────────────────────────

describe('GameState — rank-change events', () => {
  function noRateLimit(game, id) {
    game.players.get(id).lastScoredAt = null;
  }

  test('took_lead fires when player overtakes all others', () => {
    const game = makeGame();
    const p1 = addConnectedPlayer(game, 'p1', 'Alice');
    const p2 = addConnectedPlayer(game, 'p2', 'Bob');
    game.start();

    // p1 scores first (is now leading)
    game.score(p1.id, 3);
    // p2 is behind; now p2 scores past p1
    noRateLimit(game, p2.id);
    const { events } = game.score(p2.id, 3);
    // p2 went from 0 to 3, tied with p1 — not strictly ahead; no took_lead
    // p1 was at 3, p2 now at 3 — tied, NOT a lead takeover (isFirst for both)
    // Let's confirm no took_lead yet (tied is not takeover in our sort)
    // Instead score p2 one more time so they're strictly ahead
    noRateLimit(game, p2.id);
    const { events: events2 } = game.score(p2.id, 1);
    assert.ok(events2.includes('took_lead'));
  });

  test('took_lead does NOT fire when already leading', () => {
    const game = makeGame();
    const p1 = addConnectedPlayer(game, 'p1', 'Alice');
    addConnectedPlayer(game, 'p2', 'Bob');
    game.start();

    game.score(p1.id, 3);
    noRateLimit(game, p1.id);
    const { events } = game.score(p1.id, 1);
    // p1 was already first, should not fire took_lead
    assert.ok(!events.includes('took_lead'));
  });

  test('took_lead does NOT fire with only one player', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();
    const { events } = game.score(player.id, 3);
    assert.ok(!events.includes('took_lead'));
  });

  test('score events always include a base event', () => {
    const game = makeGame();
    const player = addConnectedPlayer(game);
    game.start();

    const r1 = game.score(player.id, 1);
    assert.ok(r1.events.includes('score_1'));

    player.lastScoredAt = null;
    const r3 = game.score(player.id, 3);
    assert.ok(r3.events.includes('score_3'));

    player.lastScoredAt = null;
    const r0 = game.score(player.id, 0);
    assert.ok(r0.events.includes('zero_roll'));
  });
});

// ─── Sequence counter (T11) ───────────────────────────────────────────────────

describe('GameState — seq counter', () => {
  test('nextSeq() is monotonically increasing from 1', () => {
    const game = makeGame();
    assert.equal(game.getSeq(), 0);
    assert.equal(game.nextSeq(), 1);
    assert.equal(game.nextSeq(), 2);
    assert.equal(game.nextSeq(), 3);
    assert.equal(game.getSeq(), 3);
  });

  test('reset() resets seq to 0', () => {
    const game = makeGame();
    game.nextSeq();
    game.nextSeq();
    assert.equal(game.getSeq(), 2);
    game.reset();
    assert.equal(game.getSeq(), 0);
    assert.equal(game.nextSeq(), 1);
  });
});
