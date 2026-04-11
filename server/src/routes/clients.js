'use strict';

const { Router } = require('express');
const http = require('http');

/**
 * Creates a clients router that exposes information about connected WebSocket
 * clients and allows forcibly closing (kicking) a client connection.
 *
 * Also provides server-proxy routes so the admin web UI can reach ESP32 REST
 * endpoints (motor calibration + Bluetooth management) without the browser
 * needing to know the ESP32's IP address directly.
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

  // ── Motor & BT Proxy ─────────────────────────────────────────────────────
  //
  // These routes proxy admin UI requests to the ESP32's built-in HTTP server.
  // The ESP32's IP is resolved from the capabilities reported on registration
  // (capabilities.ip), or falls back to connecting over the WS socket address.
  //
  // Route: POST|GET /api/devices/:clientId/motor/*
  // Route: POST|GET /api/devices/:clientId/bt/*

  function _resolveEsp32Ip(clientId) {
    const client = connectionManager.getDeviceById(clientId);
    if (!client) return null;
    // Prefer explicitly reported IP (set in capabilities during registration)
    if (client.capabilities && client.capabilities.ip) {
      return client.capabilities.ip;
    }
    // Fall back to the peer address of the WebSocket connection
    const ws = client.ws;
    if (ws && ws._socket && ws._socket.remoteAddress) {
      // Strip IPv6-mapped IPv4 prefix (::ffff:1.2.3.4 → 1.2.3.4)
      return ws._socket.remoteAddress.replace(/^::ffff:/, '');
    }
    return null;
  }

  function _proxyToEsp32(req, res, clientId, esp32Path) {
    const ip = _resolveEsp32Ip(clientId);
    if (!ip) {
      return res.status(404).json({ error: 'Motor client not found or IP unavailable' });
    }

    const body = (req.method !== 'GET' && req.body) ? JSON.stringify(req.body) : null;
    const options = {
      hostname: ip,
      port: 80,
      path: esp32Path,
      method: req.method,
      headers: { 'Content-Type': 'application/json' },
      timeout: 8000,
    };
    if (body) {
      options.headers['Content-Length'] = Buffer.byteLength(body);
    }

    const proxyReq = http.request(options, (proxyRes) => {
      let data = '';
      proxyRes.on('data', (chunk) => { data += chunk; });
      proxyRes.on('end', () => {
        res.status(proxyRes.statusCode);
        try {
          res.json(JSON.parse(data));
        } catch {
          res.send(data);
        }
      });
    });

    proxyReq.on('timeout', () => {
      proxyReq.destroy();
      res.status(504).json({ error: 'ESP32 request timed out' });
    });
    proxyReq.on('error', (err) => {
      if (!res.headersSent) {
        res.status(502).json({ error: `ESP32 unreachable: ${err.message}` });
      }
    });

    if (body) proxyReq.write(body);
    proxyReq.end();
  }

  // Motor calibration proxy: /api/clients/:id/motor/*  → ESP32 /api/motor/*
  router.all('/:id/motor/*', (req, res) => {
    const subPath = '/api/motor/' + req.params[0];
    _proxyToEsp32(req, res, req.params.id, subPath);
  });

  // Bluetooth management proxy: /api/clients/:id/bt/*  → ESP32 /api/bt/*
  router.all('/:id/bt/*', (req, res) => {
    const subPath = '/api/bt/' + req.params[0];
    _proxyToEsp32(req, res, req.params.id, subPath);
  });

  return router;
}

module.exports = createClientsRouter;
