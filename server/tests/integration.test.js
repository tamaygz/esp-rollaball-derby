'use strict';

const { test, describe, before, after } = require('node:test');
const assert = require('node:assert/strict');
const http = require('http');
const path = require('path');

const express = require('express');
const { WebSocketServer } = require('ws');
const WebSocket = require('ws');

const GameState = require('../src/game/GameState');
const ConnectionManager = require('../src/ws/ConnectionManager');
const healthRouter = require('../src/routes/health');
const createGameRouter = require('../src/routes/game');
const createPlayersRouter = require('../src/routes/players');

// ─── Helpers ──────────────────────────────────────────────────────────────────

function startServer() {
  return new Promise((resolve) => {
    const app = express();
    app.use(express.json());

    const gameState = new GameState();
    let connectionManager;

    app.use('/api/health', healthRouter);
    app.use('/api/game', (req, res, next) =>
      createGameRouter(gameState, connectionManager)(req, res, next)
    );
    app.use('/api/players', (req, res, next) =>
      createPlayersRouter(gameState, connectionManager)(req, res, next)
    );

    const server = http.createServer(app);
    const wss = new WebSocketServer({ server });
    connectionManager = new ConnectionManager(gameState);

    wss.on('connection', (ws) => connectionManager.handleConnection(ws));

    // Random available port
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address();
      resolve({ server, gameState, connectionManager, port });
    });
  });
}

function wsConnect(port) {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`ws://127.0.0.1:${port}`);
    ws.once('open', () => resolve(ws));
    ws.once('error', reject);
  });
}

function waitForMessage(ws, predicate, timeoutMs = 2000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('Timeout waiting for message')), timeoutMs);

    function onMessage(data) {
      const msg = JSON.parse(data.toString());
      if (predicate(msg)) {
        clearTimeout(timer);
        ws.removeListener('message', onMessage);
        resolve(msg);
      }
    }
    ws.on('message', onMessage);
  });
}

function collectMessages(ws, count, timeoutMs = 2000) {
  return new Promise((resolve, reject) => {
    const msgs = [];
    const timer = setTimeout(
      () => reject(new Error(`Timeout: only received ${msgs.length}/${count} messages`)),
      timeoutMs
    );

    function onMessage(data) {
      msgs.push(JSON.parse(data.toString()));
      if (msgs.length >= count) {
        clearTimeout(timer);
        ws.removeListener('message', onMessage);
        resolve(msgs);
      }
    }
    ws.on('message', onMessage);
  });
}

// ─── Tests ────────────────────────────────────────────────────────────────────

