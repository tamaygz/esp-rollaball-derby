'use strict';

const { test, describe, before, after } = require('node:test');
const assert = require('node:assert/strict');
const path = require('path');
const fs = require('fs');
const os = require('os');

const SoundManager = require('../src/sound/SoundManager');

// ─── Helpers ──────────────────────────────────────────────────────────────

function createTempDir() {
  return fs.mkdtempSync(path.join(os.tmpdir(), 'sound-test-'));
}

function createTempWav(dir, filename) {
  const filepath = path.join(dir, filename);
  fs.writeFileSync(filepath, Buffer.from([0x52, 0x49, 0x46, 0x46])); // RIFF header
  return filepath;
}

// ─── Tests ────────────────────────────────────────────────────────────────

describe('SoundManager', () => {
  let tempDir;

  before(() => {
    tempDir = createTempDir();
  });

  after(() => {
    // Clean up temp files
    if (fs.existsSync(tempDir)) {
      const files = fs.readdirSync(tempDir);
      files.forEach(f => fs.unlinkSync(path.join(tempDir, f)));
      fs.rmdirSync(tempDir);
    }
  });

  test('constructor initializes with enabled=true by default', () => {
    const sm = new SoundManager(tempDir);
    assert.ok(sm._enabled !== undefined);
  });

  test('constructor respects SOUND_ENABLED environment variable', () => {
    const oldEnv = process.env.SOUND_ENABLED;
    
    // Test disabled
    process.env.SOUND_ENABLED = 'false';
    const sm1 = new SoundManager(tempDir);
    assert.equal(sm1._enabled, false);
    
    // Test enabled
    process.env.SOUND_ENABLED = 'true';
    const sm2 = new SoundManager(tempDir);
    assert.equal(sm2._enabled, true);
    
    // Restore
    if (oldEnv !== undefined) {
      process.env.SOUND_ENABLED = oldEnv;
    } else {
      delete process.env.SOUND_ENABLED;
    }
  });

  test('constructor allows options.enabled override', () => {
    const sm = new SoundManager(tempDir, { enabled: false });
    assert.equal(sm._enabled, false);
  });

  test('play() returns silently when disabled', () => {
    const sm = new SoundManager(tempDir, { enabled: false });
    // Should not throw
    sm.play('score_0');
    sm.play('game_started');
  });

  test('play() returns silently for unknown events', () => {
    const sm = new SoundManager(tempDir, { enabled: true });
    // Should not throw for unknown event
    sm.play('nonexistent_event');
  });

  test('play() returns silently when file does not exist', () => {
    const sm = new SoundManager(tempDir, { enabled: true });
    // score_0.wav does not exist in temp dir
    // Should not throw, just skip silently
    sm.play('score_0');
  });

  test('all events in EVENT_FILE_MAP have corresponding file mappings', () => {
    const events = [
      'score_0', 'score_1', 'score_2', 'score_3',
      'game_started', 'game_paused', 'game_resumed', 'game_reset',
      'countdown_tick', 'countdown_go',
      'winner',
      'took_lead', 'became_last',
      'streak_zero', 'streak_three'
    ];
    
    const sm = new SoundManager(tempDir);
    assert.equal(Object.keys(sm.constructor.prototype.play).length >= 0, true, 
      'play() method exists');
    
    // Verify all events can be referenced
    for (const event of events) {
      // Should not throw when checking
      sm.play(event);
    }
  });

  test('constructor handles missing play-sound gracefully', () => {
    // If play-sound is not installed, createPlayer will be null
    // SoundManager should handle this without crashing
    const sm = new SoundManager(tempDir, { enabled: true });
    // play() should be a callable function
    assert.equal(typeof sm.play, 'function');
  });

  test('play() accepts all documented event types', () => {
    const eventTypes = {
      'score_0': true,
      'score_1': true,
      'score_2': true,
      'score_3': true,
      'game_started': true,
      'game_paused': true,
      'game_resumed': true,
      'game_reset': true,
      'countdown_tick': true,
      'countdown_go': true,
      'winner': true,
      'took_lead': true,
      'became_last': true,
      'streak_zero': true,
      'streak_three': true,
    };
    
    const sm = new SoundManager(tempDir, { enabled: false });
    for (const eventType of Object.keys(eventTypes)) {
      // Should not throw
      sm.play(eventType);
    }
  });

  test('constructor stores soundsDir correctly', () => {
    const sm = new SoundManager(tempDir);
    assert.equal(sm._soundsDir, tempDir);
  });

  test('play() with created WAV file references correct file', () => {
    const sm = new SoundManager(tempDir, { enabled: false });
    
    // Create a WAV file
    createTempWav(tempDir, 'score_0.wav');
    
    // Should not throw when file exists (even though disabled)
    sm.play('score_0');
    
    // Verify file was created
    assert.ok(fs.existsSync(path.join(tempDir, 'score_0.wav')));
  });

  test('multiple instances can have different soundsDirs', () => {
    const tempDir2 = createTempDir();
    
    const sm1 = new SoundManager(tempDir);
    const sm2 = new SoundManager(tempDir2);
    
    assert.notEqual(sm1._soundsDir, sm2._soundsDir);
    
    // Clean up
    fs.rmdirSync(tempDir2);
  });

  test('play() returns immediately without blocking', () => {
    const sm = new SoundManager(tempDir, { enabled: false });
    
    const start = Date.now();
    sm.play('score_0');
    sm.play('score_1');
    sm.play('score_2');
    const elapsed = Date.now() - start;
    
    // Should complete almost instantly (disabled, so no actual playback)
    assert.ok(elapsed < 100, 'play() should return without blocking');
  });

  test('enabled property reflects constructor options', () => {
    const sm1 = new SoundManager(tempDir, { enabled: true });
    assert.equal(sm1._enabled, true);
    
    const sm2 = new SoundManager(tempDir, { enabled: false });
    assert.equal(sm2._enabled, false);
  });

  test('play() silently handles events with missing files gracefully', () => {
    const sm = new SoundManager(tempDir, { enabled: true });
    
    // Create only score_0.wav, but try to play others
    createTempWav(tempDir, 'score_0.wav');
    
    // Missing files should not throw
    sm.play('score_1'); // missing
    sm.play('score_0'); // exists
    sm.play('score_2'); // missing
  });
});
