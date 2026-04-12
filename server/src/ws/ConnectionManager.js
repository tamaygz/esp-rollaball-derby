'use strict';

const { randomUUID } = require('crypto');

const VALID_TYPES    = new Set(['sensor', 'web', 'motor', 'display']);
const HARDWARE_TYPES = new Set(['sensor', 'motor']);
const HTML_TAG_PATTERN = /<[^>]*>/g;
const CHIPID_PATTERN   = /^[A-Fa-f0-9]{4,16}$/;

const AUTO_RESET_DELAY_MS = 15_000;
const COUNTDOWN_TICK_MS   = 1_000;
const COUNTDOWN_GO_HOLD_MS = 600;

class ConnectionManager {
  constructor(gameState, ledConfigManager = null, soundManager = null) {
    this.gameState       = gameState;
    this.ledConfigManager = ledConfigManager;
    this._soundManager   = soundManager;
    this.clients         = new Map(); // id → { ws, type, playerId, id, chipId, chipType, reportedLedCount }
    this._chipIdToPlayerId = new Map(); // chipId → playerId  (survives WS disconnects)
    this._botManager     = null;
    this._autoResetTimer = null;
    this._countingDown   = false;
    this._countdownGen   = 0;
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

  cancelCountdown() {
    if (this._countingDown) {
      this._countdownGen++;
      this._countingDown = false;
    }
  }

  /**
   * Validate the game can start, then either start immediately (countdown=0)
   * or broadcast countdown ticks asynchronously before transitioning to running.
   * Throws synchronously if the game cannot be started.
   */
  startWithCountdown(botManager) {
    if (this._countingDown) {
      throw new Error('Countdown already in progress');
    }
    this.gameState.canStart(); // throws if invalid

    const countdown = this.gameState.config.countdown;
    if (countdown === 0) {
      this.gameState.start();
      if (botManager) botManager.onGameStart();
      this.broadcastGameEvent('game_started');
      this.broadcastState();
      return;
    }

    this._countingDown = true;
    const gen = ++this._countdownGen;
    this._runCountdown(countdown, botManager, gen);
  }

  async _runCountdown(countdown, botManager, gen) {
    const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
    try {
      for (let i = countdown; i >= 1; i--) {
        if (this._countdownGen !== gen) return;
        this.broadcastAll({ type: 'countdown', payload: { count: i } });
        this._soundManager?.play('countdown_tick');
        await wait(COUNTDOWN_TICK_MS);
      }
      if (this._countdownGen !== gen) return;
      this.broadcastAll({ type: 'countdown', payload: { count: 0 } });
      this._soundManager?.play('countdown_go');
      await wait(COUNTDOWN_GO_HOLD_MS);
      if (this._countdownGen !== gen) return;

      this.gameState.start();
      if (botManager) botManager.onGameStart();
      this.broadcastGameEvent('game_started');
      this.broadcastState();
    } catch (err) {
      console.error('[Derby] Countdown aborted:', err.message);
      try { this.broadcastState(); } catch (_) { /* ignore */ }
    } finally {
      if (this._countdownGen === gen) this._countingDown = false;
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

  _getDeviceList() {
    const devices = [];
    for (const client of this.clients.values()) {
      if (client.type !== 'sensor' && client.type !== 'motor') continue;
      const player = client.playerId
        ? this.gameState.players.get(client.playerId)
        : null;
      const entry = {
        id: client.id,
        type: client.type,
        name: player ? player.name : (client.chipType || 'Unknown'),
        chipId: client.chipId || null,
        colorIndex: player ? player.colorIndex : null,
        ledCount: client.reportedLedCount || 0,
        connected: client.ws.readyState === 1,
      };
      if (client.type === 'motor') {
        entry.motorCount   = client.motorCount   || 0;
        entry.motorColors  = client.motorColors  || [];
        entry.capabilities = client.capabilities || {};
      }
      devices.push(entry);
    }
    return devices;
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
    state.devices = this._getDeviceList();
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
    // Play the most significant sound: lead/last/streak events take priority over score.
    const PRIORITY_EVENTS = ['took_lead', 'became_last', 'streak_three', 'streak_zero'];
    const soundEvent = (events || []).find((e) => PRIORITY_EVENTS.includes(e)) || `score_${points}`;
    this._soundManager?.play(soundEvent);

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

  /**
   * Broadcast a game lifecycle event to all connected clients.
   * Used for events that affect all devices (start, pause, resume, reset).
   * @param {'game_started'|'game_paused'|'game_resumed'|'game_reset'} event
   */
  broadcastGameEvent(event) {
    this._soundManager?.play(event);
    this.broadcastAll({ type: 'game_event', payload: { event } });
  }

  broadcastWinner(player) {
    this._soundManager?.play('winner');
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
      this.broadcastGameEvent('game_reset');
      this.broadcastState();
      this.broadcastPositions();
    }, AUTO_RESET_DELAY_MS);
  }

  /**
   * Broadcast LED configuration to all devices of a specific type
   * @param {string} deviceType - Device type (sensor, motor, display)
   * @param {Object} config - LED configuration object
   */
  broadcastLedConfig(deviceType, config) {
    const startTime = Date.now();

    let sentCount = 0;
    for (const client of this.clients.values()) {
      if (client.type === deviceType && client.ws.readyState === 1 /* OPEN */) {
        const payload = { ...config };
        // Include per-device color if available
        if (this.ledConfigManager && client.playerId) {
          const player = this.gameState.players.get(client.playerId);
          if (player && player.colorIndex !== undefined) {
            payload.deviceColor = this.ledConfigManager.getColorHex(player.colorIndex);
          }
        }
        client.ws.send(JSON.stringify({
          type: 'led_config',
          timestamp: startTime,
          payload
        }));
        sentCount++;
      }
    }

    const elapsed = Date.now() - startTime;
    if (sentCount === 0) {
      console.log(`[ConnectionManager] LED config broadcast: no ${deviceType} devices connected`);
    } else {
      console.log(`[ConnectionManager] LED config broadcast: ${sentCount} ${deviceType} device(s) updated in ${elapsed}ms`);
    }
  }

  /**
   * Send test effect to a specific device
   * @param {string} deviceId - Device client ID
   * @param {string} effectName - Effect name
   * @param {Object} params - Effect parameters
   * @returns {boolean} Success status
   */
  sendTestEffect(deviceId, effectName, params) {
    const client = this.clients.get(deviceId);
    if (!client || client.ws.readyState !== 1 /* OPEN */) {
      return false;
    }

    const msg = JSON.stringify({
      type: 'test_effect',
      payload: {
        effectName,
        params
      }
    });

    try {
      client.ws.send(msg);
      return true;
    } catch (error) {
      console.error(`[ConnectionManager] Failed to send test effect to ${deviceId}:`, error.message);
      return false;
    }
  }

  /**
   * Get device by client ID
   * @param {string} deviceId - Device client ID
   * @returns {Object|null} Client object or null
   */
  getDeviceById(deviceId) {
    return this.clients.get(deviceId) || null;
  }

  // ─── Private ──────────────────────────────────────────────────────────────────

  _send(ws, msg) {
    if (ws.readyState === 1) {
      ws.send(JSON.stringify(msg));
    }
  }

  /**
   * Shared helper for both playerId- and chipId-based reconnect paths.
   * Restores name/color, sends 'registered' + LED config, and broadcasts state.
   */
  _finalizeReconnect(ws, client, existing, playerId, { type, chipId, sanitized, ledCount, chipType }) {
    // Restore / update name
    if (sanitized.length > 0) {
      existing.name = sanitized;
      if (this.ledConfigManager && chipId) {
        this.ledConfigManager.setDeviceName(chipId, sanitized);
      }
    } else if (this.ledConfigManager && chipId) {
      const persisted = this.ledConfigManager.getDeviceName(chipId);
      if (persisted) existing.name = persisted;
    }

    client.playerId = playerId;

    // Close / remove any stale client entries that still reference this player.
    // This prevents duplicate score messages from lingering sockets.
    for (const [cid, c] of this.clients.entries()) {
      if (cid !== client.id && c.playerId === playerId) {
        c.playerId = null;            // detach so _handleDisconnect won't touch the player
        try { c.ws.close(); } catch (_) { /* ignore */ }
        this.clients.delete(cid);
      }
    }

    // Track chipId → playerId for future reconnects (hardware types only)
    if (chipId && HARDWARE_TYPES.has(type)) {
      this._chipIdToPlayerId.set(chipId, playerId);
    }

    // Assign / restore device color
    if (this.ledConfigManager) {
      const isHardwareDevice = HARDWARE_TYPES.has(type);

      if (!isHardwareDevice) {
        existing.colorIndex = this.ledConfigManager.assignColor(null);
      } else if (chipId) {
        existing.colorIndex = this.ledConfigManager.assignColor(chipId);
      }
    }

    const response = {
      type: 'registered',
      payload: { id: playerId, name: existing.name, playerType: type, colorIndex: existing.colorIndex }
    };

    if (this.ledConfigManager && typeof ledCount === 'number' && chipType) {
      const validation = this.ledConfigManager.validateDeviceLedCount(type, ledCount, chipType);
      if (!validation.valid && validation.warning) {
        response.payload.warning = validation.warning;
      }
      console.log(`[ConnectionManager] Device ${playerId} reconnected: ${chipType}, ${ledCount} LEDs detected`);
    }

    this._send(ws, response);
    this.broadcastState();

    // Motor color sync on reconnect: re-apply physical lane colors
    if (type === 'motor' && Array.isArray(client.motorColors) && client.motorColors.length > 0) {
      this._applyMotorColorSync(client);
    }

    // Send LED config to reconnected device (with device color)
    if (this.ledConfigManager && type !== 'display') {
      const ledConfig = this.ledConfigManager.getConfigForDeviceType(type);
      if (ledConfig && ledConfig.ledCount > 0) {
        const configPayload = { ...ledConfig };
        if (existing.colorIndex !== undefined) {
          configPayload.deviceColor = this.ledConfigManager.getColorHex(existing.colorIndex);
        }
        this._send(ws, {
          type: 'led_config',
          timestamp: Date.now(),
          payload: configPayload
        });
      }
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
        case 'button':
          this._handleButton(clientId, ws, payload || {});
          break;
        default:
          this._send(ws, { type: 'error', payload: { message: 'Unknown message type' } });
      }
    } catch (err) {
      this._send(ws, { type: 'error', payload: { message: err.message } });
    }
  }

  _handleRegister(clientId, ws, payload) {
    const { type, playerName, playerId: reconnectId, ledCount, chipType, chipId, motorCount, motorColors, capabilities } = payload;

    if (!VALID_TYPES.has(type)) {
      this._send(ws, {
        type: 'error',
        payload: { message: `Invalid type. Must be one of: ${[...VALID_TYPES].join(', ')}` },
      });
      return;
    }

    const client = this.clients.get(clientId);
    client.type = type;

    // Store LED metadata if provided
    if (typeof ledCount === 'number') {
      client.reportedLedCount = ledCount;
    }
    if (typeof chipType === 'string') {
      client.chipType = chipType;
    }
    // Store motor-client metadata
    if (type === 'motor') {
      if (typeof motorCount === 'number') {
        client.motorCount = motorCount;
      }
      if (Array.isArray(motorColors)) {
        client.motorColors = motorColors.map(Number).filter((n) => Number.isFinite(n));
      }
      if (capabilities && typeof capabilities === 'object') {
        client.capabilities = capabilities;
      }
    }
    // Only accept chipId from hardware device types and validate its format.
    const validatedChipId = (typeof chipId === 'string' && HARDWARE_TYPES.has(type) && CHIPID_PATTERN.test(chipId))
      ? chipId
      : undefined;
    if (validatedChipId) {
      client.chipId = validatedChipId;
    }

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
        this._finalizeReconnect(ws, client, existing, reconnectId, { type, chipId: validatedChipId, sanitized, ledCount, chipType });
        return;
      }
    }

