#pragma once
#include <stdint.h>

/**
 * EventQueue<T, N> — bounded FIFO ring buffer with priority-based overflow eviction.
 *
 * Designed for firmware event dispatch where high-priority events must never be
 * lost. On overflow, the entry with the lowest enum value (lowest priority) is
 * evicted. Ties are broken by age: the oldest (head-side) low-priority entry
 * is evicted first.
 *
 * Intended use: T is an enum class ordered so that higher enum value = higher
 * priority (e.g. LocalEventType from GameEvents.h). N is the ring buffer depth
 * (8 or 16 is typical for firmware).
 *
 * Example (LocalEventType has NONE=0 < SCORE_PLUS1 < … < TOOK_LEAD):
 * ```cpp
 * EventQueue<LocalEventType, 8> q;
 * q.push(LocalEventType::TOOK_LEAD);      // high priority
 * q.push(LocalEventType::SCORE_PLUS1);    // low priority — safe while space remains
 * // … on overflow, SCORE_PLUS1 would be evicted before TOOK_LEAD
 * ```
 *
 * Thread-safety: none — intended for single-threaded Arduino loop() use.
 */
template<typename T, uint8_t N>
class EventQueue {
public:
  EventQueue() : _head(0), _tail(0), _count(0) {}

  /**
   * Push an event onto the queue.
   *
   * If the queue has room, the event is appended and true is returned.
   * If the queue is full, the entry with the lowest priority (smallest enum
   * value) among all current entries is found. If the incoming event has
   * strictly higher priority, it replaces the lowest-priority entry in-place
   * and true is returned. Otherwise, the incoming event is dropped and false
   * is returned.
   *
   * @param ev Event to enqueue.
   * @return true if ev was added, false if it was dropped (lower/equal priority
   *         than all existing entries when the queue was full).
   */
  bool push(T ev) {
    if (_count < N) {
      _buf[_tail] = ev;
      _tail = (_tail + 1) % N;
      _count++;
      return true;
    }

    // Buffer full — find the oldest entry with the minimum priority value.
    uint8_t evictIdx = _head;
    for (uint8_t i = 1; i < N; i++) {
      uint8_t idx = (_head + i) % N;
      if (_buf[idx] < _buf[evictIdx]) {
        evictIdx = idx;
      }
    }

    // Drop the incoming event if it is not strictly higher priority.
    if (ev <= _buf[evictIdx]) {
      return false;
    }

    _buf[evictIdx] = ev;
    return true;
  }

  /**
   * Pop the oldest event (FIFO) into @p out.
   *
   * @param out Receives the event value. Unchanged if the queue is empty.
   * @return true if an event was popped, false if the queue was empty.
   */
  bool pop(T& out) {
    if (_count == 0) return false;
    out = _buf[_head];
    _head = (_head + 1) % N;
    _count--;
    return true;
  }

  /** @return true if the queue contains no events. */
  bool isEmpty() const { return _count == 0; }

  /** @return Number of events currently in the queue. */
  uint8_t size() const { return _count; }

  /** Remove all events. */
  void clear() { _head = 0; _tail = 0; _count = 0; }

private:
  T       _buf[N];
  uint8_t _head;   // index of the oldest entry (next to pop)
  uint8_t _tail;   // index where the next push is written
  uint8_t _count;  // number of entries currently in the buffer
};
