const express = require('express');
const path = require('path');
const rateLimit = require('express-rate-limit');

/**
 * LED Configuration and Control Routes
 * Provides REST API for LED configuration management and effect testing.
 *
 * Effect names are validated against the shared manifest at
 * clients/shared/led-effects-manifest.json — single source of truth.
 *
 * @module routes/leds
 * @requires express
 * @requires express-rate-limit
 */

// Load the shared manifest once at startup. Hard-coded lists must not exist here.
const MANIFEST_PATH = path.resolve(__dirname, '..', '..', '..', 'clients', 'shared', 'led-effects-manifest.json');
let _validEffects;
try {
  const manifest = require(MANIFEST_PATH);
  _validEffects = manifest.effects.map((e) => e.name);
} catch (err) {
  // Fallback so server still boots if manifest is missing; log prominently.
  console.error('[LED Routes] WARN: could not load led-effects-manifest.json —', err.message);
  _validEffects = ['solid', 'blink', 'pulse', 'rainbow', 'chase', 'sparkle',
    'countdown', 'text', 'winner', 'ballroll', 'clear'];
}

// Maximum allowed durationMs for test effects.
// Firmware stores test_effect.durationMs as uint32_t (up to ~49 days).
// Cap server-side at 1 hour to prevent accidental indefinitely-sticky effects.
const MAX_EFFECT_DURATION_MS = 60 * 60 * 1000;

// Rate limiter for effect test endpoint: 1 request per second per device

const effectTestLimiter = rateLimit({
  windowMs: 1000, // 1 second
  max: 1,
  keyGenerator: (req) => {
    // Rate limit per deviceId
    return req.body?.deviceId || req.ip;
  },
  handler: (req, res) => {
    res.status(429).json({
      error: 'Too many requests',
      message: 'Effect test rate limit: 1 request per second per device'
    });
  }
});

/**
 * Create LED routes router
 * @param {LedConfigManager} ledConfigManager - Configuration manager instance
 * @param {ConnectionManager} connectionManager - WebSocket connection manager instance
 * @returns {express.Router} Express router with LED endpoints
 */