    // Check whether this physical device was previously associated with a player
    // (via chipId).  Only hardware types (sensor/motor) participate in chipId
    // reconnect to prevent non-hardware clients from hijacking device identities.
    if (validatedChipId) {
      const previousPlayerId = this._chipIdToPlayerId.get(validatedChipId);
      if (previousPlayerId) {
        const existing = this.gameState.reconnectPlayer(previousPlayerId);
        if (existing) {
          this._finalizeReconnect(ws, client, existing, previousPlayerId, { type, chipId: validatedChipId, sanitized, ledCount, chipType });
          return;
        }
      }
    }

    // First-time registration — restore persisted name if known device,
    // otherwise auto-assign one and persist it for future sessions.
    const deviceChipIdForName = validatedChipId || null;
    let name;
    if (sanitized.length > 0) {
      name = sanitized;
      if (this.ledConfigManager && deviceChipIdForName) {
        this.ledConfigManager.setDeviceName(deviceChipIdForName, name);
      }
    } else {
      const persistedName = this.ledConfigManager && deviceChipIdForName
        ? this.ledConfigManager.getDeviceName(deviceChipIdForName)
        : null;
      if (persistedName) {
        name = persistedName;
      } else {
        name = this.gameState.assignName();
        if (this.ledConfigManager && deviceChipIdForName) {
          this.ledConfigManager.setDeviceName(deviceChipIdForName, name);
        }
      }
    }

