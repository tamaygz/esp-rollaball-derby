const fs = require('fs/promises');
const path = require('path');
const { EventEmitter } = require('events');

/**
 * LED Configuration Manager
 * Handles persistence and validation of LED configurations per device type
 */
class LedConfigManager extends EventEmitter {
  constructor(configFilePath = path.join(__dirname, '../../data/led-config.json')) {
    super();
    this.configFilePath = configFilePath;
    this.config = null;
    
    // Default configuration fallback
    this.defaultConfig = {
      sensor: {
        ledCount: 10,
        topology: 'strip',
        gpioPin: 4,
        brightness: 128,
        defaultEffect: 'rainbow'
      },
      motor: {
        ledCount: 10,
        topology: 'strip',
        gpioPin: 4,
        brightness: 128,
        defaultEffect: 'chase'
      },
      display: {
        ledCount: 0,
        topology: 'strip',
        gpioPin: 4,
        brightness: 0,
        defaultEffect: 'solid'
      }
    };
  }

  /**
   * Load configuration from file
   * Falls back to default config if file missing or corrupted
   */
  async loadConfig() {
    try {
      const data = await fs.readFile(this.configFilePath, 'utf8');
      this.config = JSON.parse(data);
      console.log('[LedConfigManager] Configuration loaded from', this.configFilePath);
      return this.config;
    } catch (error) {
      if (error.code === 'ENOENT') {
        console.warn('[LedConfigManager] Config file not found, using defaults');
      } else {
        console.error('[LedConfigManager] Error loading config, using defaults:', error.message);
      }
      this.config = { ...this.defaultConfig };
      return this.config;
    }
  }

  /**
   * Save configuration to file with atomic write
   * @param {Object} config - Configuration object
   */
  async saveConfig(config) {
    const tempPath = this.configFilePath + '.tmp';
    
    try {
      // Validate before saving
      this._validateConfig(config);
      
      // Ensure directory exists
      const dir = path.dirname(this.configFilePath);
      await fs.mkdir(dir, { recursive: true });
      
      // Atomic write: write to temp file, then rename
      await fs.writeFile(tempPath, JSON.stringify(config, null, 2), 'utf8');
      await fs.rename(tempPath, this.configFilePath);
      
      this.config = config;
      console.log('[LedConfigManager] Configuration saved at', new Date().toISOString());
      
      // Emit change event for observers
      this.emit('configChanged', config);
      
      return true;
    } catch (error) {
      // Clean up temp file if it exists
      try {
        await fs.unlink(tempPath);
      } catch (_) {}
      
      throw error;
    }
  }

  /**
   * Get configuration for a specific device type
   * @param {string} deviceType - Device type (sensor, motor, display)
   * @returns {Object|null} Device-specific configuration
   */
  getConfigForDeviceType(deviceType) {
    if (!this.config) {
      throw new Error('Configuration not loaded. Call loadConfig() first.');
    }
    
    return this.config[deviceType] || null;
  }

  /**
   * Update configuration for a specific device type
   * @param {string} deviceType - Device type
   * @param {Object} deviceConfig - Device-specific configuration
   */
  async updateDeviceConfig(deviceType, deviceConfig) {
    if (!this.config) {
      await this.loadConfig();
    }
    
    // Validate device config
    this._validateDeviceConfig(deviceConfig);
    
    // Update and save
    const newConfig = {
      ...this.config,
      [deviceType]: deviceConfig
    };
    
    await this.saveConfig(newConfig);
    console.log(`[LedConfigManager] Updated configuration for device type: ${deviceType}`);
  }

  /**
   * Get all configurations
   * @returns {Object} Complete configuration object
   */
  getAllConfigs() {
    if (!this.config) {
      throw new Error('Configuration not loaded. Call loadConfig() first.');
    }
    return this.config;
  }

  /**
   * Validate complete configuration object
   * @private
   */
  _validateConfig(config) {
    if (!config || typeof config !== 'object') {
      throw new Error('Configuration must be an object');
    }
    
    // Validate each device type config
    for (const [deviceType, deviceConfig] of Object.entries(config)) {
      try {
        this._validateDeviceConfig(deviceConfig);
      } catch (error) {
        throw new Error(`Invalid config for ${deviceType}: ${error.message}`);
      }
    }
  }

  /**
   * Validate device-specific configuration
   * @private
   */
  _validateDeviceConfig(deviceConfig) {
    const { ledCount, topology, gpioPin, brightness, defaultEffect } = deviceConfig;
    
    // Validate LED count
    if (typeof ledCount !== 'number' || ledCount < 0 || ledCount > 1000) {
      throw new Error('ledCount must be a number between 0 and 1000');
    }
    
    // Validate topology
    const validTopologies = ['strip', 'matrix', 'ring'];
    if (!validTopologies.includes(topology)) {
      throw new Error(`topology must be one of: ${validTopologies.join(', ')}`);
    }
    
    // Validate GPIO pin (basic check)
    if (typeof gpioPin !== 'number' || gpioPin < 0 || gpioPin > 39) {
      throw new Error('gpioPin must be a number between 0 and 39');
    }
    
    // Validate brightness
    if (typeof brightness !== 'number' || brightness < 0 || brightness > 255) {
      throw new Error('brightness must be a number between 0 and 255');
    }
    
    // Validate default effect
    const validEffects = ['solid', 'blink', 'pulse', 'rainbow', 'chase', 'sparkle'];
    if (!validEffects.includes(defaultEffect)) {
      throw new Error(`defaultEffect must be one of: ${validEffects.join(', ')}`);
    }
  }

  /**
   * Validate device-reported LED count against configured count
   * @param {string} deviceType - Device type
   * @param {number} reportedCount - LED count reported by device
   * @param {string} chipType - Chip type (ESP8266, ESP32)
   * @returns {Object} { valid: boolean, warning: string|null }
   */
  validateDeviceLedCount(deviceType, reportedCount, chipType) {
    const config = this.getConfigForDeviceType(deviceType);
    
    if (!config) {
      return {
        valid: true,
        warning: `No configuration found for device type: ${deviceType}`
      };
    }
    
    const configuredCount = config.ledCount;
    const tolerance = 5;
    const diff = Math.abs(reportedCount - configuredCount);
    
    // Check platform limits
    const platformLimit = chipType === 'ESP8266' ? 300 : 1000;
    if (reportedCount > platformLimit) {
      return {
        valid: false,
        warning: `LED count ${reportedCount} exceeds ${chipType} limit of ${platformLimit}`
      };
    }
    
    // Check mismatch tolerance
    if (diff > tolerance) {
      return {
        valid: false,
        warning: `LED count mismatch: expected ${configuredCount}, detected ${reportedCount}`
      };
    }
    
    return { valid: true, warning: null };
  }
}

module.exports = LedConfigManager;
