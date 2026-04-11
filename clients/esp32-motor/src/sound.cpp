#include "sound.h"
#include <HTTPClient.h>
#include <Arduino.h>

SoundManager* SoundManager::_instance = nullptr;

bool SoundManager::begin(BtAudio* btAudio, const char* host, uint16_t port) {
    _btAudio = btAudio;
    strlcpy(_host, host ? host : "", sizeof(_host));
    _port = port;
    _instance = this;

    // Allocate PCM buffer from PSRAM if available, else heap
    if (psramFound()) {
        _pcmBuf = static_cast<int16_t*>(ps_malloc(PCM_BUF_MAX));
    }
    if (!_pcmBuf) {
        _pcmBuf = static_cast<int16_t*>(malloc(PCM_BUF_MAX));
    }
    if (!_pcmBuf) {
        Serial.println("[SOUND] Failed to allocate PCM buffer — sound disabled");
        return false;
    }

    if (btAudio) {
        btAudio->setAudioCallback(_audioCallback);
    }

    Serial.println("[SOUND] Sound manager ready");
    return true;
}

void SoundManager::loop() {
    // Promote queued event to playing if not currently playing
    if (_queued != SoundEvent::NONE && _pcmPos >= _pcmSamples) {
        SoundEvent next = _queued;
        _queued  = SoundEvent::NONE;
        _playing = SoundEvent::NONE;

        if (_btAudio && _btAudio->isConnected()) {
            if (_fetchAndDecode(next)) {
                _pcmPos  = 0;
                _playing = next;
            }
        } else if (_buzzerPin != 255) {
            // Fallback: buzzer tone
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

int32_t SoundManager::_audioCallback(Frame* frame, int32_t frame_count) {
    if (!_instance || _instance->_pcmPos >= _instance->_pcmSamples) {
        memset(frame, 0, frame_count * sizeof(Frame));
        return frame_count;
    }

    int32_t remaining = (int32_t)(_instance->_pcmSamples - _instance->_pcmPos);
    int32_t toWrite   = min(frame_count, remaining);

    const int16_t* src = _instance->_pcmBuf + _instance->_pcmPos;
    for (int32_t i = 0; i < toWrite; ++i) {
        // PCM is stereo (L,R interleaved); Frame contains {int16_t channel1, int16_t channel2}
        frame[i].channel1 = src[i * 2];
        frame[i].channel2 = src[i * 2 + 1];
    }
    _instance->_pcmPos += toWrite * 2;  // advance by stereo sample pairs

    // Pad remaining frames with silence
    if (toWrite < frame_count) {
        memset(frame + toWrite, 0, (frame_count - toWrite) * sizeof(Frame));
    }

    return frame_count;
}

bool SoundManager::_fetchAndDecode(SoundEvent event) {
    const char* filename = _fileNameForEvent(event);
    if (!filename) return false;

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%u/assets/sounds/%s", _host, _port, filename);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);  // 8 s timeout
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[SOUND] HTTP %d fetching %s\n", code, url);
        http.end();
        return false;
    }

    size_t len = http.getSize();
    if (len == 0 || len > PCM_BUF_MAX) {
        Serial.printf("[SOUND] File too large or empty (%u bytes): %s\n", len, filename);
        http.end();
        return false;
    }

    // Read WAV data into a temporary buffer
    uint8_t* wavBuf = reinterpret_cast<uint8_t*>(_pcmBuf);  // reuse PCM buf as staging
    size_t   bytesRead = http.getStream().readBytes(reinterpret_cast<char*>(wavBuf), len);
    http.end();

    if (bytesRead < 44) {  // WAV header minimum
        Serial.printf("[SOUND] Short read (%u bytes) for %s\n", bytesRead, filename);
        return false;
    }

    uint32_t pcmOffset = 0, numSamples = 0;
    if (!_parseWavHeader(wavBuf, bytesRead, pcmOffset, numSamples)) {
        Serial.printf("[SOUND] WAV header parse failed for %s\n", filename);
        return false;
    }

    // Copy PCM data to start of PCM buffer (may overlap — use memmove)
    memmove(_pcmBuf, wavBuf + pcmOffset, numSamples * sizeof(int16_t));
    _pcmSamples = numSamples;
    _pcmPos     = 0;

    Serial.printf("[SOUND] Loaded %s: %u stereo samples\n", filename, numSamples / 2);
    return true;
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

bool SoundManager::_parseWavHeader(const uint8_t* data, size_t dataLen,
                                    uint32_t& pcmOffset, uint32_t& numSamples) {
    // Standard RIFF WAV header layout:
    //  Offset  Size  Field
    //  0       4     "RIFF"
    //  4       4     file size - 8
    //  8       4     "WAVE"
    //  12      4     "fmt "
    //  16      4     fmt chunk size (16 for PCM)
    //  20      2     audio format (1=PCM)
    //  22      2     num channels
    //  24      4     sample rate
    //  28      4     byte rate
    //  32      2     block align
    //  34      2     bits per sample
    //  36      4     "data"
    //  40      4     data chunk size

    if (dataLen < 44) return false;
    if (memcmp(data,     "RIFF", 4) != 0) return false;
    if (memcmp(data + 8, "WAVE", 4) != 0) return false;

    // Walk sub-chunks to find "data"
    uint32_t offset = 12;
    while (offset + 8 <= dataLen) {
        uint32_t chunkSize;
        memcpy(&chunkSize, data + offset + 4, 4);
        if (memcmp(data + offset, "data", 4) == 0) {
            pcmOffset  = offset + 8;
            // numSamples = total int16_t values in the data chunk
            numSamples = chunkSize / sizeof(int16_t);
            return true;
        }
        offset += 8 + chunkSize;
        if (chunkSize % 2 != 0) offset++;  // RIFF chunks are word-aligned
    }
    return false;
}

void SoundManager::_buzzerTone(uint32_t freqHz, uint32_t durationMs) {
    if (_buzzerPin == 255) return;
    ledcWriteTone(0, freqHz);
    delay(durationMs);
    ledcWriteTone(0, 0);
}
