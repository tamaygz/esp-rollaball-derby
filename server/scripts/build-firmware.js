#!/usr/bin/env node
'use strict';

/**
 * build-firmware.js — builds all PlatformIO firmware targets and copies the
 * resulting binaries into each client's web-install/ directory so the
 * "Bundled firmware" option on the flash pages works without a GitHub release.
 *
 * Run from the server/ directory:  npm run build:firmware
 */

const { execSync }    = require('child_process');
const { copyFileSync, existsSync } = require('fs');
const path            = require('path');

const ROOT = path.join(__dirname, '..', '..');

// ── Helpers ──────────────────────────────────────────────────────────────────

function pio(dir, envs) {
  const flags = envs.map(e => `-e ${e}`).join(' ');
  const label = path.relative(ROOT, dir);
  console.log(`\n▶  pio run ${flags}  [${label}]`);
  execSync(`pio run ${flags}`, { cwd: dir, stdio: 'inherit' });
}

function copy(src, dst) {
  if (!existsSync(src)) {
    console.error(`\n✗  Binary not found: ${path.relative(ROOT, src)}`);
    process.exit(1);
  }
  copyFileSync(src, dst);
  const rel = f => path.relative(ROOT, f).replace(/\\/g, '/');
  console.log(`   ✓  ${rel(src)}  →  ${rel(dst)}`);
}

// ── Paths ────────────────────────────────────────────────────────────────────

const SENSOR_DIR   = path.join(ROOT, 'clients', 'esp8266-sensor');
const SENSOR_BUILD = path.join(SENSOR_DIR, '.pio', 'build');
const SENSOR_WEB   = path.join(SENSOR_DIR, 'web-install');

const MOTOR_DIR    = path.join(ROOT, 'clients', 'esp32-motor');
const MOTOR_BUILD  = path.join(MOTOR_DIR, '.pio', 'build');
const MOTOR_WEB    = path.join(MOTOR_DIR, 'web-install');

// ── Sensor firmware (ESP8266 / ESP32) ────────────────────────────────────────

pio(SENSOR_DIR, ['d1_mini', 'nodemcuv2', 'esp32dev']);

copy(
  path.join(SENSOR_BUILD, 'd1_mini',   'firmware.bin'),
  path.join(SENSOR_WEB,   'firmware.bin'),
);
copy(
  path.join(SENSOR_BUILD, 'nodemcuv2', 'firmware.bin'),
  path.join(SENSOR_WEB,   'firmware-nodemcuv2.bin'),
);
copy(
  path.join(SENSOR_BUILD, 'esp32dev',  'firmware.bin'),
  path.join(SENSOR_WEB,   'firmware-esp32dev.bin'),
);

// ── Motor firmware (ESP32) ───────────────────────────────────────────────────

pio(MOTOR_DIR, ['esp32dev', 'esp32-s3']);

copy(
  path.join(MOTOR_BUILD, 'esp32dev', 'firmware.bin'),
  path.join(MOTOR_WEB,   'firmware-motor.bin'),
);
copy(
  path.join(MOTOR_BUILD, 'esp32-s3', 'firmware.bin'),
  path.join(MOTOR_WEB,   'firmware-motor-esp32-s3.bin'),
);

console.log('\n✔  All firmware binaries built and staged.\n');
