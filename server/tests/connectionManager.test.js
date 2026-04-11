'use strict';

const { test, describe, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const EventEmitter = require('events');

const GameState = require('../src/game/GameState');
const ConnectionManager = require('../src/ws/ConnectionManager');

// ─── Mock WebSocket ───────────────────────────────────────────────────────────
// Must extend EventEmitter so ws.on('message', ...) / ws.on('close', ...) work.

function makeMockWs() {
  const ws = new EventEmitter();
  ws.readyState = 1; // OPEN
  ws.sent = [];
  ws.send = function (data) {
    this.sent.push(JSON.parse(data));
  };
  ws.lastMessage = function () {
    return this.sent[this.sent.length - 1];
  };
  return ws;
}

// ─── handleConnection() ───────────────────────────────────────────────────────

describe('ConnectionManager — handleConnection()', () => {
  test('registers a new client in the clients map', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);

    assert.equal(cm.clients.size, 1);
    const [client] = cm.clients.values();
    assert.equal(client.ws, ws);
    assert.equal(client.type, null);
    assert.equal(client.playerId, null);
  });

  test('assigns a unique id per connection', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    cm.handleConnection(makeMockWs());
    cm.handleConnection(makeMockWs());

    const ids = [...cm.clients.keys()];
    assert.equal(ids.length, 2);
    assert.notEqual(ids[0], ids[1]);
  });
});

// ─── _handleRegister() ────────────────────────────────────────────────────────

describe('ConnectionManager — _handleRegister()', () => {
  test('assigns name and adds player to gameState for sensor type', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'TestHorse' });

    assert.equal(gameState.players.size, 1);
    assert.equal(gameState.players.get(clientId).name, 'TestHorse');

    const registered = ws.sent.find((m) => m.type === 'registered');
    assert.ok(registered);
    assert.equal(registered.payload.name, 'TestHorse');
    assert.equal(registered.payload.playerType, 'sensor');
  });

  test('auto-assigns a name if none provided', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleRegister(clientId, ws, { type: 'web' });

    const registered = ws.sent.find((m) => m.type === 'registered');
    assert.ok(registered.payload.name.length > 0);
  });

  test('sanitizes HTML tags from playerName', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: '<b>Hack</b>' });

    const registered = ws.sent.find((m) => m.type === 'registered');
    assert.equal(registered.payload.name, 'Hack');
  });

  test('display clients do not get added to gameState.players', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleRegister(clientId, ws, { type: 'display' });

    assert.equal(gameState.players.size, 0);
  });

  test('rejects invalid type', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleRegister(clientId, ws, { type: 'invalid' });

    const err = ws.sent.find((m) => m.type === 'error');
    assert.ok(err);
  });
});

// ─── _handleMessage() ────────────────────────────────────────────────────────

describe('ConnectionManager — _handleMessage()', () => {
  test('returns error on invalid JSON', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleMessage(clientId, ws, 'not valid json{{{');

    const err = ws.lastMessage();
    assert.equal(err.type, 'error');
    assert.equal(err.payload.message, 'Invalid JSON');
  });

  test('returns error on unknown message type', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleMessage(clientId, ws, JSON.stringify({ type: 'unknownType', payload: {} }));

    const err = ws.lastMessage();
    assert.equal(err.type, 'error');
    assert.match(err.payload.message, /Unknown message type/);
  });

  test('routes register message correctly', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleMessage(
      clientId,
      ws,
      JSON.stringify({ type: 'register', payload: { type: 'web', playerName: 'Tester' } })
    );

    const registered = ws.sent.find((m) => m.type === 'registered');
    assert.ok(registered);
  });
});

// ─── _handleDisconnect() ─────────────────────────────────────────────────────

describe('ConnectionManager — _handleDisconnect()', () => {
  test('removes client from clients map', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();

    cm._handleDisconnect(clientId);

    assert.equal(cm.clients.size, 0);
  });

  test('marks player disconnected when game is running', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();
    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'Rider' });

    // Start requires a connected player; add one first then start
    gameState.start();

    cm._handleDisconnect(clientId);

    // Player should still exist but marked disconnected
    const player = gameState.players.get(clientId);
    assert.ok(player);
    assert.equal(player.connected, false);
  });

  test('removes player from gameState when game is idle', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();
    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'Rider' });

    assert.equal(gameState.players.size, 1);
    cm._handleDisconnect(clientId);
    assert.equal(gameState.players.size, 0);
  });
});

