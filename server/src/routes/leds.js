const express = require('express');
const rateLimit = require('express-rate-limit');

/**
 * LED Configuration and Control Routes
 * Provides REST API for LED configuration management and effect testing
 * 
 * @module routes/leds
 * @requires express
 * @requires express-rate-limit
 */

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
   * POST /api/leds/config/sync-all
   * Re-broadcast current LED config to all connected devices
   */
  router.post('/config/sync-all', (req, res) => {
    try {
      const config = ledConfigManager.config || {};
      let totalSent = 0;

      for (const deviceType of Object.keys(config)) {
        if (deviceType === 'deviceColorMap') continue;
        connectionManager.broadcastLedConfig(deviceType, config[deviceType]);
        totalSent++;
      }

      console.log(`[LED Routes] Sync-all: broadcast config for ${totalSent} device type(s)`);
      res.json({ success: true, message: `Configuration synced to ${totalSent} device type(s)` });
    } catch (error) {
      console.error('[LED Routes] Error syncing config:', error);
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
      const { deviceId, effectName, params } = req.body;
      
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
      
      // Validate effect name
      const validEffects = ['solid', 'blink', 'pulse', 'rainbow', 'chase', 'sparkle'];
      if (!validEffects.includes(effectName)) {
        return res.status(400).json({
          error: 'Bad request',
          message: `Invalid effectName. Must be one of: ${validEffects.join(', ')}`
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
      const success = connectionManager.sendTestEffect(deviceId, effectName, params || {});
      
      if (success) {
        console.log(`[LED Routes] Effect test: ${effectName} sent to device ${deviceId}`);
        res.json({
          success: true,
          message: `Test effect '${effectName}' sent to device ${deviceId}`
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

  return router;
}

module.exports = createLedRoutes;
