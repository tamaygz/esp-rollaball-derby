'use strict';

const http = require('http');
const net  = require('net');
const { Router } = require('express');

/**
 * Returns true if `ip` (already confirmed to be a valid IP by net.isIP()) is
 * within an RFC 1918 / RFC 4193 private address range.  We only allow the
 * sensor proxy to reach LAN devices, not arbitrary internet hosts.
 */
function _isPrivateIp(ip) {
  // IPv4 private ranges: 10.x, 172.16-31.x, 192.168.x, 127.x (loopback)
  if (net.isIPv4(ip)) {
    const [a, b] = ip.split('.').map(Number);
    return (
      a === 10 ||
      a === 127 ||
      (a === 172 && b >= 16 && b <= 31) ||
      (a === 192 && b === 168)
    );
  }
  // IPv6: loopback (::1) and ULA (fc00::/7)
  if (net.isIPv6(ip)) {
    const norm = ip.toLowerCase();
    if (norm === '::1') return true;
    // ULA prefix fc00::/7 covers fc… and fd…
    if (norm.startsWith('fc') || norm.startsWith('fd')) return true;
  }
  return false;
}

/**
 * Creates a sensors router that allows the admin to push a new server
 * IP/port/name to any sensor that is already on the local network.
 *
 * The sensor must be running firmware with the HTTP config server on port 80
 * (POST /config endpoint).
 *
 * @param {object} [options]
 * @param {number} [options.sensorPort=80] Override the port used to reach the
 *   sensor — useful for unit tests that spin up a fake sensor on an ephemeral
 *   port.
 */
function createSensorsRouter({ sensorPort = 80 } = {}) {
  const router = Router();

  // POST /configure — push server config to a sensor over HTTP
  // Body: { sensorIp, serverIp, serverPort?, playerName? }
  router.post('/configure', (req, res) => {
    const { sensorIp, serverIp, serverPort, playerName } = req.body || {};

    if (!sensorIp || typeof sensorIp !== 'string' || !sensorIp.trim()) {
      return res.status(400).json({ error: 'sensorIp is required' });
    }
    const trimmedSensorIp = sensorIp.trim();
    if (!net.isIP(trimmedSensorIp)) {
      return res.status(400).json({ error: 'sensorIp must be a valid IP address' });
    }
    if (!_isPrivateIp(trimmedSensorIp)) {
      return res.status(400).json({ error: 'sensorIp must be a private/LAN IP address' });
    }
    if (!serverIp || typeof serverIp !== 'string' || !serverIp.trim()) {
      return res.status(400).json({ error: 'serverIp is required' });
    }

    const portNum = parseInt(serverPort, 10);
    if (serverPort !== undefined && serverPort !== '' &&
        (Number.isNaN(portNum) || portNum < 1 || portNum > 65535)) {
      return res.status(400).json({ error: 'serverPort must be a number between 1 and 65535' });
    }

    const config = {
      server_ip: serverIp.trim(),
    };

    if (serverPort !== undefined && serverPort !== '') {
      config.server_port = String(portNum);
    }

    if (typeof playerName === 'string') {
      const trimmedPlayerName = playerName.trim();
      if (trimmedPlayerName) {
        config.player_name = trimmedPlayerName;
      }
    }

    const payload = JSON.stringify(config);
    const options = {
      hostname: trimmedSensorIp,
      port:     sensorPort,
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
        console.log(`[Sensors] Config pushed to ${trimmedSensorIp}`);
        res.json({ ok: true });
      } else {
        console.warn(`[Sensors] Sensor ${trimmedSensorIp} returned HTTP ${sensorRes.statusCode}`);
        res.status(502).json({ error: `Sensor returned HTTP ${sensorRes.statusCode}` });
      }
    });

    sensorReq.on('timeout', () => {
      sensorReq.destroy();
      if (!res.headersSent) {
        console.warn(`[Sensors] Sensor ${trimmedSensorIp} timed out`);
        res.status(504).json({ error: 'Sensor did not respond within timeout' });
      }
    });

    sensorReq.on('error', (e) => {
      if (!res.headersSent) {
        console.warn(`[Sensors] Sensor ${trimmedSensorIp} request error: ${e.message}`);
        res.status(502).json({ error: `Could not reach sensor: ${e.message}` });
      }
    });

    sensorReq.write(payload);
    sensorReq.end();
  });

  return router;
}

module.exports = createSensorsRouter;
