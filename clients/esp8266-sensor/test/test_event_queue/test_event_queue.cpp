/**
 * Native unit tests for EventQueue<T, N>
 *
 * Run with:  pio test -e native_test
 *
 * Covers:
 *   1. test_push_pop_fifo            — basic FIFO ordering for equal-priority items
 *   2. test_priority_eviction        — TOOK_LEAD survives 3× SCORE_PLUS1 overflow
 *   3. test_empty_pop_returns_false  — pop on empty returns false, does not crash
 *   4. test_full_no_crash            — overflow eviction does not corrupt memory
 */

#include <unity.h>
#include "EventQueue.h"

// Minimal enum that mirrors the priority ordering of LocalEventType.
// Higher value = higher priority — same contract as the real enum.
enum class Ev : uint8_t {
  NONE        = 0,
  SCORE_PLUS1 = 1,
  SCORE_PLUS2 = 2,
  SCORE_PLUS3 = 3,
  TOOK_LEAD   = 7,
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void setUp()    {}
static void tearDown() {}

// ─── Tests ───────────────────────────────────────────────────────────────────

/**
 * test_push_pop_fifo
 * Pushing three equal-priority events and popping them should yield FIFO order.
 */
static void test_push_pop_fifo() {
  EventQueue<Ev, 4> q;

  TEST_ASSERT_TRUE(q.isEmpty());
  TEST_ASSERT_EQUAL(0, q.size());

  q.push(Ev::SCORE_PLUS1);
  q.push(Ev::SCORE_PLUS2);
  q.push(Ev::SCORE_PLUS3);

  TEST_ASSERT_FALSE(q.isEmpty());
  TEST_ASSERT_EQUAL(3, q.size());

  Ev out = Ev::NONE;
  TEST_ASSERT_TRUE(q.pop(out));
  TEST_ASSERT_EQUAL((uint8_t)Ev::SCORE_PLUS1, (uint8_t)out);

  TEST_ASSERT_TRUE(q.pop(out));
  TEST_ASSERT_EQUAL((uint8_t)Ev::SCORE_PLUS2, (uint8_t)out);

  TEST_ASSERT_TRUE(q.pop(out));
  TEST_ASSERT_EQUAL((uint8_t)Ev::SCORE_PLUS3, (uint8_t)out);

  TEST_ASSERT_TRUE(q.isEmpty());
}

/**
 * test_priority_eviction
 * Fill a 3-slot queue with SCORE_PLUS1 entries, then push a high-priority
 * TOOK_LEAD event. One SCORE_PLUS1 should be evicted; TOOK_LEAD must survive.
 * Pushing another SCORE_PLUS1 into a queue that already has TOOK_LEAD should
 * be dropped (SCORE_PLUS1 < TOOK_LEAD).
 */
static void test_priority_eviction() {
  EventQueue<Ev, 3> q;

  // Fill queue with low-priority events
  TEST_ASSERT_TRUE(q.push(Ev::SCORE_PLUS1));
  TEST_ASSERT_TRUE(q.push(Ev::SCORE_PLUS1));
  TEST_ASSERT_TRUE(q.push(Ev::SCORE_PLUS1));
  TEST_ASSERT_EQUAL(3, q.size());

  // Push high-priority event — should evict one SCORE_PLUS1
  TEST_ASSERT_TRUE(q.push(Ev::TOOK_LEAD));
  TEST_ASSERT_EQUAL(3, q.size());

  // Push another low-priority event — should be dropped (SCORE_PLUS1 <= TOOK_LEAD)
  TEST_ASSERT_FALSE(q.push(Ev::SCORE_PLUS1));
  TEST_ASSERT_EQUAL(3, q.size());

  // TOOK_LEAD must be present in the queue
  bool foundLead = false;
  for (uint8_t i = 0; i < 3; i++) {
    Ev ev = Ev::NONE;
    q.pop(ev);
    if (ev == Ev::TOOK_LEAD) foundLead = true;
  }
  TEST_ASSERT_TRUE(foundLead);
}

/**
 * test_empty_pop_returns_false
 * pop() on an empty queue returns false and does not modify the output variable.
 */
static void test_empty_pop_returns_false() {
  EventQueue<Ev, 4> q;

  Ev out = Ev::SCORE_PLUS3;  // sentinel
  bool result = q.pop(out);

  TEST_ASSERT_FALSE(result);
  // out must be unchanged
  TEST_ASSERT_EQUAL((uint8_t)Ev::SCORE_PLUS3, (uint8_t)out);
}

/**
 * test_full_no_crash
 * Overflowing the queue many times should not corrupt memory or crash.
 * After overflow, the queue must remain consistent (size == N).
 */
static void test_full_no_crash() {
  EventQueue<Ev, 4> q;

  // Fill queue
  for (uint8_t i = 0; i < 4; i++) {
    q.push(Ev::SCORE_PLUS1);
  }
  TEST_ASSERT_EQUAL(4, q.size());

  // Overflow many times with varying priorities
  for (uint8_t i = 0; i < 16; i++) {
    // Alternately push high and low priority — no crash allowed
    q.push(i % 2 == 0 ? Ev::TOOK_LEAD : Ev::NONE);
  }

  // Queue must still report a consistent, bounded size
  TEST_ASSERT_EQUAL(4, q.size());

  // All pops should succeed (no garbage)
  Ev out = Ev::NONE;
  for (uint8_t i = 0; i < 4; i++) {
    TEST_ASSERT_TRUE(q.pop(out));
  }
  TEST_ASSERT_TRUE(q.isEmpty());
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_push_pop_fifo);
  RUN_TEST(test_priority_eviction);
  RUN_TEST(test_empty_pop_returns_false);
  RUN_TEST(test_full_no_crash);
  return UNITY_END();
}
