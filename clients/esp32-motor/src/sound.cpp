#include "sound.h"
#include <Arduino.h>

SoundManager* SoundManager::_instance = nullptr;

bool SoundManager::begin(BtAudio* btAudio, const char* host, uint16_t port) {
    _btAudio = btAudio;
    strlcpy(_host, host ? host : "", sizeof(_host));
    _port = port;
    _instance = this;

    // Allocate ring buffer (16 KB — tiny compared to old 256 KB PCM buf)
    if (psramFound()) {
        _ringBuf = static_cast<uint8_t*>(ps_malloc(RING_BUF_SIZE));
    }
    if (!_ringBuf) {
        _ringBuf = static_cast<uint8_t*>(malloc(RING_BUF_SIZE));
    }
    if (!_ringBuf) {
        Serial.println("[SOUND] Failed to allocate ring buffer — sound disabled");
        return false;
    }

    if (btAudio) {
#ifdef HAS_BT_AUDIO
        btAudio->setAudioCallback(_audioCallback);
#endif
    }

    Serial.printf("[SOUND] Sound manager ready (ring buf %u bytes)\n", RING_BUF_SIZE);
    return true;
}

void SoundManager::loop() {
    // Start new stream when an event is queued
    if (_queued != SoundEvent::NONE) {
        SoundEvent next = _queued;
        _queued = SoundEvent::NONE;

        // Interrupt any active stream
        if (_streaming) _stopStream();

        if (_btAudio && _btAudio->isConnected()) {
            _startStream(next);
        } else if (_buzzerPin != 255) {
            switch (next) {
                case SoundEvent::COUNTDOWN_TICK: _buzzerTone(800,  80); break;
                case SoundEvent::COUNTDOWN_GO:   _buzzerTone(1200, 200); break;
                case SoundEvent::SCORE_PLUS1:    _buzzerTone(880,  100); break;
                case SoundEvent::SCORE_PLUS2:    _buzzerTone(990,  100); break;
                case SoundEvent::SCORE_PLUS3:    _buzzerTone(1100, 150); break;
                case SoundEvent::SCORE_ZERO:     _buzzerTone(300,  200); break;
                case SoundEvent::WINNER:         _buzzerTone(1400, 500); break;
                case SoundEvent::BUTTON_CLICK:   _buzzerTone(1000, 30);  break;
                case SoundEvent::BECAME_LAST:    _buzzerTone(250,  300); break;
                case SoundEvent::STREAK_ZERO:    _buzzerTone(200,  400); break;
                case SoundEvent::STREAK_THREE:   _buzzerTone(1300, 200); break;
                case SoundEvent::TOOK_LEAD:      _buzzerTone(1500, 250); break;
                case SoundEvent::GAME_STARTED:   _buzzerTone(1200, 200); break;
                case SoundEvent::GAME_PAUSED:    _buzzerTone(600,  150); break;
                case SoundEvent::GAME_RESUMED:   _buzzerTone(900,  150); break;
                case SoundEvent::GAME_RESET:     _buzzerTone(400,  200); break;
                case SoundEvent::DRAW:           _buzzerTone(700,  400); break;
                default: break;
            }
        }
    }

    // Feed ring buffer from HTTP stream
    if (_streaming) {
        _fillRingBuffer();

        // Finished when HTTP done and callback has drained the buffer
        if (_httpDone && (_ringWr == _ringRd)) {
            Serial.println("[SOUND] Playback complete");
            _stopStream();
        }
    }
}

void SoundManager::play(SoundEvent event) {
    _queued = event;
}

void SoundManager::setBuzzerPin(uint8_t pin) {
    _buzzerPin = pin;
    if (pin != 255) {
        ledcSetup(0, 1000, 8);
        ledcAttachPin(pin, 0);
    }
}

// ─── A2DP audio callback (runs on BT core) ──────────────────────────────────

