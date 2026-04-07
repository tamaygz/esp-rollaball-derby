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

/**
 * Create LED routes router
 * @param {LedConfigManager} ledConfigManager - Configuration manager instance
 * @param {ConnectionManager} connectionManager - WebSocket connection manager instance
 * @returns {express.Router} Express router with LED endpoints
 */
function createLedRoutes(ledConfigManager, connectionManager) {
  const router = express.Router();

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
