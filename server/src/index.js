'use strict';

const http = require('http');
const path = require('path');
const os = require('os');

const express = require('express');
const { WebSocketServer } = require('ws');
const { Bonjour } = require('bonjour-service');

const GameState = require('./game/GameState');
const BotManager = require('./game/BotManager');
const ConnectionManager = require('./ws/ConnectionManager');
const LedConfigManager = require('./config/LedConfigManager');
const SoundManager = require('./sound/SoundManager');
const healthRouter = require('./routes/health');
const adminRouter = require('./routes/admin');
const createGameRouter = require('./routes/game');
const createPlayersRouter = require('./routes/players');
const createClientsRouter = require('./routes/clients');
const createBotsRouter = require('./routes/bots');
const createSensorsRouter = require('./routes/sensors');
const createLedRoutes = require('./routes/leds');

const PORT = process.env.PORT || 3000;
const HOST = process.env.HOST || '0.0.0.0';

// ─── App & state ──────────────────────────────────────────────────────────────

const app = express();
const gameState = new GameState();
const ledConfigManager = new LedConfigManager();
const soundManager = new SoundManager(path.join(__dirname, '..', 'sounds'));

// Load LED configuration on startup
ledConfigManager.loadConfig().catch(error => {
  console.error('[Derby Server] Failed to load LED config:', error.message);
});

// ─── Middleware ───────────────────────────────────────────────────────────────

app.use(express.json());

// ─── Template engine ─────────────────────────────────────────────────────────
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, '..', 'views'));

// Static files (display SPA, web test client)
app.use(express.static(path.join(__dirname, '..', 'public')));

// Admin page routes (EJS-rendered; must precede static middleware to take priority)
app.use('/admin', adminRouter);

// Admin static assets (CSS, JS, images) served at /admin/
app.use('/admin', express.static(path.join(__dirname, '..', '..', 'clients', 'web')));

// Display (beamer/TV) SPA served at /display/
app.use('/display', express.static(path.join(__dirname, '..', '..', 'clients', 'display')));

// Shared game assets (sprites, track backgrounds, themes) served at /assets/
// Source of truth lives in clients/assets/ — no copy needed.
app.use('/assets', express.static(path.join(__dirname, '..', '..', 'clients', 'assets')));

// Shared JS utilities (gameEvents.js etc.) served at /shared/
// Available to both the display client and the web admin client.
app.use('/shared', express.static(path.join(__dirname, '..', '..', 'clients', 'shared')));

// ESP8266 sensor browser flasher (ESP Web Tools) served at /flash-sensor/
// No toolchain needed — works from Chrome/Edge via Web Serial.
app.use('/flash-sensor', express.static(path.join(__dirname, '..', '..', 'clients', 'esp8266-sensor', 'web-install')));

// ESP Web Tools vendor bundle shared by both flasher pages (avoids duplication).
app.use('/flash-vendor', express.static(path.join(__dirname, '..', '..', 'clients', 'esp8266-sensor', 'web-install', 'vendor')));

// ESP32 motor controller browser flasher served at /flash-motor/
app.use('/flash-motor', express.static(path.join(__dirname, '..', '..', 'clients', 'esp32-motor', 'web-install')));

// ─── Routes ───────────────────────────────────────────────────────────────────

// Placeholder — connectionManager created after HTTP server; routes mounted later.
// We forward a proxy so routes always see the current connectionManager reference.
let connectionManager;
let botManager;

app.use('/api/health', healthRouter);
app.use('/api/leds', (req, res, next) => createLedRoutes(ledConfigManager, connectionManager)(req, res, next));

app.use('/api/game', (req, res, next) => createGameRouter(gameState, connectionManager, botManager)(req, res, next));
app.use('/api/players', (req, res, next) => createPlayersRouter(gameState, connectionManager)(req, res, next));
app.use('/api/clients', (req, res, next) => createClientsRouter(gameState, connectionManager)(req, res, next));
app.use('/api/bots', (req, res, next) => createBotsRouter(botManager)(req, res, next));
app.use('/api/sensors', createSensorsRouter());

// ─── Global error handler ─────────────────────────────────────────────────────

// eslint-disable-next-line no-unused-vars
app.use((err, req, res, next) => {
  res.status(500).json({ error: err.message });
});

// ─── HTTP + WebSocket server ──────────────────────────────────────────────────

const server = http.createServer(app);

const wss = new WebSocketServer({ server });
connectionManager = new ConnectionManager(gameState, ledConfigManager, soundManager);
botManager = new BotManager(gameState, connectionManager);
connectionManager.setBotManager(botManager);

// Wire config change events to broadcast LED config
ledConfigManager.on('configChanged', (deviceType, config) => {
  connectionManager.broadcastLedConfig(deviceType, config);
});

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

  // ─── mDNS / DNS-SD advertisement ─────────────────────────────────────────
  // Publishes _derby._tcp.local so ESP8266 sensors (and browsers on macOS/iOS)
  // can auto-discover the server without manual IP configuration.
  // On Windows hosts running WSL2 / Hyper-V, os.networkInterfaces() contains
  // multiple IPv4 addresses (e.g. 172.x for virtual adapters). We pick the
  // first non-internal, non-link-local, non-virtual address — the physical LAN
  // IP — so the ESP32 clients can reach us.
  const lanIp = (() => {
    const ifaces = os.networkInterfaces();
    for (const name of Object.keys(ifaces)) {
      // Skip known virtual adapter name prefixes (Hyper-V, WSL2, VirtualBox)
      if (/vethernet|wsl|loopback|vmware|virtualbox/i.test(name)) continue;
      for (const iface of ifaces[name]) {
        if (iface.family !== 'IPv4' || iface.internal) continue;
        // Skip link-local (169.254.x.x) and known virtual ranges (172.16–31.x)
        const [a, b] = iface.address.split('.').map(Number);
        if (a === 169 && b === 254) continue;
        if (a === 172 && b >= 16 && b <= 31) continue;
        return iface.address;
      }
    }
    return undefined; // let bonjour choose
  })();

  const bonjour = new Bonjour(lanIp ? { interface: lanIp } : {});
  const hostname = os.hostname();
  bonjour.publish({
    name: 'derby-server',
    type: 'derby',
    port: Number(PORT),
    txt: { version: '1', hostname }
  });
  console.log(`[Derby Server] mDNS: advertising _derby._tcp on port ${PORT}${lanIp ? ` (${lanIp})` : ''}`);
});

module.exports = { app, server, gameState, connectionManager: () => connectionManager, botManager: () => botManager };