#ifdef HAS_BT_AUDIO
int32_t SoundManager::_audioCallback(Frame* frame, int32_t frame_count) {
    if (!_instance || !_instance->_streaming) {
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    uint32_t wr = _instance->_ringWr;
    uint32_t rd = _instance->_ringRd;
    uint32_t avail = wr - rd;  // unsigned diff handles wrap

    const uint32_t bytesPerFrame = sizeof(Frame);  // 4
    int32_t framesAvail = (int32_t)(avail / bytesPerFrame);
    int32_t toRead = min(frame_count, framesAvail);

    for (int32_t i = 0; i < toRead; i++) {
        uint32_t rdPos = rd & RING_MASK;
        // rd is always 4-byte aligned (starts at 0, advances by 4)
        // RING_BUF_SIZE is a multiple of 4, so no wrap-split possible
        memcpy(&frame[i], _instance->_ringBuf + rdPos, bytesPerFrame);
        rd += bytesPerFrame;
    }

    _instance->_ringRd = rd;

    // Pad remaining with silence
    if (toRead < frame_count) {
        memset(frame + toRead, 0, (frame_count - toRead) * sizeof(Frame));
    }

    return frame_count;
}
#endif  // HAS_BT_AUDIO

// ─── Streaming internals ─────────────────────────────────────────────────────

bool SoundManager::_startStream(SoundEvent event) {
    const char* filename = _fileNameForEvent(event);
    if (!filename) return false;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%u/assets/sounds/%s", _host, _port, filename);

    _http.begin(url);
    _http.setTimeout(8000);
    int code = _http.GET();
    if (code != 200) {
        Serial.printf("[SOUND] HTTP %d fetching %s\n", code, url);
        _http.end();
        return false;
    }

    _httpStream = _http.getStreamPtr();
    if (!_httpStream) { _http.end(); return false; }

    // Read WAV header (up to 256 bytes covers any standard WAV)
    uint8_t hdr[256];
    size_t hdrRead = _httpStream->readBytes(reinterpret_cast<char*>(hdr),
                                            min((size_t)256, (size_t)_http.getSize()));
    if (hdrRead < 44 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        Serial.printf("[SOUND] Invalid WAV header for %s\n", filename);
        _http.end(); _httpStream = nullptr;
        return false;
    }

    // Walk sub-chunks to find "data"
    uint32_t offset = 12;
    bool found = false;
    while (offset + 8 <= hdrRead) {
        uint32_t chunkSize;
        memcpy(&chunkSize, hdr + offset + 4, 4);
        if (memcmp(hdr + offset, "data", 4) == 0) {
            uint32_t dataStart = offset + 8;
            // Reset ring buffer
            _ringWr = 0;
            _ringRd = 0;
            // Seed with any PCM bytes already read past the data chunk header
            if (dataStart < hdrRead) {
                size_t seed = hdrRead - dataStart;
                if (seed > RING_BUF_SIZE) seed = RING_BUF_SIZE;
                memcpy(_ringBuf, hdr + dataStart, seed);
                _ringWr = (uint32_t)seed;
            }
            found = true;
            break;
        }
        offset += 8 + chunkSize;
        if (chunkSize % 2 != 0) offset++;  // RIFF word alignment
    }

    if (!found) {
        Serial.printf("[SOUND] No data chunk in %s\n", filename);
        _http.end(); _httpStream = nullptr;
        return false;
    }

    _httpDone  = false;
    _streaming = true;
    _playing   = event;
    Serial.printf("[SOUND] Streaming %s\n", filename);
    return true;
}

void SoundManager::_fillRingBuffer() {
    if (_httpDone || !_httpStream) return;

    uint32_t wr = _ringWr;
    uint32_t rd = _ringRd;
    uint32_t used = wr - rd;
    uint32_t freeBytes = RING_BUF_SIZE - used;

    // Fill in up to two contiguous segments (handles wrap-around)
    while (freeBytes > 0) {
        int avail = _httpStream->available();
        if (avail <= 0) break;

        uint32_t wrPos = wr & RING_MASK;
        uint32_t contiguous = RING_BUF_SIZE - wrPos;        // bytes to end of buffer
        uint32_t toRead = min(freeBytes, contiguous);
        if ((uint32_t)avail < toRead) toRead = (uint32_t)avail;

        int got = _httpStream->readBytes(reinterpret_cast<char*>(_ringBuf + wrPos), toRead);
        if (got <= 0) break;

        wr += (uint32_t)got;
        freeBytes -= (uint32_t)got;
    }

    _ringWr = wr;

    // Detect end-of-stream
    if (!_httpStream->available() && !_httpStream->connected()) {
        _httpDone = true;
        _http.end();
        _httpStream = nullptr;
    }
}

void SoundManager::_stopStream() {
    _streaming = false;  // stop callback from reading first
    _playing   = SoundEvent::NONE;
    _ringWr    = 0;
    _ringRd    = 0;
    _httpDone  = true;
    if (_httpStream) {
        _http.end();
        _httpStream = nullptr;
    }
}

const char* SoundManager::_fileNameForEvent(SoundEvent event) const {
    switch (event) {
        case SoundEvent::COUNTDOWN_TICK: return SOUND_FILE_COUNTDOWN_TICK;
        case SoundEvent::COUNTDOWN_GO:   return SOUND_FILE_COUNTDOWN_GO;
        case SoundEvent::SCORE_PLUS1:    return SOUND_FILE_SCORE_1;
        case SoundEvent::SCORE_PLUS2:    return SOUND_FILE_SCORE_2;
        case SoundEvent::SCORE_PLUS3:    return SOUND_FILE_SCORE_3;
        case SoundEvent::SCORE_ZERO:     return SOUND_FILE_SCORE_0;
        case SoundEvent::WINNER:         return SOUND_FILE_WINNER;
        case SoundEvent::BUTTON_CLICK:   return SOUND_FILE_BUTTON_CLICK;
        case SoundEvent::BECAME_LAST:    return SOUND_FILE_BECAME_LAST;
        case SoundEvent::STREAK_ZERO:    return SOUND_FILE_STREAK_ZERO;
        case SoundEvent::STREAK_THREE:   return SOUND_FILE_STREAK_THREE;
        case SoundEvent::TOOK_LEAD:      return SOUND_FILE_TOOK_LEAD;
        case SoundEvent::GAME_STARTED:   return SOUND_FILE_GAME_STARTED;
        case SoundEvent::GAME_PAUSED:    return SOUND_FILE_GAME_PAUSED;
        case SoundEvent::GAME_RESUMED:   return SOUND_FILE_GAME_RESUMED;
        case SoundEvent::GAME_RESET:     return SOUND_FILE_GAME_RESET;
        case SoundEvent::DRAW:           return SOUND_FILE_DRAW;
        default:                         return nullptr;
    }
}

void SoundManager::_buzzerTone(uint32_t freqHz, uint32_t durationMs) {
    if (_buzzerPin == 255) return;
    ledcWriteTone(0, freqHz);
    delay(durationMs);
    ledcWriteTone(0, 0);
}