    // Assign device color
    let colorIndex = 0;
    if (this.ledConfigManager) {
      colorIndex = this.ledConfigManager.assignColor(deviceChipIdForName);
    }

    if (type !== 'display') {
      this.gameState.addPlayer(clientId, name, type, colorIndex);
      client.playerId = clientId;
      // Record chipId → playerId so the device can reconnect after a reboot
      if (validatedChipId) {
        this._chipIdToPlayerId.set(validatedChipId, clientId);
      }
      // Motor color sync: if the motor client reports physical lane colors, override
      // the auto-assigned color with the first available motor lane color.
      if (type === 'motor' && Array.isArray(client.motorColors) && client.motorColors.length > 0) {
        this._applyMotorColorSync(client);
        colorIndex = this.gameState.players.get(clientId)?.colorIndex ?? colorIndex;
      }
    } else {
      client.playerId = null;
    }

    // Build response with LED validation warning if applicable
    const response = {
      type: 'registered',
      payload: { id: clientId, name, playerType: type, colorIndex }
    };

    // Validate LED count and log device registration
    if (this.ledConfigManager && typeof ledCount === 'number' && chipType) {
      const validation = this.ledConfigManager.validateDeviceLedCount(type, ledCount, chipType);
      if (!validation.valid && validation.warning) {
        response.payload.warning = validation.warning;
      }
      console.log(`[ConnectionManager] Device ${clientId} registered: ${chipType}, ${ledCount} LEDs detected`);
    }

