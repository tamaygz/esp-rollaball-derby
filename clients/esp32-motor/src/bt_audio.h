#pragma once
#include <Arduino.h>
#include <BluetoothA2DPSource.h>

// Maximum number of scan results kept in memory
#define BT_SCAN_MAX_RESULTS  16

struct BtDevice {
    char    name[32];
    char    address[18];  // "XX:XX:XX:XX:XX:XX\0"
    int8_t  rssi;
};

// Bluetooth A2DP Source manager.
// Streams PCM audio data (16-bit stereo 44100 Hz) to a paired BT speaker.
// Audio data is provided by registering a data callback (set by SoundManager).
//
// Graceful degradation: all methods are safe to call; if BT is not available or
// no speaker is paired, they silently do nothing.

class BtAudio {
public:
    // Initialise BT stack, attempt auto-connect to last paired device.
    // Returns false if BT init fails (e.g. BT disabled in SDK config).
    bool begin();

    // Non-blocking update — call every loop()
    void loop();

    // Scan for nearby A2DP sink devices.
    // Blocking: up to timeoutSec seconds; returns discovered device count.
    uint8_t scan(uint8_t timeoutSec = 10);

    // Connect to a specific device by address string "XX:XX:XX:XX:XX:XX"
    bool connect(const char* address);

    // Disconnect and optionally forget (un-pair) the current device
    void disconnect(bool forget = false);

    // Status queries
    bool        isConnected()         const { return _connected; }
    const char* getPairedDeviceName() const { return _pairedName; }
    const char* getPairedAddress()    const { return _pairedAddress; }
    bool        hasPairedDevice()     const { return _pairedAddress[0] != '\0'; }

    // Scan results (valid after scan() returns)
    uint8_t          scanResultCount() const { return _scanCount; }
    const BtDevice*  scanResult(uint8_t i) const;

    // Register a callback that provides PCM data for the A2DP stream.
    // The callback must fill `buf` with `len` int16_t samples (stereo interleaved).
    // Returns number of samples written (0 = silence).
    using AudioCallback = int32_t(*)(Frame* frame, int32_t frame_count);
    void setAudioCallback(AudioCallback cb);

    bool isAvailable() const { return _available; }

private:
    BluetoothA2DPSource _a2dp;
    bool        _available    = false;
    bool        _connected    = false;
    char        _pairedName[32]    = {};
    char        _pairedAddress[18] = {};

    BtDevice    _scanResults[BT_SCAN_MAX_RESULTS] = {};
    uint8_t     _scanCount   = 0;

    static BtAudio* _instance;
    static int32_t  _staticAudioCb(Frame* frame, int32_t frame_count);

    AudioCallback _audioCb = nullptr;

    void _onConnected();
    void _onDisconnected();
    void _persistPairedAddress(const char* address, const char* name);
    bool _loadPairedAddress();
};
