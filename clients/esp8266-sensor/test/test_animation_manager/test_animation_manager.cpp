/**
 * Native unit tests for AnimationManager priority gate (T6).
 *
 * Run with:  pio test -e native_test
 *
 * The test file includes the production .cpp sources directly (single-TU
 * inclusion pattern) after setting up mock headers via -I test/mocks.
 * This avoids a full PlatformIO library build of NeoPixelBus / Arduino.
 *
 * Covers:
 *   1. test_low_priority_dropped_while_high_active
 *      — playEffect(low) while a high-priority effect is active → request dropped.
 *   2. test_priority_resets_after_effect_completes
 *      — high-priority effect completes → _activePriority resets; next request fires.
 */

// ── Mock headers must be included BEFORE any production headers ───────────────
// The build flag -I test/mocks means <Arduino.h>, <NeoPixelBus.h>, and
// <NeoPixelBusLg.h> resolve to our stubs.
#define NATIVE_TEST

// Explicitly pull in our mock Arduino first so it wins over any system header.
#include "Arduino.h"

// ── Production sources (single-TU inclusion) ──────────────────────────────────
// Including the .cpp files brings in all method bodies without a separate
// link step. The NATIVE_TEST guard in LedPlatform.h prevents the #error and
// provides stub platform types.
#include "LedController.cpp"
#include "AnimationManager.cpp"

// ── Unity test framework ──────────────────────────────────────────────────────
#include <unity.h>

// ─── Minimal mock LedEffect ───────────────────────────────────────────────────

class MockEffect : public LedEffect {
public:
  explicit MockEffect(bool startsComplete = false)
      : LedEffect(nullptr), _complete(startsComplete), _beginCalled(false) {}

  void begin()   override { _beginCalled = true; }
  void update(uint32_t) override {}
  void reset()   override { _complete = false; _beginCalled = false; }
  bool isComplete() const override { return _complete; }
  bool setParams(EffectParams) override { return true; }
  const char* getName() const override { return "mock"; }

  void setComplete(bool c) { _complete = c; }
  bool beginCalled() const { return _beginCalled; }

private:
  bool _complete;
  bool _beginCalled;
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static LedController* makeController() {
  LedController* c = new LedController();
  c->begin(8, 4);  // 8 LEDs on GPIO4 — ledPinIsValid returns true for NATIVE_TEST
  return c;
}

static void setUp()    {}
static void tearDown() {}

// ─── Tests ───────────────────────────────────────────────────────────────────

/**
 * test_low_priority_dropped_while_high_active
 *
 * Start a PRIORITY_ADMIN effect, then attempt to play a PRIORITY_GAME effect.
 * The PRIORITY_GAME request must be dropped (admin wins).
 */
static void test_low_priority_dropped_while_high_active() {
  LedController* ctrl = makeController();
  AnimationManager mgr(ctrl);
  mgr.begin();

  MockEffect highEffect;
  MockEffect lowEffect;

  // Play high-priority (ADMIN) effect
  mgr.playEffect(&highEffect, AnimationManager::PRIORITY_ADMIN);
  TEST_ASSERT_TRUE(mgr.isPlaying());
  TEST_ASSERT_TRUE(highEffect.beginCalled());

  // Attempt lower-priority (GAME) request — must be dropped
  mgr.playEffect(&lowEffect, AnimationManager::PRIORITY_GAME);

  // High-priority effect must still be active
  TEST_ASSERT_EQUAL_PTR(&highEffect, mgr.getCurrentEffect());
  TEST_ASSERT_FALSE(lowEffect.beginCalled());

  delete ctrl;
}

/**
 * test_priority_resets_after_effect_completes
 *
 * After a PRIORITY_ADMIN effect completes (isComplete() → true), the manager
 * should reset _activePriority to PRIORITY_AMBIENT so that a subsequent
 * PRIORITY_GAME effect is accepted.
 */
static void test_priority_resets_after_effect_completes() {
  LedController* ctrl = makeController();
  AnimationManager mgr(ctrl);
  mgr.begin();

  // Use a non-zero frame interval that never fires on zero micros, so we can
  // advance the mock clock precisely.
  mgr.setTargetFPS(60);  // 16 667 µs per frame

  MockEffect adminEffect;
  MockEffect gameEffect;

  // Play admin effect
  mgr.playEffect(&adminEffect, AnimationManager::PRIORITY_ADMIN);
  TEST_ASSERT_TRUE(mgr.isPlaying());

  // Mark as complete, advance mock clock past the frame interval, then tick loop()
  adminEffect.setComplete(true);
  _mock_micros += 20000;  // > 16 667 µs frame interval
  mgr.loop();

  // Effect should have been cleaned up
  TEST_ASSERT_FALSE(mgr.isPlaying());

  // Now a PRIORITY_GAME request must be accepted (priority gate reset)
  mgr.playEffect(&gameEffect, AnimationManager::PRIORITY_GAME);
  TEST_ASSERT_TRUE(mgr.isPlaying());
  TEST_ASSERT_EQUAL_PTR(&gameEffect, mgr.getCurrentEffect());

  delete ctrl;
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_low_priority_dropped_while_high_active);
  RUN_TEST(test_priority_resets_after_effect_completes);
  return UNITY_END();
}