    this._send(ws, response);
    this.broadcastState();

    // Send LED config to newly registered device (with device color)
    if (this.ledConfigManager && type !== 'display') {
      const ledConfig = this.ledConfigManager.getConfigForDeviceType(type);
      if (ledConfig && ledConfig.ledCount > 0) {
        const configPayload = { ...ledConfig };
        if (colorIndex !== undefined) {
          configPayload.deviceColor = this.ledConfigManager.getColorHex(colorIndex);
        }
        this._send(ws, {
          type: 'led_config',
          timestamp: Date.now(),
          payload: configPayload
        });
      }
    }
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

  _handleButton(clientId, ws, payload) {
    const client = this.clients.get(clientId);
    if (!client || client.type !== 'motor') {
      this._send(ws, { type: 'error', payload: { message: 'button messages only accepted from motor clients' } });
      return;
    }

    const VALID_ACTIONS = new Set(['start', 'reset', 'pause', 'resume']);
    const { action } = payload;

    if (!VALID_ACTIONS.has(action)) {
      this._send(ws, { type: 'error', payload: { message: `Invalid button action. Must be one of: ${[...VALID_ACTIONS].join(', ')}` } });
      return;
    }

    try {
      switch (action) {
        case 'start':
          this.startWithCountdown(this._botManager);
          break;
        case 'reset':
          this.cancelCountdown();
          this.cancelAutoReset();
          if (this._botManager) this._botManager.onGameReset();
          this.gameState.reset();
          this.broadcastGameEvent('game_reset');
          this.broadcastState();
          this.broadcastPositions();
          break;
        case 'pause':
        case 'resume': {
          const newStatus = this.gameState.pause(); // toggle: running↔paused
          this.broadcastGameEvent(newStatus === 'paused' ? 'game_paused' : 'game_resumed');
          this.broadcastState();
          break;
        }
      }
    } catch (err) {
      this._send(ws, { type: 'error', payload: { message: err.message } });
    }
  }

  /**
   * Apply motor color sync: override each newly registered motor-client player's
   * colorIndex to match the physical lane color reported in motorColors.
   * Only called for the motor player itself; subsequent player join auto-assigns
   * from the motor lane color pool when available.
   * @param {object} client - The motor client record
   */
  _applyMotorColorSync(client) {
    if (!client.playerId || !Array.isArray(client.motorColors) || client.motorColors.length === 0) return;
    // Use the first lane's color as the motor controller's own identity color.
    const firstLaneColor = client.motorColors[0];
    if (typeof firstLaneColor !== 'number') return;
    try {
      this.gameState.setPlayerColorIndex(client.playerId, firstLaneColor);
      if (this.ledConfigManager && client.chipId) {
        this.ledConfigManager.assignColor(client.chipId, firstLaneColor);
      }
    } catch (_) { /* player may not exist yet — ignore */ }
  }

  _handleDisconnect(clientId) {
    // Use the player's stable ID (which may differ from the WS clientId when the
    // client reconnected and the server reused an existing player entry).
    const client = this.clients.get(clientId);
    const playerId = client ? client.playerId : null;
    this.clients.delete(clientId);

    // Only disconnect the player if no other active client is now responsible for
    // it.  This prevents a stale WS close from marking a player disconnected after
    // the device already reconnected on a new socket.
    if (playerId) {
      let takenOver = false;
      for (const c of this.clients.values()) {
        if (c.playerId === playerId) { takenOver = true; break; }
      }
      if (!takenOver) {
        this.gameState.disconnectPlayer(playerId);
      }
    }
    this.broadcastState();
  }
}

module.exports = ConnectionManager;