function createLedRoutes(ledConfigManager, connectionManager) {
  const router = express.Router();

  /**
   * GET /api/leds/config
   * Get all LED configurations for all device types
   */
  router.get('/config', (req, res) => {
    try {
      const config = ledConfigManager.getAllConfigs();
      res.json(config);
    } catch (error) {
      console.error('[LED Routes] Error getting config:', error);
      res.status(500).json({
        error: 'Internal server error',
        message: error.message
      });
    }
  });

  /**
   * GET /api/leds/config/:deviceType
   * Get LED configuration for a specific device type
   */
  router.get('/config/:deviceType', (req, res) => {
    try {
      const { deviceType } = req.params;
      const config = ledConfigManager.getConfigForDeviceType(deviceType);
      
      if (!config) {
        return res.status(404).json({
          error: 'Not found',
          message: `No configuration found for device type: ${deviceType}`
        });
      }
      
      res.json(config);
    } catch (error) {
      console.error('[LED Routes] Error getting device config:', error);
      res.status(500).json({
        error: 'Internal server error',
        message: error.message
      });
    }
  });

  /**
   * PUT /api/leds/config/:deviceType
   * Update LED configuration for a specific device type
   */
  router.put('/config/:deviceType', async (req, res) => {
    try {
      const { deviceType } = req.params;
      const deviceConfig = req.body;
      
      // Validate payload structure
      if (!deviceConfig || typeof deviceConfig !== 'object') {
        return res.status(400).json({
          error: 'Bad request',
          message: 'Request body must be a valid configuration object'
        });
      }
      
      // Required fields validation
      const requiredFields = ['ledCount', 'topology', 'gpioPin', 'brightness', 'defaultEffect'];
      const missingFields = requiredFields.filter(field => !(field in deviceConfig));
      
      if (missingFields.length > 0) {
        return res.status(400).json({
          error: 'Bad request',
          message: `Missing required fields: ${missingFields.join(', ')}`
        });
      }
      
      // Update configuration (validation happens in LedConfigManager)
      await ledConfigManager.updateDeviceConfig(deviceType, deviceConfig);

      console.log(`[LED Routes] Config updated for ${deviceType}`);
      
      // Broadcast new configuration to connected devices
      connectionManager.broadcastLedConfig(deviceType, deviceConfig);
      
      res.json({
        success: true,
        message: `Configuration updated for device type: ${deviceType}`,
        config: deviceConfig
      });
      
    } catch (error) {
      console.error('[LED Routes] Error updating config:', error);
      
      // Validation errors return 400
      if (error.message.includes('must be') || error.message.includes('Invalid')) {
        return res.status(400).json({
          error: 'Validation error',
          message: error.message
        });
      }
      
      // Other errors return 500
      res.status(500).json({
        error: 'Internal server error',
        message: error.message
      });
    }
  });

  /**
   * GET /api/leds/config/:deviceType/:chipId
   * Get the effective LED config for a specific device (per-device override or type fallback).
   */
  router.get('/config/:deviceType/:chipId', (req, res) => {
    try {
      const { deviceType, chipId } = req.params;
      const config = ledConfigManager.getConfigForDevice(deviceType, chipId);
      if (!config) {
        return res.status(404).json({
          error: 'Not found',
          message: `No configuration found for device type: ${deviceType}`
        });
      }
      const overrides = ledConfigManager.getAllConfigs().deviceConfigOverrides || {};
      const hasOverride = !!overrides[`${deviceType}/${chipId}`];
      res.json({ config, hasOverride });
    } catch (error) {
      console.error('[LED Routes] Error getting device config:', error);
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  /**
   * PUT /api/leds/config/:deviceType/:chipId
   * Set a per-device LED config override for a specific chip.
   */
  router.put('/config/:deviceType/:chipId', async (req, res) => {
    try {
      const { deviceType, chipId } = req.params;
      const deviceConfig = req.body;

      if (!deviceConfig || typeof deviceConfig !== 'object') {
        return res.status(400).json({
          error: 'Bad request',
          message: 'Request body must be a valid configuration object'
        });
      }

      const requiredFields = ['ledCount', 'topology', 'gpioPin', 'brightness', 'defaultEffect'];
      const missingFields = requiredFields.filter((field) => !(field in deviceConfig));
      if (missingFields.length > 0) {
        return res.status(400).json({
          error: 'Bad request',
          message: `Missing required fields: ${missingFields.join(', ')}`
        });
      }

      await ledConfigManager.updateDeviceOverride(deviceType, chipId, deviceConfig);

      console.log(`[LED Routes] Override set for ${deviceType}/${chipId}`);

      // Send the override directly to the specific device (if connected)
      for (const client of connectionManager.clients.values()) {
        if (client.chipId === chipId && client.type === deviceType && client.ws.readyState === 1) {
          const payload = { ...deviceConfig };
          if (client.playerId) {
            const player = connectionManager.gameState.players.get(client.playerId);
            if (player && player.colorIndex !== undefined) {
              payload.deviceColor = ledConfigManager.getColorHex(player.colorIndex);
            }
          }
          client.ws.send(JSON.stringify({ type: 'led_config', timestamp: Date.now(), payload }));
          break;
        }
      }

      res.json({
        success: true,
        message: `Per-device config override set for ${deviceType}/${chipId}`,
        config: deviceConfig
      });
    } catch (error) {
      console.error('[LED Routes] Error setting device config override:', error);
      if (error.message.includes('must be') || error.message.includes('Invalid')) {
        return res.status(400).json({ error: 'Validation error', message: error.message });
      }
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  /**
   * DELETE /api/leds/config/:deviceType/:chipId
   * Remove the per-device override, reverting the device to the type-wide config.
   */
  router.delete('/config/:deviceType/:chipId', async (req, res) => {
    try {
      const { deviceType, chipId } = req.params;
      const removed = await ledConfigManager.deleteDeviceOverride(deviceType, chipId);
      if (!removed) {
        return res.status(404).json({
          error: 'Not found',
          message: `No per-device override exists for ${deviceType}/${chipId}`
        });
      }

      // Push the (now reverted) type-wide config to the device if connected
      for (const client of connectionManager.clients.values()) {
        if (client.chipId === chipId && client.type === deviceType && client.ws.readyState === 1) {
          const typeConfig = ledConfigManager.getConfigForDeviceType(deviceType);
          if (typeConfig) {
            const payload = { ...typeConfig };
            if (client.playerId) {
              const player = connectionManager.gameState.players.get(client.playerId);
              if (player && player.colorIndex !== undefined) {
                payload.deviceColor = ledConfigManager.getColorHex(player.colorIndex);
              }
            }
            client.ws.send(JSON.stringify({ type: 'led_config', timestamp: Date.now(), payload }));
          }
          break;
        }
      }

      res.json({ success: true, message: `Per-device override removed for ${deviceType}/${chipId}` });
    } catch (error) {
      console.error('[LED Routes] Error deleting device config override:', error);
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  // ─── Device Color Management ────────────────────────────────────────────────

  /**
   * GET /api/leds/device-colors
   * Returns the persistent deviceColorMap and the full palette.
   */
  router.get('/device-colors', (req, res) => {
    try {
      res.json({
        deviceColorMap: ledConfigManager.getDeviceColorMap(),
        palette: ledConfigManager.getPalette()
      });
    } catch (error) {
      console.error('[LED Routes] Error getting device colors:', error);
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  /**
   * PUT /api/leds/device-colors/:chipId
   * Admin override: assign a specific color to a device.
   * Body: { colorIndex: 0-15 }
   * Re-broadcasts led_config to the affected device.
   */
  router.put('/device-colors/:chipId', async (req, res) => {
    try {
      const { chipId } = req.params;
      const { colorIndex } = req.body;

      if (typeof colorIndex !== 'number' || !Number.isInteger(colorIndex) || colorIndex < 0 || colorIndex > 15) {
        return res.status(400).json({ error: 'colorIndex must be an integer 0–15' });
      }

      await ledConfigManager.updateDeviceColor(chipId, colorIndex);

      // Find the connected client with this chipId and update its player + re-send led_config
      for (const client of connectionManager.clients.values()) {
        if (client.chipId === chipId && client.playerId) {
          const player = connectionManager.gameState.players.get(client.playerId);
          if (player) {
            player.colorIndex = colorIndex;
          }
          // Re-send led_config with new device color
          const ledConfig = ledConfigManager.getConfigForDeviceType(client.type);
          if (ledConfig) {
            const payload = { ...ledConfig, deviceColor: ledConfigManager.getColorHex(colorIndex) };
            if (client.ws.readyState === 1) {
              client.ws.send(JSON.stringify({ type: 'led_config', timestamp: Date.now(), payload }));
            }
          }
          break;
        }
      }

      // Broadcast updated state so admin/display clients see the new color
      connectionManager.broadcastState();

      res.json({
        success: true,
        chipId,
        colorIndex,
        colorHex: ledConfigManager.getColorHex(colorIndex)
      });
    } catch (error) {
      console.error('[LED Routes] Error updating device color:', error);
      if (error.message.includes('must be')) {
        return res.status(400).json({ error: error.message });
      }
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  /**
   * POST /api/leds/effects/test
   * Send test effect to a specific device
   */
  router.post('/effects/test', effectTestLimiter, (req, res) => {
    try {
      const { deviceId, effectName, params, durationMs } = req.body;
      
      // Validate required fields
      if (!deviceId) {
        return res.status(400).json({
          error: 'Bad request',
          message: 'deviceId is required'
        });
      }
      
      if (!effectName) {
        return res.status(400).json({
          error: 'Bad request',
          message: 'effectName is required'
        });
      }
      
      // Validate effect name against shared manifest
      if (!_validEffects.includes(effectName)) {
        return res.status(400).json({
          error: 'Bad request',
          message: `Invalid effectName. Must be one of: ${_validEffects.join(', ')}`
        });
      }

      // Validate durationMs if provided
      const duration = durationMs !== undefined ? Number(durationMs) : 0;
      if (!Number.isInteger(duration) || duration < 0 || duration > MAX_EFFECT_DURATION_MS) {
        return res.status(400).json({
          error: 'Bad request',
          message: `durationMs must be an integer between 0 and ${MAX_EFFECT_DURATION_MS} (0 = indefinite)`
        });
      }
      
      // Check if device is connected
      const device = connectionManager.getDeviceById(deviceId);
      if (!device) {
        return res.status(404).json({
          error: 'Device not found',
          message: `Device ${deviceId} is not connected`
        });
      }
      
      // Send test effect to device
      const success = connectionManager.sendTestEffect(deviceId, effectName, params || {}, duration);
      
      if (success) {
        console.log(`[LED Routes] Effect test: ${effectName} sent to device ${deviceId}${duration ? ` (TTL ${duration}ms)` : ''}`);
        res.json({
          success: true,
          message: `Test effect '${effectName}' sent to device ${deviceId}`,
          durationMs: duration
        });
      } else {
        res.status(500).json({
          error: 'Internal server error',
          message: 'Failed to send test effect to device'
        });
      }
      
    } catch (error) {
      console.error('[LED Routes] Error sending test effect:', error);
      res.status(500).json({
        error: 'Internal server error',
        message: error.message
      });
    }
  });

  /**
   * POST /api/leds/effects/stop
   * Stop any active test effect on a specific device, restoring the ambient state.
   */
  router.post('/effects/stop', (req, res) => {
    try {
      const { deviceId } = req.body;
      if (!deviceId) {
        return res.status(400).json({ error: 'Bad request', message: 'deviceId is required' });
      }
      const device = connectionManager.getDeviceById(deviceId);
      if (!device) {
        return res.status(404).json({
          error: 'Device not found',
          message: `Device ${deviceId} is not connected`
        });
      }
      const success = connectionManager.sendStopEffect(deviceId);
      if (success) {
        console.log(`[LED Routes] stop_effect sent to device ${deviceId}`);
        res.json({ success: true, message: `stop_effect sent to device ${deviceId}` });
      } else {
        res.status(500).json({ error: 'Internal server error', message: 'Failed to send stop_effect to device' });
      }
    } catch (error) {
      console.error('[LED Routes] Error sending stop_effect:', error);
      res.status(500).json({ error: 'Internal server error', message: error.message });
    }
  });

  /**
   * GET /api/leds/effects
   * Return the full effects manifest so clients can build dropdowns and
   * run validation without duplicating the list.
   */
  router.get('/effects', (req, res) => {
    try {
      const manifest = require(MANIFEST_PATH);
      res.json(manifest);
    } catch (err) {
      res.status(500).json({ error: 'Could not load effects manifest', message: err.message });
    }
  });

  return router;
}

module.exports = createLedRoutes;
