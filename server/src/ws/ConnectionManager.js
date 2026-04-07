'use strict';

const { randomUUID } = require('crypto');

const VALID_TYPES = new Set(['sensor', 'web', 'motor', 'display']);
const HTML_TAG_PATTERN = /<[^>]*>/g;

const AUTO_RESET_DELAY_MS = 15_000;

class ConnectionManager {
  constructor(gameState) {
    this.gameState     = gameState;
    this.clients       = new Map(); // id → { ws, type, playerId, id }
    this._botManager   = null;
    this._autoResetTimer = null;
  }

  // ─── Public ───────────────────────────────────────────────────────────────

  setBotManager(botManager) {
    this._botManager = botManager;
  }

  cancelAutoReset() {
    if (this._autoResetTimer !== null) {
      clearTimeout(this._autoResetTimer);
      this._autoResetTimer = null;
    }
  }

  handleConnection(ws) {
    const clientId = randomUUID();

    this.clients.set(clientId, { ws, type: null, playerId: null, id: clientId });

    ws.on('message', (data) => this._handleMessage(clientId, ws, data));
    ws.on('close', () => this._handleDisconnect(clientId));
    ws.on('error', () => this._handleDisconnect(clientId));
  }

  getConnectedCounts() {
    const counts = { total: 0, sensor: 0, web: 0, motor: 0, display: 0 };
    for (const client of this.clients.values()) {
      counts.total += 1;
      if (client.type && counts[client.type] !== undefined) {
        counts[client.type] += 1;
      }
    }
    return counts;
  }

  getClientsList() {
    const list = [];
    for (const { id, type, playerId } of this.clients.values()) {
      list.push({ id, type: type || null, playerId: playerId || null });
    }
    return list;
  }

  kickClient(clientId) {
    const client = this.clients.get(clientId);
    if (!client) return false;

    const { ws } = client;

    if (ws.readyState === 3 /* CLOSED */) {
      this._handleDisconnect(clientId);
      return true;
    }

    try {
      if (typeof ws.terminate === 'function') {
        const terminateTimer = setTimeout(() => {
          try { ws.terminate(); } catch (_) { /* ignore */ }
        }, 1000);

        ws.once('close', () => clearTimeout(terminateTimer));
      }

      ws.close();
    } catch (_) {
      try {
        if (typeof ws.terminate === 'function') ws.terminate();
      } catch (_) { /* ignore */ }
    }
    return true;
  }

  broadcastAll(msg) {
    const json = JSON.stringify(msg);
    for (const { ws } of this.clients.values()) {
      if (ws.readyState === 1 /* OPEN */) {
        ws.send(json);
      }
    }
  }

  broadcastState() {
    const state = this.gameState.toJSON();
    state.connectedClients = this.getConnectedCounts();
    this.broadcastAll({ type: 'state', payload: state });
  }

  broadcastPositions() {
    const players = [...this.gameState.players.values()].map((p) => ({
      id: p.id,
      position: p.position,
      maxPosition: this.gameState.config.trackLength,
    }));

    const msg = JSON.stringify({ type: 'positions', payload: { players } });

    for (const client of this.clients.values()) {
      if (client.type === 'motor' && client.ws.readyState === 1) {
        client.ws.send(msg);
      }
    }
  }

  broadcastScored(player, points, events) {
    this.broadcastAll({
      type: 'scored',
      payload: {
        playerId: player.id,
        playerName: player.name,
        points,
        newPosition: player.position,
        events: events || [],
      },
    });
  }

  broadcastWinner(player) {
    this.broadcastAll({
      type: 'winner',
      payload: { playerId: player.id, name: player.name },
    });

    // Auto-reset after 15 s so the screen clears without manual intervention.
    this.cancelAutoReset();
    this._autoResetTimer = setTimeout(() => {
      this._autoResetTimer = null;
      if (this._botManager) this._botManager.onGameReset();
      this.gameState.reset();
      this.broadcastState();
      this.broadcastPositions();
    }, AUTO_RESET_DELAY_MS);
  }

