#pragma once
#include "bt_audio.h"
#include "config.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#ifdef HAS_BT_AUDIO
#include <BluetoothA2DPSource.h>
#endif

// Sound events mapped to WAV files on the server
enum class SoundEvent {
    NONE,
    COUNTDOWN_TICK,
    COUNTDOWN_GO,
    SCORE_PLUS1,
    SCORE_PLUS2,
    SCORE_PLUS3,
    SCORE_ZERO,
    WINNER,
    BUTTON_CLICK,
    BECAME_LAST,
    STREAK_ZERO,
    STREAK_THREE,
    TOOK_LEAD,
    GAME_STARTED,
    GAME_PAUSED,
    GAME_RESUMED,
    GAME_RESET,
    DRAW,
};

// Sound manager: streams WAV files from the game server via HTTP through
// a small ring buffer fed to the A2DP audio callback.
// Graceful degradation: if no BT speaker is connected, playback silently skips.
class SoundManager {
public:
    bool begin(BtAudio* btAudio, const char* host, uint16_t port);
    void loop();
    void play(SoundEvent event);
    void setBuzzerPin(uint8_t pin);
    bool isAvailable() const { return _btAudio != nullptr; }

private:
    BtAudio*  _btAudio    = nullptr;
    char      _host[40]   = {};
    uint16_t  _port       = 3000;

    SoundEvent _queued    = SoundEvent::NONE;
    SoundEvent _playing   = SoundEvent::NONE;

    uint8_t   _buzzerPin  = 255;

    // Ring buffer for streaming PCM data (power of 2 for masking)
    static const size_t RING_BUF_SIZE = 16 * 1024;  // 16 KB
    static const size_t RING_MASK     = RING_BUF_SIZE - 1;
    uint8_t*       _ringBuf  = nullptr;
    volatile uint32_t _ringWr = 0;   // write index (producer: loop)
    volatile uint32_t _ringRd = 0;   // read index  (consumer: audio callback)

    // HTTP streaming state
    HTTPClient  _http;
    WiFiClient* _httpStream = nullptr;
    bool        _streaming  = false;
    bool        _httpDone   = false;

    // A2DP data callback
    static SoundManager* _instance;
#ifdef HAS_BT_AUDIO
    static int32_t _audioCallback(Frame* frame, int32_t frame_count);
#endif

    bool _startStream(SoundEvent event);
    void _fillRingBuffer();
    void _stopStream();
    const char* _fileNameForEvent(SoundEvent event) const;
    void _buzzerTone(uint32_t freqHz, uint32_t durationMs);
};
