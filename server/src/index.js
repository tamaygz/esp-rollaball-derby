'use strict';

const http = require('http');
const path = require('path');

const express = require('express');
const { WebSocketServer } = require('ws');

const GameState = require('./game/GameState');
const BotManager = require('./game/BotManager');
const ConnectionManager = require('./ws/ConnectionManager');
const healthRouter = require('./routes/health');
const createGameRouter = require('./routes/game');
const createPlayersRouter = require('./routes/players');
const createClientsRouter = require('./routes/clients');
const createBotsRouter = require('./routes/bots');

const PORT = process.env.PORT || 3000;
const HOST = process.env.HOST || '0.0.0.0';

// ─── App & state ──────────────────────────────────────────────────────────────

const app = express();
const gameState = new GameState();

// ─── Middleware ───────────────────────────────────────────────────────────────

app.use(express.json());

// Static files (display SPA, web test client)
app.use(express.static(path.join(__dirname, '..', 'public')));

// Admin + test client SPA served at /admin/
app.use('/admin', express.static(path.join(__dirname, '..', '..', 'clients', 'web')));

// Display (beamer/TV) SPA served at /display/
app.use('/display', express.static(path.join(__dirname, '..', '..', 'clients', 'display')));

// Shared game assets (sprites, track backgrounds, themes) served at /assets/
// Source of truth lives in clients/assets/ — no copy needed.
app.use('/assets', express.static(path.join(__dirname, '..', '..', 'clients', 'assets')));

// ─── Routes ───────────────────────────────────────────────────────────────────

// Placeholder — connectionManager created after HTTP server; routes mounted later.
// We forward a proxy so routes always see the current connectionManager reference.
let connectionManager;
let botManager;

app.use('/api/health', healthRouter);

app.use('/api/game', (req, res, next) => createGameRouter(gameState, connectionManager, botManager)(req, res, next));
app.use('/api/players', (req, res, next) => createPlayersRouter(gameState, connectionManager)(req, res, next));
app.use('/api/clients', (req, res, next) => createClientsRouter(gameState, connectionManager)(req, res, next));
app.use('/api/bots', (req, res, next) => createBotsRouter(botManager)(req, res, next));

// ─── Global error handler ─────────────────────────────────────────────────────

// eslint-disable-next-line no-unused-vars
app.use((err, req, res, next) => {
  res.status(500).json({ error: err.message });
});

// ─── HTTP + WebSocket server ──────────────────────────────────────────────────

const server = http.createServer(app);

const wss = new WebSocketServer({ server });
connectionManager = new ConnectionManager(gameState);
botManager = new BotManager(gameState, connectionManager);

wss.on('connection', (ws) => {
  connectionManager.handleConnection(ws);
});

// ─── Start ────────────────────────────────────────────────────────────────────

server.listen(PORT, HOST, () => {
  const now = new Date().toISOString();
  console.log(`[Derby Server] Starting on http://${HOST}:${PORT} [${now}]`);
  console.log(`[Derby Server] WebSocket hub ready`);
  console.log(
    `[Derby Server] Game state: ${gameState.getStatus()} | Players: ${gameState.players.size}`
  );
});

module.exports = { app, server, gameState, connectionManager: () => connectionManager, botManager: () => botManager };
