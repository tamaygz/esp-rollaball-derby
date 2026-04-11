'use strict';

/**
 * Tests for POST /api/sensors/configure
 *
 * Uses a lightweight local HTTP server as a fake sensor to verify the happy
 * path and a variety of validation/error cases without network access.
 */

const { test, describe, before, after, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const http = require('http');

const express = require('express');
const createSensorsRouter = require('../src/routes/sensors');

// ─── Test-server helpers ──────────────────────────────────────────────────────

function startApiServer(options = {}) {
  return new Promise((resolve) => {
    const app = express();
    app.use(express.json());
    app.use('/api/sensors', createSensorsRouter(options));
    const server = http.createServer(app);
    server.listen(0, '127.0.0.1', () => {
      resolve({ server, port: server.address().port });
    });
  });
}

function startFakeSensor(handler) {
  return new Promise((resolve) => {
    const server = http.createServer(handler);
    server.listen(0, '127.0.0.1', () => {
      resolve({ server, port: server.address().port });
    });
  });
}

function apiPost(apiPort, path, body) {
  return new Promise((resolve, reject) => {
    const payload = JSON.stringify(body);
    const req = http.request(
      {
        hostname: '127.0.0.1',
        port: apiPort,
        path,
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(payload),
        },
      },
      (res) => {
        let data = '';
        res.on('data', (c) => { data += c; });
        res.on('end', () => {
          resolve({ status: res.statusCode, body: JSON.parse(data) });
        });
      }
    );
    req.on('error', reject);
    req.write(payload);
    req.end();
  });
}

// ─── Tests ───────────────────────────────────────────────────────────────────

describe('POST /api/sensors/configure — validation', () => {
  let apiServer, apiPort;

  before(async () => {
    ({ server: apiServer, port: apiPort } = await startApiServer());
  });

  after(() => { apiServer.close(); });

  test('returns 400 when sensorIp is missing', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', { serverIp: '192.168.1.10' });
    assert.equal(res.status, 400);
    assert.ok(res.body.error);
  });

  test('returns 400 when serverIp is missing', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', { sensorIp: '192.168.1.50' });
    assert.equal(res.status, 400);
    assert.ok(res.body.error);
  });

  test('returns 400 when sensorIp is not a valid IP address', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', {
      sensorIp: 'not-an-ip',
      serverIp: '192.168.1.10',
    });
    assert.equal(res.status, 400);
    assert.match(res.body.error, /valid IP/i);
  });

  test('returns 400 when sensorIp is a public IP address', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', {
      sensorIp: '8.8.8.8',
      serverIp: '192.168.1.10',
    });
    assert.equal(res.status, 400);
    assert.match(res.body.error, /private/i);
  });

  test('returns 400 when serverPort is out of range', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', {
      sensorIp: '192.168.1.50',
      serverIp: '192.168.1.10',
      serverPort: '99999',
    });
    assert.equal(res.status, 400);
    assert.match(res.body.error, /serverPort/i);
  });

  test('returns 400 when serverPort is not numeric', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', {
      sensorIp: '192.168.1.50',
      serverIp: '192.168.1.10',
      serverPort: 'abc',
    });
    assert.equal(res.status, 400);
    assert.match(res.body.error, /serverPort/i);
  });
});

describe('POST /api/sensors/configure — happy path', () => {
  let apiServer, apiPort;
  let fakeSensor, sensorPort;
  let receivedBody;

  before(async () => {
    // Start fake sensor first so we know its port before wiring the router.
    ({ server: fakeSensor, port: sensorPort } = await startFakeSensor((req, res) => {
      let data = '';
      req.on('data', (c) => { data += c; });
      req.on('end', () => {
        try { receivedBody = JSON.parse(data); } catch (_) { receivedBody = null; }
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end('{"ok":true}');
      });
    }));

    // Wire the API server to forward to the fake sensor's ephemeral port.
    ({ server: apiServer, port: apiPort } = await startApiServer({ sensorPort }));
  });

  after(() => {
    apiServer.close();
    fakeSensor.close();
  });

  beforeEach(() => { receivedBody = undefined; });

  function sensorPayload(overrides = {}) {
    return {
      sensorIp:   '127.0.0.1',
      serverIp:   '192.168.1.10',
      serverPort: '3000',
      playerName: 'Alice',
      ...overrides,
    };
  }

  test('proxies config to sensor and returns ok:true', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', sensorPayload());
    assert.equal(res.status, 200, `Unexpected status ${res.status}: ${JSON.stringify(res.body)}`);
    assert.deepEqual(res.body, { ok: true });
    assert.equal(receivedBody.server_ip,   '192.168.1.10');
    assert.equal(receivedBody.server_port, '3000');
    assert.equal(receivedBody.player_name, 'Alice');
  });

  test('omits player_name from forwarded payload when blank', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', sensorPayload({ playerName: '' }));
    assert.equal(res.status, 200);
    assert.equal(receivedBody.server_ip,   '192.168.1.10');
    assert.equal(receivedBody.server_port, '3000');
    assert.ok(!('player_name' in receivedBody), 'player_name should be omitted when blank');
  });

  test('omits server_port from forwarded payload when blank', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', sensorPayload({ serverPort: '' }));
    assert.equal(res.status, 200);
    assert.equal(receivedBody.server_ip, '192.168.1.10');
    assert.ok(!('server_port' in receivedBody), 'server_port should be omitted when blank');
  });

  test('accepts 10.x.x.x private IP', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', sensorPayload({ sensorIp: '10.0.0.50' }));
    // 10.x is allowed but the fake sensor only listens on 127.0.0.1, so we get ECONNREFUSED → 502.
    assert.ok(
      res.status === 200 || res.status === 502 || res.status === 504,
      `Expected connection attempt, got ${res.status}`
    );
  });

  test('accepts 172.16.x.x private IP', async () => {
    const res = await apiPost(apiPort, '/api/sensors/configure', sensorPayload({ sensorIp: '172.16.0.50' }));
    assert.ok(
      res.status === 200 || res.status === 502 || res.status === 504,
      `Expected connection attempt, got ${res.status}`
    );
  });
});