// ─── _handleScore() ──────────────────────────────────────────────────────────

describe('ConnectionManager — _handleScore()', () => {
  function setupRunningGame() {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);
    const ws = makeMockWs();

    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();
    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'Rider' });
    gameState.start();

    return { gameState, cm, ws, clientId };
  }

  test('broadcast scored message includes events array', () => {
    const { cm, ws, clientId, gameState } = setupRunningGame();
    const [playerId] = gameState.players.keys();

    cm._handleScore(clientId, ws, { playerId, points: 1 });

    const scored = ws.sent.find((m) => m.type === 'scored');
    assert.ok(scored, 'should receive scored message');
    assert.ok(Array.isArray(scored.payload.events), 'events should be an array');
    assert.ok(scored.payload.events.includes('score_1'));
  });

  test('zero-point score accepted — event zero_roll, position unchanged', () => {
    const { cm, ws, clientId, gameState } = setupRunningGame();
    const [playerId] = gameState.players.keys();

    cm._handleScore(clientId, ws, { playerId, points: 0 });

    const scored = ws.sent.find((m) => m.type === 'scored');
    assert.ok(scored, 'should receive scored message');
    assert.equal(scored.payload.points, 0);
    assert.equal(scored.payload.newPosition, 0);
    assert.ok(scored.payload.events.includes('zero_roll'));
  });

  test('invalid points value sends error', () => {
    const { cm, ws, clientId, gameState } = setupRunningGame();
    const [playerId] = gameState.players.keys();

    cm._handleScore(clientId, ws, { playerId, points: 5 });

    const err = ws.sent.find((m) => m.type === 'error');
    assert.ok(err, 'should receive error message');
    assert.match(err.payload.message, /points must be 0, 1, 2, or 3/i);
  });
});

describe('ConnectionManager — getConnectedCounts()', () => {
  test('counts clients by type', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws1 = makeMockWs();
    const ws2 = makeMockWs();
    const ws3 = makeMockWs();

    cm.handleConnection(ws1);
    cm.handleConnection(ws2);
    cm.handleConnection(ws3);

    const ids = [...cm.clients.keys()];

    cm._handleRegister(ids[0], ws1, { type: 'sensor' });
    cm._handleRegister(ids[1], ws2, { type: 'motor' });
    cm._handleRegister(ids[2], ws3, { type: 'display' });

    const counts = cm.getConnectedCounts();
    assert.equal(counts.total, 3);
    assert.equal(counts.sensor, 1);
    assert.equal(counts.motor, 1);
    assert.equal(counts.display, 1);
    assert.equal(counts.web, 0);
  });
});

// ─── chipId-based reconnect during active game ────────────────────────────────

