'use strict';

const { Router } = require('express');
const http = require('http');
const rateLimit = require('express-rate-limit');

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

  // Rate limiting middleware for motor control endpoints
  // Prevents excessive requests that could overwhelm the ESP32 wireless link
  // or cause rapid motor commands to queue up and become unsafe
  const motorLimiter = rateLimit({
    windowMs: 1000,        // 1-second window
    max: 10,               // Max 10 requests per window (10 req/sec)
    standardHeaders: true, // Include rate-limit headers in response
    skip: (_req, _res) => false,
    message: 'Motor endpoint rate limit exceeded. Please try again later.',
  });

  const motorColorsLimiter = rateLimit({
    windowMs: 1000,
    max: 5,                // Stricter limit for color changes (5 req/sec)
    standardHeaders: true,
    skip: (_req, _res) => false,
    message: 'Color update rate limit exceeded. Please try again later.',
  });

  // GET / — all currently connected WS clients, enriched with player context
  router.get('/', (req, res) => {
    const raw = connectionManager.getClientsList();
    const enriched = raw.map((c) => {
      const player = c.playerId ? gameState.players.get(c.playerId) : null;
      const result = {
        id: c.id,
        type: c.type,
        playerId: c.playerId,
        playerName: player ? player.name : null,
        playerPosition: player != null ? player.position : null,
      };
      if (c.type === 'motor') {
        const device = connectionManager.getDeviceById(c.id);
        if (device) {
          result.motorCount  = device.motorCount  || 0;
          result.motorColors = device.motorColors || [];
        }
      }
      return result;
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

  /**
   * Promise-based proxy to ESP32 HTTP endpoints. Returns { statusCode, body, ok }.
   * Does NOT write to response — caller handles that.
   */
  function _proxyToEsp32Promise(req, clientId, esp32Path, timeoutMs = 8000) {
    return new Promise((resolve, reject) => {
      const ip = _resolveEsp32Ip(clientId);
      if (!ip) {
        return reject(new Error('Motor client not found or IP unavailable'));
      }

      const body = (req.method !== 'GET' && req.body) ? JSON.stringify(req.body) : null;
      const options = {
        hostname: ip,
        port: 80,
        path: esp32Path,
        method: req.method,
        headers: { 'Content-Type': 'application/json' },
        timeout: timeoutMs,
      };
      if (body) {
        options.headers['Content-Length'] = Buffer.byteLength(body);
      }

      const proxyReq = http.request(options, (proxyRes) => {
        let data = '';
        proxyRes.on('data', (chunk) => { data += chunk; });
        proxyRes.on('end', () => {
          let parsedBody;
          try {
            parsedBody = JSON.parse(data);
          } catch {
            parsedBody = data;
          }
          resolve({
            statusCode: proxyRes.statusCode,
            body: parsedBody,
            ok: proxyRes.statusCode >= 200 && proxyRes.statusCode < 300,
          });
        });
      });

      proxyReq.on('timeout', () => {
        proxyReq.destroy();
        reject(new Error('ESP32 request timed out'));
      });
      proxyReq.on('error', (err) => {
        reject(err);
      });

      if (body) proxyReq.write(body);
      proxyReq.end();
    });
  }

  /**
   * Legacy callback-based proxy. Writes directly to response.
   * Kept for backward compatibility with existing routes.
   */
  function _proxyToEsp32(req, res, clientId, esp32Path, timeoutMs = 8000) {
    _proxyToEsp32Promise(req, clientId, esp32Path, timeoutMs)
      .then((result) => {
        res.status(result.statusCode);
        if (typeof result.body === 'object') {
          res.json(result.body);
        } else {
          res.send(result.body);
        }
      })
      .catch((err) => {
        if (!res.headersSent) {
          const statusCode = err.message.includes('not found') ? 404 :
                            err.message.includes('timed out') ? 504 : 502;
          res.status(statusCode).json({ error: err.message });
        }
      });
  }

  // Motor calibration proxy: /api/clients/:id/motor/*  → ESP32 /api/motor/*

  // POST /:id/motor/colors — update server-side motorColors then persist to ESP32
  // Rate limited to prevent overwhelming the ESP32 wireless link
  router.post('/:id/motor/colors', motorColorsLimiter, async (req, res) => {
    const { colors } = req.body || {};
    if (!Array.isArray(colors)) {
      return res.status(400).json({ error: 'colors must be an array' });
    }

    const client = connectionManager.getDeviceById(req.params.id);
    if (!client || client.type !== 'motor') {
      return res.status(404).json({ error: 'Motor client not found' });
    }

    // Validate and clamp color indices to 0-15 range
    const validatedColors = colors.map((c) => {
      const num = Number(c);
      if (!Number.isFinite(num)) return 0;
      return Math.max(0, Math.min(15, Math.floor(num)));
    });

    // Proxy to ESP32 first — only update server state on success
    try {
      const result = await _proxyToEsp32Promise(req, req.params.id, '/api/motor/colors');
      if (result.ok) {
        // ESP32 save succeeded — update server in-memory state
        client.motorColors = validatedColors;
        console.log(`[Clients] Updated motorColors for ${req.params.id}:`, validatedColors);
      }
      res.status(result.statusCode).json(result.body);
    } catch (err) {
      const statusCode = err.message.includes('not found') ? 404 :
                        err.message.includes('timed out') ? 504 : 502;
      res.status(statusCode).json({ error: err.message });
    }
  });

  router.all('/:id/motor/*', motorLimiter, (req, res) => {
    const subPath = '/api/motor/' + req.params[0];
    _proxyToEsp32(req, res, req.params.id, subPath);
  });

  return router;
}

module.exports = createClientsRouter;