describe('Integration — WebSocket flow', () => {
  let server, port, gameState;

  before(async () => {
    ({ server, port, gameState } = await startServer());
  });

  after(() => {
    server.close();
  });

  test('connect, register, receive registered + state broadcast', async () => {
    const ws = await wsConnect(port);

    const pending = collectMessages(ws, 2);

    ws.send(JSON.stringify({ type: 'register', payload: { type: 'sensor', playerName: 'Speedy' } }));

    const msgs = await pending;
    ws.close();

    const registered = msgs.find((m) => m.type === 'registered');
    const state = msgs.find((m) => m.type === 'state');

    assert.ok(registered, 'should receive registered message');
    assert.equal(registered.payload.name, 'Speedy');
    assert.equal(registered.payload.playerType, 'sensor');

    assert.ok(state, 'should receive state broadcast');
    assert.equal(state.payload.status, 'idle');
  });

  test('score event produces scored + state broadcasts', async () => {
    // Reset game
    gameState.reset();
    // Clear existing players
    gameState.players.clear();

    const ws = await wsConnect(port);

    // Register
    const regPromise = waitForMessage(ws, (m) => m.type === 'registered');
    ws.send(JSON.stringify({ type: 'register', payload: { type: 'sensor', playerName: 'Racer' } }));
    const registered = await regPromise;
    const playerId = registered.payload.id;

    // Pre-arm listener for 'running' state BEFORE making the REST call to avoid
    // a race where the WS broadcast arrives before the listener is registered.
    const runningStatePromise = waitForMessage(
      ws,
      (m) => m.type === 'state' && m.payload.status === 'running'
    );

    // Start game via REST
    await new Promise((resolve, reject) => {
      const req = http.request(
        { hostname: '127.0.0.1', port, path: '/api/game/start', method: 'POST',
          headers: { 'Content-Type': 'application/json' } },
        (res) => { res.resume(); res.on('end', resolve); }
      );
      req.on('error', reject);
      req.end();
    });

    // Wait for the running state broadcast
    await runningStatePromise;

    // Send score
    const scoredPromise = waitForMessage(ws, (m) => m.type === 'scored');
    ws.send(JSON.stringify({ type: 'score', payload: { playerId, points: 1 } }));

    const scored = await scoredPromise;
    assert.equal(scored.payload.playerId, playerId);
    assert.equal(scored.payload.points, 1);
    assert.equal(scored.payload.newPosition, 1);
    assert.ok(Array.isArray(scored.payload.events), 'scored message should include events array');
    assert.ok(scored.payload.events.includes('score_1'));

    ws.close();
  });

  test('zero-point score produces scored message with zero_roll event', async () => {
    gameState.reset();
    gameState.players.clear();

    const ws = await wsConnect(port);

    const regPromise = waitForMessage(ws, (m) => m.type === 'registered');
    ws.send(JSON.stringify({ type: 'register', payload: { type: 'sensor', playerName: 'ZeroRoller' } }));
    const registered = await regPromise;
    const playerId = registered.payload.id;

    const runningStatePromise = waitForMessage(
      ws,
      (m) => m.type === 'state' && m.payload.status === 'running'
    );
    await new Promise((resolve, reject) => {
      const req = http.request(
        { hostname: '127.0.0.1', port, path: '/api/game/start', method: 'POST',
          headers: { 'Content-Type': 'application/json' } },
        (res) => { res.resume(); res.on('end', resolve); }
      );
      req.on('error', reject);
      req.end();
    });
    await runningStatePromise;

    const scoredPromise = waitForMessage(ws, (m) => m.type === 'scored');
    ws.send(JSON.stringify({ type: 'score', payload: { playerId, points: 0 } }));

    const scored = await scoredPromise;
    assert.equal(scored.payload.points, 0);
    assert.equal(scored.payload.newPosition, 0);
    assert.ok(scored.payload.events.includes('zero_roll'));

    ws.close();
  });

  test('disconnect updates state', async () => {
    gameState.reset();
    gameState.players.clear();

    const ws = await wsConnect(port);

    const regPromise = waitForMessage(ws, (m) => m.type === 'registered');
    ws.send(JSON.stringify({ type: 'register', payload: { type: 'web', playerName: 'Leaver' } }));
    await regPromise;

    assert.equal(gameState.players.size, 1);

    ws.close();

    // Give the server a tick to process the disconnect
    await new Promise((r) => setTimeout(r, 100));

    // In idle state, disconnected player is removed
    assert.equal(gameState.players.size, 0);
  });

  test('invalid JSON sends error message, does not crash', async () => {
    const ws = await wsConnect(port);

    const errPromise = waitForMessage(ws, (m) => m.type === 'error');
    ws.send('{ not valid json ');
    const err = await errPromise;

    assert.equal(err.payload.message, 'Invalid JSON');
    ws.close();
  });
});

// ─── REST API ─────────────────────────────────────────────────────────────────

describe('Integration — REST API', () => {
  let server, port, gameState, connectionManager;

  before(async () => {
    ({ server, port, gameState, connectionManager } = await startServer());
  });

  after(() => {
    server.close();
  });

  function restRequest(method, path, body) {
    return new Promise((resolve, reject) => {
      const data = body ? JSON.stringify(body) : null;
      const req = http.request(
        {
          hostname: '127.0.0.1',
          port,
          path,
          method,
          headers: {
            'Content-Type': 'application/json',
            ...(data ? { 'Content-Length': Buffer.byteLength(data) } : {}),
          },
        },
        (res) => {
          let raw = '';
          res.on('data', (c) => (raw += c));
          res.on('end', () => resolve({ status: res.statusCode, body: JSON.parse(raw) }));
        }
      );
      req.on('error', reject);
      if (data) req.write(data);
      req.end();
    });
  }

  test('GET /api/health returns ok', async () => {
    const res = await restRequest('GET', '/api/health');
    assert.equal(res.status, 200);
    assert.equal(res.body.status, 'ok');
  });

  test('GET /api/game returns game state', async () => {
    const res = await restRequest('GET', '/api/game');
    assert.equal(res.status, 200);
    assert.equal(res.body.status, 'idle');
  });

  test('POST /api/game/start returns 400 when no players', async () => {
    gameState.reset();
    gameState.players.clear();
    const res = await restRequest('POST', '/api/game/start');
    assert.equal(res.status, 400);
    assert.ok(res.body.error);
  });

  test('PUT /api/game/config updates trackLength', async () => {
    const res = await restRequest('PUT', '/api/game/config', { trackLength: 20 });
    assert.equal(res.status, 200);
    assert.equal(res.body.trackLength, 20);
  });

  test('PUT /api/game/config rejects invalid trackLength', async () => {
    const res = await restRequest('PUT', '/api/game/config', { trackLength: 9999 });
    assert.equal(res.status, 400);
    assert.ok(res.body.error);
  });
});
