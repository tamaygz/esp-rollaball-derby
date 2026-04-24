#pragma once

#ifndef NATIVE_TEST
#include <Arduino.h>
#include <stdarg.h>
#endif

// ─── Queue configuration ──────────────────────────────────────────────────────
// Holds the last N log lines when no WebSocket sender is available.
// The oldest entry is overwritten when the buffer is full ("investigation stack").
// Adjust to taste — defaults are conservative for ESP8266 (80 KB SRAM).
#ifndef DERBY_LOG_QUEUE_SIZE
#  define DERBY_LOG_QUEUE_SIZE 16    // max queued lines (ring buffer)
#endif
#ifndef DERBY_LOG_LINE_MAX
#  define DERBY_LOG_LINE_MAX   120   // chars per line (truncated if longer)
#endif

// ─── ILogSender ───────────────────────────────────────────────────────────────
// Implement on WSClient to forward log lines over WebSocket to the admin UI.
class ILogSender {
public:
    virtual bool isConnected() const = 0;
    virtual bool sendLog(const char* message) = 0;
    virtual ~ILogSender() = default;
};

// ─── DerbyLogger ──────────────────────────────────────────────────────────────
// Static singleton logger. Native Serial output is NEVER touched here — the
// DERBY_LOG_* macros below call Serial first, then DerbyLogger to forward
// the line over WebSocket.
//
// Behaviour:
//   • Connected sender → line forwarded immediately via ILogSender::sendLog().
//   • No sender or disconnected → line pushed onto a bounded ring buffer.
//     When the buffer is full the oldest entry is silently overwritten.
//   • Call flushQueue() once a connection opens to drain buffered lines.
//
// This header is fully self-contained (Meyers-singleton state; no .cpp needed).
class DerbyLogger {
public:
    // Register (or clear) the WebSocket sender. Call from main() after wsClient
    // is initialised. Pass nullptr to detach.
    static void setSender(ILogSender* sender) {
        _state().sender = sender;
    }

    // Enqueue a pre-formatted C-string.
    // Called by DERBY_LOG_* macros — do not invoke directly.
    static void _enqueue(const char* msg) {
        State& s = _state();
        if (s.sender && s.sender->isConnected()) {
            s.sender->sendLog(msg);
            return;
        }
        // Ring buffer: overwrite oldest when full
        strlcpy(s.queue[s.head], msg, sizeof(s.queue[s.head]));
        s.head = static_cast<uint8_t>((s.head + 1) % DERBY_LOG_QUEUE_SIZE);
        if (s.count < DERBY_LOG_QUEUE_SIZE) s.count++;
    }

    // Format-and-enqueue. Called by DERBY_LOG_F after Serial.printf.
    static void _fmtEnqueue(const char* fmt, ...) {
        char buf[DERBY_LOG_LINE_MAX + 1];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        // Strip trailing newline — WS consumer adds its own line separation
        const size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        _enqueue(buf);
    }

    // Drain the ring buffer to the registered sender.
    // Call once a WebSocket connection is established (from _onEvent).
    static void flushQueue() {
        State& s = _state();
        if (!s.sender || !s.sender->isConnected() || s.count == 0) return;
        const uint8_t tail = static_cast<uint8_t>(
            (s.head - s.count + DERBY_LOG_QUEUE_SIZE) % DERBY_LOG_QUEUE_SIZE);
        for (uint8_t i = 0; i < s.count; i++) {
            s.sender->sendLog(s.queue[(tail + i) % DERBY_LOG_QUEUE_SIZE]);
        }
        s.count = 0;
        s.head  = 0;
    }

private:
    struct State {
        ILogSender* sender;
        char        queue[DERBY_LOG_QUEUE_SIZE][DERBY_LOG_LINE_MAX + 1];
        uint8_t     head;   // next write position
        uint8_t     count;  // valid entries in buffer
    };

    // Meyers singleton — C++11 compatible, header-only, no race condition on
    // embedded single-core MCUs (no std::mutex needed).
    static State& _state() {
        static State s = {};
        return s;
    }
};

// ─── Logging macros ───────────────────────────────────────────────────────────
// Serial is ALWAYS called first; DerbyLogger runs only after.
// This guarantees native Serial output is never hindered even if the logger
// misbehaves or the WS connection is absent.
//
//   DERBY_LOG_F(fmt, ...)  — printf-style (replaces Serial.printf calls)
//   DERBY_LOG_LN(str)      — plain C-string + newline (replaces Serial.println)

#ifndef NATIVE_TEST
#  define DERBY_LOG_F(fmt, ...) do {                              \
       Serial.printf(fmt, ##__VA_ARGS__);                         \
       DerbyLogger::_fmtEnqueue(fmt, ##__VA_ARGS__);              \
   } while (0)

#  define DERBY_LOG_LN(str) do {                                  \
       Serial.println(str);                                       \
       DerbyLogger::_enqueue(str);                                \
   } while (0)
#else
// Host / native unit-test build — no Serial; macros are no-ops.
#  define DERBY_LOG_F(fmt, ...) ((void)0)
#  define DERBY_LOG_LN(str)     ((void)0)
#endif