describe('ConnectionManager — chipId-based reconnect', () => {
  test('device reconnects via chipId after reboot during running game (no playerId)', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    // First connection: device registers with chipId
    const ws1 = makeMockWs();
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'Rider', chipId: 'ABC123' });
    assert.equal(gameState.players.size, 1);
    const playerId = cm.clients.get(clientId1).playerId;

    // Start game, score some points
    gameState.start();
    gameState.score(playerId, 2);

    // Device disconnects (player stays as disconnected)
    cm._handleDisconnect(clientId1);
    assert.equal(gameState.players.get(playerId).connected, false);
    assert.equal(gameState.players.get(playerId).position, 2);

    // Device reboots — no playerId, but same chipId
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'sensor', chipId: 'ABC123' });

    // Should NOT have created a second player
    assert.equal(gameState.players.size, 1);
    // Player should be reconnected (same player entry, same position)
    const player = gameState.players.get(playerId);
    assert.ok(player);
    assert.equal(player.connected, true);
    assert.equal(player.position, 2);

    // The new client should point to the same player
    assert.equal(cm.clients.get(clientId2).playerId, playerId);

    // The registered response should carry the original playerId
    const registered = ws2.sent.find(m => m.type === 'registered');
    assert.ok(registered);
    assert.equal(registered.payload.id, playerId);
  });

  test('chipId mapping is recorded on first registration', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws = makeMockWs();
    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();
    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'Test', chipId: 'C01D0001' });

    assert.equal(cm._chipIdToPlayerId.get('C01D0001'), cm.clients.get(clientId).playerId);
  });

  test('chipId-based reconnect preserves player name when none provided', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws1 = makeMockWs();
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'OriginalName', chipId: 'C01D0002' });

    gameState.start();
    cm._handleDisconnect(clientId1);

    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'sensor', chipId: 'C01D0002' });

    // Name should be preserved (not re-assigned)
    const playerId = cm.clients.get(clientId2).playerId;
    assert.equal(gameState.players.get(playerId).name, 'OriginalName');
  });

  test('chipId reconnect falls through to new registration when player was deleted', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    // Register and then reset to idle + disconnect (player gets deleted)
    const ws1 = makeMockWs();
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'Temp', chipId: 'C01D0003' });
    // In idle, disconnect deletes the player
    cm._handleDisconnect(clientId1);
    assert.equal(gameState.players.size, 0);

    // New connection with same chipId — should create a new player (no one to reconnect to)
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'sensor', chipId: 'C01D0003' });

    assert.equal(gameState.players.size, 1);
    // It's a new player, so playerId should be the new clientId
    assert.equal(cm.clients.get(clientId2).playerId, clientId2);
  });

  test('no chipId — normal registration even during active game', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws1 = makeMockWs();
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'web', playerName: 'WebPlayer' });
    gameState.start();

    // Another web client without chipId
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'web', playerName: 'WebPlayer2' });

    // Should create a second player (no chipId to reconnect)
    assert.equal(gameState.players.size, 2);
  });

  test('web client with chipId does NOT trigger chipId-based reconnect', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    // Hardware device registers with chipId
    const ws1 = makeMockWs();
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'Sensor', chipId: 'ABCD1234' });
    const sensorPlayerId = cm.clients.get(clientId1).playerId;

    gameState.start();
    cm._handleDisconnect(clientId1);

    // Web client tries to register with the same chipId
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'web', playerName: 'Hacker', chipId: 'ABCD1234' });

    // Should NOT have reconnected to the sensor's player — new player created
    assert.notEqual(cm.clients.get(clientId2).playerId, sensorPlayerId);
    assert.equal(gameState.players.size, 2);
  });

  test('chipId with invalid format is ignored', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws = makeMockWs();
    cm.handleConnection(ws);
    const [clientId] = cm.clients.keys();
    cm._handleRegister(clientId, ws, { type: 'sensor', playerName: 'Test', chipId: '<script>alert(1)</script>' });

    // chipId should not be stored in mapping
    assert.equal(cm._chipIdToPlayerId.size, 0);
    // Client should not have chipId set
    assert.equal(cm.clients.get(clientId).chipId, undefined);
  });
});

// ─── Stale socket cleanup on reconnect ────────────────────────────────────────

describe('ConnectionManager — stale socket cleanup', () => {
  test('reconnect via chipId closes stale client socket', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws1 = makeMockWs();
    let ws1Closed = false;
    ws1.close = () => { ws1Closed = true; };
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'Rider', chipId: 'ABCD1234' });
    const playerId = cm.clients.get(clientId1).playerId;

    gameState.start();

    // New connection arrives while old one is still in clients map
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'sensor', chipId: 'ABCD1234' });

    // Old socket should have been closed and removed
    assert.equal(ws1Closed, true);
    assert.equal(cm.clients.has(clientId1), false);
    // New client owns the player
    assert.equal(cm.clients.get(clientId2).playerId, playerId);
  });

  test('reconnect via playerId closes stale client socket', () => {
    const gameState = new GameState();
    const cm = new ConnectionManager(gameState);

    const ws1 = makeMockWs();
    let ws1Closed = false;
    ws1.close = () => { ws1Closed = true; };
    cm.handleConnection(ws1);
    const [clientId1] = cm.clients.keys();
    cm._handleRegister(clientId1, ws1, { type: 'sensor', playerName: 'Rider' });
    const playerId = cm.clients.get(clientId1).playerId;

    gameState.start();

    // New connection arrives with playerId
    const ws2 = makeMockWs();
    cm.handleConnection(ws2);
    const clientId2 = [...cm.clients.keys()].find(k => k !== clientId1);
    cm._handleRegister(clientId2, ws2, { type: 'sensor', playerId: playerId });

    // Old socket should have been closed and removed
    assert.equal(ws1Closed, true);
    assert.equal(cm.clients.has(clientId1), false);
    assert.equal(cm.clients.get(clientId2).playerId, playerId);
  });
});
