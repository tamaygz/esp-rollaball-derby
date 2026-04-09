const fs = require('fs/promises');
const fsSync = require('fs');
const path = require('path');
const { EventEmitter } = require('events');

// Load the shared 16-color palette once at module load.
const PALETTE_PATH = path.join(__dirname, '../../../clients/assets/themes/shared/player-colors.json');
let _palette = [];
try {
  const raw = fsSync.readFileSync(PALETTE_PATH, 'utf8');
  _palette = JSON.parse(raw).colors;
} catch (err) {
  console.error('[LedConfigManager] Failed to load player-colors.json:', err.message);
  _palette = [];
}
const PALETTE_SIZE = _palette.length || 16;

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
      },
      deviceColorMap: {},  // { "<chipId>": colorIndex }
      deviceNameMap: {}    // { "<chipId>": name }
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
      // Ensure deviceColorMap and deviceNameMap exist (migration from older config files)
      if (!this.config.deviceColorMap) {
        this.config.deviceColorMap = {};
      }
      if (!this.config.deviceNameMap) {
        this.config.deviceNameMap = {};
      }
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
    
    // Validate each device type config (skip non-device-type keys)
    for (const [key, value] of Object.entries(config)) {
      if (key === 'deviceColorMap') continue;
      if (key === 'deviceNameMap') continue;
      try {
        this._validateDeviceConfig(value);
      } catch (error) {
        throw new Error(`Invalid config for ${key}: ${error.message}`);
      }
    }
  }

  // ─── Device Color Management ────────────────────────────────────────────────

  /**
   * Get the full player-color palette.
   * @returns {Array} Array of { index, hex, pixi, name }
   */
  getPalette() {
    return _palette;
  }

  /**
   * Get the persistent device → colorIndex map.
   * @returns {Object} { "<chipId>": colorIndex }
   */
  getDeviceColorMap() {
    if (!this.config) return {};
    return this.config.deviceColorMap || {};
  }

  /**
   * Get the persistent device → name map.
   * @returns {Object} { "<chipId>": name }
   */
  getDeviceNameMap() {
    if (!this.config) return {};
    return this.config.deviceNameMap || {};
  }

  /**
   * Look up the persisted name for a known device.
   * @param {string|null} chipId
   * @returns {string|null} Persisted name, or null if unknown.
   */
  getDeviceName(chipId) {
    if (!chipId) return null;
    const map = this.getDeviceNameMap();
    return map[chipId] !== undefined ? map[chipId] : null;
  }

  /**
   * Persist a name for a device identified by chipId.
   * Fire-and-forget — errors are logged but not thrown.
   * @param {string} chipId
   * @param {string} name
   */
  setDeviceName(chipId, name) {
    if (!chipId || !this.config) return;
    const map = this.getDeviceNameMap();
    map[chipId] = name;
    this.config.deviceNameMap = map;
    this.saveConfig(this.config).catch((err) => {
      console.error('[LedConfigManager] Failed to persist deviceNameMap:', err.message);
    });
  }

  /**
   * Assign a color to a device (or web client).
   *
   * For devices with a chipId: looks up the persisted mapping first. If the
   * chipId is unknown, picks the lowest unused colorIndex and persists it.
   *
   * For web clients (chipId is null/undefined): picks the lowest colorIndex
   * not currently claimed by any persisted device.
   *
   * @param {string|null} chipId  The ESP chipId hex string, or null for web clients.
   * @returns {number} colorIndex 0 … PALETTE_SIZE-1
   */
  assignColor(chipId) {
    const map = this.getDeviceColorMap();

    // Known device → return persisted color
    if (chipId && map[chipId] !== undefined) {
      return map[chipId];
    }

    // Collect indices already claimed by persisted devices
    const usedByDevices = new Set(Object.values(map));

    // Pick lowest unused index
    for (let i = 0; i < PALETTE_SIZE; i++) {
      if (!usedByDevices.has(i)) {
        // Persist for hardware devices, not for web clients
        if (chipId) {
          map[chipId] = i;
          this._persistDeviceColorMap(map);
        }
        return i;
      }
    }

    // All slots occupied — wrap with modulo (RISK-002)
    const fallback = Object.keys(map).length % PALETTE_SIZE;
    if (chipId) {
      map[chipId] = fallback;
      this._persistDeviceColorMap(map);
    }
    return fallback;
  }

  /**
   * Admin override: assign a specific color to a device.
   * @param {string} chipId     ESP chipId hex string
   * @param {number} colorIndex 0 … PALETTE_SIZE-1
   */
  async updateDeviceColor(chipId, colorIndex) {
    if (typeof colorIndex !== 'number' || colorIndex < 0 || colorIndex >= PALETTE_SIZE) {
      throw new Error(`colorIndex must be 0–${PALETTE_SIZE - 1}`);
    }
    const map = this.getDeviceColorMap();
    map[chipId] = colorIndex;
    if (!this.config) await this.loadConfig();
    this.config.deviceColorMap = map;
    await this.saveConfig(this.config);
  }

  /**
   * Get the hex color string for a given colorIndex.
   * @param {number} colorIndex
   * @returns {string} e.g. "#E53E3E"
   */
  getColorHex(colorIndex) {
    if (_palette[colorIndex]) return _palette[colorIndex].hex;
    return '#FFFFFF';
  }

  /** @private — fire-and-forget persistence of deviceColorMap */
  _persistDeviceColorMap(map) {
    if (!this.config) return;
    this.config.deviceColorMap = map;
    this.saveConfig(this.config).catch((err) => {
      console.error('[LedConfigManager] Failed to persist deviceColorMap:', err.message);
    });
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
