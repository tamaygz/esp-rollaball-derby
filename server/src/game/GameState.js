'use strict';

const fs = require('fs');
const path = require('path');

const VALID_THEMES = ['horse', 'camel'];
const RATE_LIMIT_MS = 300;

class GameState {
  constructor() {
    this.status = 'idle';
    this.config = {
      trackLength: 15,
      maxPlayers: 16,
      theme: 'horse',
    };
    this.players = new Map();
    this.startedAt = null;

    this._usedNames = new Set();
    this._allNames = this._loadNames();
    this._nameCounter = 0;
  }

  // ─── Name management ──────────────────────────────────────────────────────

  _loadNames() {
    try {
      const filePath = path.join(__dirname, '..', '..', 'data', 'names.txt');
      const raw = fs.readFileSync(filePath, 'utf8');
      return raw
        .split('\n')
        .map((n) => n.trim())
        .filter(Boolean);
    } catch {
      return [];
    }
  }

  assignName() {
    const available = this._allNames.filter((n) => !this._usedNames.has(n));
    if (available.length === 0) {
      this._nameCounter += 1;
      return `Player_${this._nameCounter}`;
    }
    const name = available[Math.floor(Math.random() * available.length)];
    this._usedNames.add(name);
    return name;
  }

  // ─── State accessors ──────────────────────────────────────────────────────

  getStatus() {
    return this.status;
  }

  getConfig() {
    return { ...this.config };
  }

  toJSON() {
    return {
      status: this.status,
      config: { ...this.config },
      players: [...this.players.values()].map((p) => ({ ...p })),
      startedAt: this.startedAt,
    };
  }

  // ─── Lifecycle transitions ────────────────────────────────────────────────

  start() {
    if (this.status !== 'idle') {
      throw new Error(`Cannot start: game is '${this.status}', must be 'idle'`);
    }
    const connected = [...this.players.values()].filter((p) => p.connected);
    if (connected.length === 0) {
      throw new Error('Cannot start: no players connected');
    }
    this.status = 'running';
    this.startedAt = Date.now();
  }

  pause() {
    if (this.status === 'running') {
      this.status = 'paused';
    } else if (this.status === 'paused') {
      this.status = 'running';
    } else {
      throw new Error(`Cannot pause: game is '${this.status}'`);
    }
    return this.status;
  }

  reset() {
    this.status = 'idle';
    this.startedAt = null;
    this._usedNames.clear();
    this._nameCounter = 0;
    for (const player of this.players.values()) {
      player.position = 0;
      player.lastScoredAt = null;
    }
    return this.toJSON();
  }

  finish(winnerId) {
    this.status = 'finished';
    this._winnerId = winnerId;
  }

  // ─── Config ───────────────────────────────────────────────────────────────

  updateConfig(updates) {
    if (this.status !== 'idle') {
      throw new Error('Cannot update config while game is running');
    }

    const merged = { ...this.config };

    if ('trackLength' in updates) {
      const v = updates.trackLength;
      if (typeof v !== 'number' || !Number.isInteger(v) || v < 5 || v > 50) {
        throw new Error('trackLength must be an integer between 5 and 50');
      }
      merged.trackLength = v;
    }

    if ('maxPlayers' in updates) {
      const v = updates.maxPlayers;
      if (typeof v !== 'number' || !Number.isInteger(v) || v < 1 || v > 16) {
        throw new Error('maxPlayers must be an integer between 1 and 16');
      }
      merged.maxPlayers = v;
    }

    if ('theme' in updates) {
      const v = updates.theme;
      if (typeof v !== 'string') {
        throw new Error('theme must be a string');
      }
      merged.theme = v;
    }

    this.config = merged;
    return this.getConfig();
  }

  // ─── Player management ────────────────────────────────────────────────────

  addPlayer(id, name, type) {
    const player = {
      id,
      name,
      position: 0,
      type,
      connected: true,
      connectedAt: Date.now(),
      lastScoredAt: null,
    };
    this.players.set(id, player);
    return player;
  }

  removePlayer(id) {
    if (this.status === 'idle') {
      this.players.delete(id);
    } else {
      this.disconnectPlayer(id);
    }
  }

  disconnectPlayer(id) {
    const player = this.players.get(id);
    if (!player) return;
    if (this.status === 'idle') {
      this.players.delete(id);
    } else {
      player.connected = false;
    }
  }

  /**
   * Mark an existing player as connected again (used when a client reconnects
   * and supplies its previously issued player ID).
   *
   * @param {string} id - The player ID issued on the original registration.
   * @returns {object|null} The player object, or null if no such player exists.
   */
  reconnectPlayer(id) {
    const player = this.players.get(id);
    if (!player) return null;
    player.connected = true;
    player.connectedAt = Date.now();
    return player;
  }

  renamePlayer(id, name) {
    const player = this.players.get(id);
    if (!player) {
      throw new Error('Player not found');
    }
    player.name = name;
    return player;
  }

  // ─── Scoring ──────────────────────────────────────────────────────────────

  score(playerId, points) {
    if (this.status !== 'running') {
      throw new Error(`Cannot score: game is '${this.status}', must be 'running'`);
    }

    if (points !== 1 && points !== 2 && points !== 3) {
      throw new Error('Points must be 1, 2, or 3');
    }

    const player = this.players.get(playerId);
    if (!player) {
      throw new Error('Player not found');
    }
    if (!player.connected) {
      throw new Error('Player is not connected');
    }

    const now = Date.now();
    if (player.lastScoredAt !== null && now - player.lastScoredAt < RATE_LIMIT_MS) {
      throw new Error('rate limited');
    }

    player.position += points;
    player.lastScoredAt = now;

    if (player.position >= this.config.trackLength) {
      this.finish(playerId);
      return { player, winner: true };
    }

    return { player, winner: false };
  }
}

module.exports = GameState;
