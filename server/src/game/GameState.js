'use strict';

const fs = require('fs');
const path = require('path');

const CONCRETE_THEMES = ['horse', 'camel'];
const VALID_THEMES = [...CONCRETE_THEMES, 'auto'];
const RATE_LIMIT_MS = 300;
const STREAK_ZERO_THRESHOLD  = 3;  // consecutive zeros to trigger streak_zero_3x
const STREAK_THREE_THRESHOLD = 2;  // consecutive +3s to trigger streak_three_2x

class GameState {
  constructor() {
    this.status = 'idle';
    this.config = {
      trackLength: 15,
      maxPlayers: 16,
      theme: 'horse',
      countdown: 0,
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

  canStart() {
    if (this.status !== 'idle') {
      throw new Error(`Cannot start: game is '${this.status}', must be 'idle'`);
    }
    const connected = [...this.players.values()].filter((p) => p.connected);
    if (connected.length === 0) {
      throw new Error('Cannot start: no players connected');
    }
  }

  start() {
    this.canStart();

    // Resolve 'auto' theme to a random concrete theme at game start
    if (this.config.theme === 'auto') {
      this.config.theme = CONCRETE_THEMES[Math.floor(Math.random() * CONCRETE_THEMES.length)];
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
      player.consecutiveZeros      = 0;
      player.consecutivePlusThrees = 0;
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

    if ('countdown' in updates) {
      const v = updates.countdown;
      if (v !== 0 && v !== 3 && v !== 5) {
        throw new Error('countdown must be 0, 3, or 5');
      }
      merged.countdown = v;
    }

    this.config = merged;
    return this.getConfig();
  }

  // ─── Player management ────────────────────────────────────────────────────

  addPlayer(id, name, type, colorIndex = 0) {
    const player = {
      id,
      name,
      position: 0,
      type,
      colorIndex,
      connected: true,
      connectedAt: Date.now(),
      lastScoredAt: null,
      consecutiveZeros:      0,
      consecutivePlusThrees: 0,
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

    if (points !== 0 && points !== 1 && points !== 2 && points !== 3) {
      throw new Error('Points must be 0, 1, 2, or 3');
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

    // ── Rankings before scoring (for rank-change detection) ──────────────────
    const ranksBefore   = this._computeRankings();
    const indexBefore   = ranksBefore.findIndex((p) => p.id === playerId);
    const wasFirst      = indexBefore === 0;
    const wasLast       = indexBefore === ranksBefore.length - 1;

    // ── Update streak counters ────────────────────────────────────────────────
    if (points === 0) {
      player.consecutiveZeros      += 1;
      player.consecutivePlusThrees  = 0;
    } else if (points === 3) {
      player.consecutiveZeros       = 0;
      player.consecutivePlusThrees += 1;
    } else {
      player.consecutiveZeros       = 0;
      player.consecutivePlusThrees  = 0;
    }

    // ── Apply score ───────────────────────────────────────────────────────────
    player.position    += points;
    player.lastScoredAt = now;

    // ── Build events array ────────────────────────────────────────────────────
    const events = [];

    if (points === 0)      events.push('zero_roll');
    else if (points === 1) events.push('score_1');
    else if (points === 2) events.push('score_2');
    else if (points === 3) events.push('score_3');

    if (player.consecutiveZeros      >= STREAK_ZERO_THRESHOLD)  events.push('streak_zero_3x');
    if (player.consecutivePlusThrees >= STREAK_THREE_THRESHOLD) events.push('streak_three_2x');

    // Rank-change events only make sense with ≥2 players and a position change
    if (points > 0 && this.players.size >= 2) {
      const ranksAfter = this._computeRankings();
      const indexAfter = ranksAfter.findIndex((p) => p.id === playerId);
      const isFirst    = indexAfter === 0;
      const isLast     = indexAfter === ranksAfter.length - 1;

      if (isFirst && !wasFirst) events.push('took_lead');
      if (isLast  && !wasLast)  events.push('became_last');
    }

    // ── Winner check ──────────────────────────────────────────────────────────
    if (player.position >= this.config.trackLength) {
      this.finish(playerId);
      return { player, winner: true, events };
    }

    return { player, winner: false, events };
  }

  // ─── Rankings helper ──────────────────────────────────────────────────────

  /**
   * Returns connected players sorted by position descending.
   * @returns {{ id: string, position: number }[]}
   */
  _computeRankings() {
    return [...this.players.values()]
      .filter((p) => p.connected)
      .sort((a, b) => b.position - a.position);
  }
}

module.exports = GameState;
