'use strict';

const { test, describe, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const path = require('path');
const fs = require('fs/promises');
const os = require('os');

const LedConfigManager = require('../src/config/LedConfigManager');

// ─── Helpers ──────────────────────────────────────────────────────────────────

function makeManager() {
  // Use a temp path so each test gets a fresh, isolated file system state.
  const tmpPath = path.join(os.tmpdir(), `led-config-test-${Date.now()}-${Math.random()}.json`);
  const mgr = new LedConfigManager(tmpPath);
  // Prime the in-memory config without touching the file system
  mgr.config = JSON.parse(JSON.stringify(mgr.defaultConfig));
  return mgr;
}

// ─── getConfigForDevice ───────────────────────────────────────────────────────

describe('LedConfigManager — getConfigForDevice()', () => {
  test('returns type-wide config when no chipId provided', () => {
    const mgr = makeManager();
    const config = mgr.getConfigForDevice('sensor', null);
    assert.ok(config);
    assert.equal(config.ledCount, mgr.defaultConfig.sensor.ledCount);
  });

  test('returns type-wide config when no per-device override exists', () => {
    const mgr = makeManager();
    const config = mgr.getConfigForDevice('sensor', 'CHIP001');
    assert.ok(config);
    assert.equal(config.ledCount, mgr.defaultConfig.sensor.ledCount);
  });

  test('returns merged config when per-device override exists', () => {
    const mgr = makeManager();
    mgr.config.deviceConfigOverrides['sensor/CHIP001'] = { ledCount: 60, gpioPin: 4 };
    const config = mgr.getConfigForDevice('sensor', 'CHIP001');
    // Override values take precedence
    assert.equal(config.ledCount, 60);
    assert.equal(config.gpioPin, 4);
    // Non-overridden values fall back to type config
    assert.equal(config.brightness, mgr.defaultConfig.sensor.brightness);
  });

  test('override for one chipId does not affect another chipId', () => {
    const mgr = makeManager();
    mgr.config.deviceConfigOverrides['sensor/CHIP001'] = { ledCount: 60 };
    const cfg1 = mgr.getConfigForDevice('sensor', 'CHIP001');
    const cfg2 = mgr.getConfigForDevice('sensor', 'CHIP002');
    assert.equal(cfg1.ledCount, 60);
    assert.equal(cfg2.ledCount, mgr.defaultConfig.sensor.ledCount);
  });
});

// ─── updateDeviceOverride ─────────────────────────────────────────────────────

describe('LedConfigManager — updateDeviceOverride()', () => {
  test('stores a per-device override', async () => {
    const mgr = makeManager();
    // Stub saveConfig to avoid file I/O in unit tests
    mgr.saveConfig = async (cfg) => { mgr.config = cfg; };

    const override = {
      ledCount: 60,
      topology: 'strip',
      gpioPin: 4,
      brightness: 100,
      defaultEffect: 'solid'
    };
    await mgr.updateDeviceOverride('sensor', 'CHIP001', override);

    const stored = mgr.config.deviceConfigOverrides['sensor/CHIP001'];
    assert.ok(stored);
    assert.equal(stored.ledCount, 60);
  });

  test('overwriting an existing override replaces it', async () => {
    const mgr = makeManager();
    mgr.saveConfig = async (cfg) => { mgr.config = cfg; };

    const override1 = { ledCount: 60, topology: 'strip', gpioPin: 4, brightness: 100, defaultEffect: 'solid' };
    const override2 = { ledCount: 10, topology: 'ring', gpioPin: 4, brightness: 50, defaultEffect: 'blink' };
    await mgr.updateDeviceOverride('sensor', 'CHIP001', override1);
    await mgr.updateDeviceOverride('sensor', 'CHIP001', override2);

    const stored = mgr.config.deviceConfigOverrides['sensor/CHIP001'];
    assert.equal(stored.ledCount, 10);
    assert.equal(stored.topology, 'ring');
  });

  test('throws on invalid config (e.g. ledCount out of range)', async () => {
    const mgr = makeManager();
    mgr.saveConfig = async (cfg) => { mgr.config = cfg; };
    await assert.rejects(
      () => mgr.updateDeviceOverride('sensor', 'CHIP001', { ledCount: -1, topology: 'strip', gpioPin: 4, brightness: 50, defaultEffect: 'solid' }),
      /ledCount/i
    );
  });
});

// ─── deleteDeviceOverride ─────────────────────────────────────────────────────

describe('LedConfigManager — deleteDeviceOverride()', () => {
  test('removes an existing override and returns true', async () => {
    const mgr = makeManager();
    mgr.saveConfig = async (cfg) => { mgr.config = cfg; };

    mgr.config.deviceConfigOverrides['sensor/CHIP001'] = { ledCount: 60, topology: 'strip', gpioPin: 4, brightness: 100, defaultEffect: 'solid' };
    const result = await mgr.deleteDeviceOverride('sensor', 'CHIP001');
    assert.equal(result, true);
    assert.equal(mgr.config.deviceConfigOverrides['sensor/CHIP001'], undefined);
  });

  test('returns false when no override exists', async () => {
    const mgr = makeManager();
    mgr.saveConfig = async (cfg) => { mgr.config = cfg; };
    const result = await mgr.deleteDeviceOverride('sensor', 'NOSUCHDEVICE');
    assert.equal(result, false);
  });
});