  // ─── Private ──────────────────────────────────────────────────────────────

  _send(ws, msg) {
    if (ws.readyState === 1) {
      ws.send(JSON.stringify(msg));
    }
  }

  _handleMessage(clientId, ws, data) {
    let envelope;
    try {
      envelope = JSON.parse(data.toString());
    } catch {
      this._send(ws, { type: 'error', payload: { message: 'Invalid JSON' } });
      return;
    }

    const { type, payload } = envelope;

    if (typeof type !== 'string') {
      this._send(ws, { type: 'error', payload: { message: 'Missing message type' } });
      return;
    }

    try {
      switch (type) {
        case 'register':
          this._handleRegister(clientId, ws, payload || {});
          break;
        case 'score':
          this._handleScore(clientId, ws, payload || {});
          break;
        default:
          this._send(ws, { type: 'error', payload: { message: 'Unknown message type' } });
      }
    } catch (err) {
      this._send(ws, { type: 'error', payload: { message: err.message } });
    }
  }

  _handleRegister(clientId, ws, payload) {
    const { type, playerName, playerId: reconnectId } = payload;

    if (!VALID_TYPES.has(type)) {
      this._send(ws, {
        type: 'error',
        payload: { message: `Invalid type. Must be one of: ${[...VALID_TYPES].join(', ')}` },
      });
      return;
    }

    const client = this.clients.get(clientId);
    client.type = type;

    // Sanitize name
    let sanitized = '';
    if (typeof playerName === 'string') {
      sanitized = playerName.replace(HTML_TAG_PATTERN, '').trim().slice(0, 20);
    }

    // Handle reconnect: if the client supplies a previously issued player ID and
    // the server still has that player, reuse the existing entry (mark connected)
    // instead of creating a duplicate.
    if (reconnectId && type !== 'display') {
      const existing = this.gameState.reconnectPlayer(reconnectId);
      if (existing) {
        // Accept a name update if the client provided one
        if (sanitized.length > 0) existing.name = sanitized;
        client.playerId = reconnectId;
        this._send(ws, {
          type: 'registered',
          payload: { id: reconnectId, name: existing.name, playerType: type },
        });
        this.broadcastState();
        return;
      }
    }

    // Normal (first-time) registration
    const name = sanitized.length > 0 ? sanitized : this.gameState.assignName();

    if (type !== 'display') {
      this.gameState.addPlayer(clientId, name, type);
      client.playerId = clientId;
    } else {
      client.playerId = null;
    }

    this._send(ws, {
      type: 'registered',
      payload: { id: clientId, name, playerType: type },
    });

    this.broadcastState();
  }

  _handleScore(clientId, ws, payload) {
    const { playerId, points } = payload;

    if (!playerId || (points !== 0 && points !== 1 && points !== 2 && points !== 3)) {
      this._send(ws, {
        type: 'error',
        payload: { message: 'Invalid score payload: playerId required, points must be 0, 1, 2, or 3' },
      });
      return;
    }

    try {
      const result = this.gameState.score(playerId, points);
      const { player, winner, events } = result;

      this.broadcastScored(player, points, events);
      this.broadcastPositions();
      this.broadcastState();

      if (winner) {
        this.broadcastWinner(player);
      }
    } catch (err) {
      this._send(ws, { type: 'error', payload: { message: err.message } });
    }
  }

  _handleDisconnect(clientId) {
    // Use the player's stable ID (which may differ from the WS clientId when the
    // client reconnected and the server reused an existing player entry).
    const client = this.clients.get(clientId);
    const playerId = client ? client.playerId : null;
    this.clients.delete(clientId);
    if (playerId) this.gameState.disconnectPlayer(playerId);
    this.broadcastState();
  }
}

module.exports = ConnectionManager;
