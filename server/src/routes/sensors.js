'use strict';

const http = require('http');
const { Router } = require('express');

/**
 * Creates a sensors router that allows the admin to push a new server
 * IP/port/name to any sensor that is already on the local network.
 *
 * The sensor must be running firmware with the HTTP config server on port 80
 * (POST /config endpoint).
 */
function createSensorsRouter() {
  const router = Router();

  // POST /configure — push server config to a sensor over HTTP
  // Body: { sensorIp, serverIp, serverPort?, playerName? }
  router.post('/configure', (req, res) => {
    const { sensorIp, serverIp, serverPort, playerName } = req.body || {};

    if (!sensorIp || typeof sensorIp !== 'string' || !sensorIp.trim()) {
      return res.status(400).json({ error: 'sensorIp is required' });
    }
    if (!serverIp || typeof serverIp !== 'string' || !serverIp.trim()) {
      return res.status(400).json({ error: 'serverIp is required' });
    }

    const portNum = parseInt(serverPort, 10);
    if (serverPort !== undefined && serverPort !== '' &&
        (Number.isNaN(portNum) || portNum < 1 || portNum > 65535)) {
      return res.status(400).json({ error: 'serverPort must be a number between 1 and 65535' });
    }

    const payload = JSON.stringify({
      server_ip:   serverIp.trim(),
      server_port: String(serverPort || '3000'),
      player_name: typeof playerName === 'string' ? playerName.trim() : '',
    });

    const options = {
      hostname: sensorIp.trim(),
      port:     80,
      path:     '/config',
      method:   'POST',
      headers:  {
        'Content-Type':   'application/json',
        'Content-Length': Buffer.byteLength(payload),
      },
      timeout: 5000,
    };

    const sensorReq = http.request(options, (sensorRes) => {
      sensorRes.resume(); // drain body — we only care about status code
      if (sensorRes.statusCode === 200) {
        res.json({ ok: true });
      } else {
        res.status(502).json({ error: `Sensor returned HTTP ${sensorRes.statusCode}` });
      }
    });

    sensorReq.on('timeout', () => {
      sensorReq.destroy();
      if (!res.headersSent) {
        res.status(504).json({ error: 'Sensor did not respond within timeout' });
      }
    });

    sensorReq.on('error', (e) => {
      if (!res.headersSent) {
        res.status(502).json({ error: `Could not reach sensor: ${e.message}` });
      }
    });

    sensorReq.write(payload);
    sensorReq.end();
  });

  return router;
}

module.exports = createSensorsRouter;
