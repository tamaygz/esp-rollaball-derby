#pragma once
#include "bt_audio.h"
#include "config.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <BluetoothA2DPSource.h>

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

// Sound manager: fetches WAV files from the game server via HTTP,
// decodes PCM and feeds the A2DP data callback.
// Graceful degradation: if no BT speaker is connected, playback silently skips.
class SoundManager {
public:
    // host/port: game server address for fetching sound files
    bool begin(BtAudio* btAudio, const char* host, uint16_t port);

    // Non-blocking update (drives fetch, decoding, playback state) — call every loop()
    void loop();

    // Queue a sound event for playback (replaces any currently queued event)
    void play(SoundEvent event);

    // Optional: PWM buzzer fallback (set pin to 255 to disable)
    void setBuzzerPin(uint8_t pin);

    bool isAvailable() const { return _btAudio != nullptr; }

private:
    BtAudio*  _btAudio    = nullptr;
    char      _host[40]   = {};
    uint16_t  _port       = 3000;

    SoundEvent _queued    = SoundEvent::NONE;
    SoundEvent _playing   = SoundEvent::NONE;

    uint8_t   _buzzerPin  = 255;

    // PCM playback state
    // WAV is decoded to a simple PCM buffer in PSRAM (or heap as fallback)
    static const size_t PCM_BUF_MAX = 256 * 1024;  // 256 KB — ~3 s at 44.1kHz stereo 16-bit
    int16_t*  _pcmBuf     = nullptr;
    size_t    _pcmSamples = 0;  // total stereo samples (L+R pairs = 1 frame)
    size_t    _pcmPos     = 0;  // current playback position in samples

    // A2DP data callback
    static SoundManager* _instance;
    static int32_t _audioCallback(Frame* frame, int32_t frame_count);

    // Fetch + decode WAV for a given sound event
    bool _fetchAndDecode(SoundEvent event);
    const char* _fileNameForEvent(SoundEvent event) const;

    // Minimal WAV header parser: fills pcmOffset and sampleCount
    bool _parseWavHeader(const uint8_t* data, size_t dataLen,
                         uint32_t& pcmOffset, uint32_t& numSamples);

    // Play a fallback buzzer tone
    void _buzzerTone(uint32_t freqHz, uint32_t durationMs);
};
