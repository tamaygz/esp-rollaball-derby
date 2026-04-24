/**
 * Native unit tests for GameEventMapper dispatch tables (T9-7).
 *
 * Run with:  pio test -e native_test
 *
 * Uses the same single-TU inclusion pattern as test_animation_manager:
 *   - mock headers first (resolved via -I test/mocks)
 *   - production .cpp sources included directly
 *   - GameEventMapper.h (header-only) included last
 *
 * Covers:
 *   1. test_all_local_events_dispatch
 *      — every non-NONE LocalEventType triggers an effect via the mapper.
 *   2. test_local_none_is_noop
 *      — LocalEventType::NONE must not start any effect.
 *   3. test_all_global_events_dispatch
 *      — every non-NONE GlobalEventType triggers an effect via the mapper.
 *   4. test_global_none_is_noop
 *      — GlobalEventType::NONE must not start any effect.
 *   5. test_set_device_color_updates_effects
 *      — SCORE_PLUS1 still dispatches after setDeviceColor().
 */

// ── Mock headers must come before any production headers ─────────────────────
#define NATIVE_TEST
#include "Arduino.h"

// ── Production sources (single-TU inclusion) ──────────────────────────────────
#include "LedController.cpp"
#include "AnimationManager.cpp"

// GameEventMapper.h is header-only; effect headers it includes are also
// header-only, so no additional .cpp sources are needed.
#include <leds/GameEventMapper.h>

// ── Unity test framework ──────────────────────────────────────────────────────
#include <unity.h>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void setUp()    {}
static void tearDown() {}

/**
 * Initialise a mapper with an 8-LED controller.
 * All three objects must outlive the test body.
 */
static void beginMapper(LedController& ctrl, AnimationManager& anim, GameEventMapper& mapper) {
    ctrl.begin(8, 4);  // 8 LEDs on GPIO4 — valid for NATIVE_TEST (LED_GPIO_MAX=39)
    anim.begin();
    mapper.begin();
}

// ─── Tests ───────────────────────────────────────────────────────────────────

/**
 * test_all_local_events_dispatch
 *
 * Calls onLocalEvent() for every non-NONE LocalEventType and asserts that
 * the AnimationManager has an active effect afterwards.
 * The animator is stopped between iterations so each event gets a clean slate.
 */
static void test_all_local_events_dispatch() {
    LedController  ctrl;
    AnimationManager anim(&ctrl);
    GameEventMapper  mapper(&ctrl, &anim);
    beginMapper(ctrl, anim, mapper);

    const LocalEventType events[] = {
        LocalEventType::SCORE_PLUS1,
        LocalEventType::SCORE_PLUS2,
        LocalEventType::SCORE_PLUS3,
        LocalEventType::ZERO_ROLL,
        LocalEventType::TOOK_LEAD,
        LocalEventType::BECAME_LAST,
        LocalEventType::STREAK_ZERO,
        LocalEventType::STREAK_THREE,
    };
    const uint8_t count = sizeof(events) / sizeof(events[0]);

    for (uint8_t i = 0; i < count; ++i) {
        anim.stop();
        mapper.onLocalEvent(events[i]);
        TEST_ASSERT_TRUE_MESSAGE(anim.isPlaying(),
            "onLocalEvent: expected an effect to start");
    }
}

/**
 * test_local_none_is_noop
 *
 * LocalEventType::NONE must not trigger any effect.
 */
static void test_local_none_is_noop() {
    LedController  ctrl;
    AnimationManager anim(&ctrl);
    GameEventMapper  mapper(&ctrl, &anim);
    beginMapper(ctrl, anim, mapper);

    mapper.onLocalEvent(LocalEventType::NONE);
    TEST_ASSERT_FALSE_MESSAGE(anim.isPlaying(),
        "onLocalEvent(NONE): no effect should start");
}

/**
 * test_all_global_events_dispatch
 *
 * Calls onGlobalEvent() for every non-NONE GlobalEventType and asserts that
 * the AnimationManager has an active effect afterwards.
 */
static void test_all_global_events_dispatch() {
    LedController  ctrl;
    AnimationManager anim(&ctrl);
    GameEventMapper  mapper(&ctrl, &anim);
    beginMapper(ctrl, anim, mapper);

    const GlobalEventType events[] = {
        GlobalEventType::COUNTDOWN_TICK,
        GlobalEventType::GAME_STARTED,
        GlobalEventType::GAME_PAUSED,
        GlobalEventType::GAME_RESUMED,
        GlobalEventType::GAME_RESET,
        GlobalEventType::WINNER_SELF,
        GlobalEventType::WINNER_OTHER,
    };
    const uint8_t count = sizeof(events) / sizeof(events[0]);

    for (uint8_t i = 0; i < count; ++i) {
        anim.stop();
        mapper.onGlobalEvent(events[i]);
        TEST_ASSERT_TRUE_MESSAGE(anim.isPlaying(),
            "onGlobalEvent: expected an effect to start");
    }
}

/**
 * test_global_none_is_noop
 *
 * GlobalEventType::NONE must not trigger any effect.
 */
static void test_global_none_is_noop() {
    LedController  ctrl;
    AnimationManager anim(&ctrl);
    GameEventMapper  mapper(&ctrl, &anim);
    beginMapper(ctrl, anim, mapper);

    mapper.onGlobalEvent(GlobalEventType::NONE);
    TEST_ASSERT_FALSE_MESSAGE(anim.isPlaying(),
        "onGlobalEvent(NONE): no effect should start");
}

/**
 * test_set_device_color_updates_effects
 *
 * After setDeviceColor(), SCORE_PLUS1 and SCORE_PLUS2 must still dispatch.
 */
static void test_set_device_color_updates_effects() {
    LedController  ctrl;
    AnimationManager anim(&ctrl);
    GameEventMapper  mapper(&ctrl, &anim);
    beginMapper(ctrl, anim, mapper);

    mapper.setDeviceColor(RgbColor(255, 0, 128));

    mapper.onLocalEvent(LocalEventType::SCORE_PLUS1);
    TEST_ASSERT_TRUE_MESSAGE(anim.isPlaying(),
        "SCORE_PLUS1 must dispatch after setDeviceColor");

    anim.stop();
    mapper.onLocalEvent(LocalEventType::SCORE_PLUS2);
    TEST_ASSERT_TRUE_MESSAGE(anim.isPlaying(),
        "SCORE_PLUS2 must dispatch after setDeviceColor");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_local_events_dispatch);
    RUN_TEST(test_local_none_is_noop);
    RUN_TEST(test_all_global_events_dispatch);
    RUN_TEST(test_global_none_is_noop);
    RUN_TEST(test_set_device_color_updates_effects);
    return UNITY_END();
}
