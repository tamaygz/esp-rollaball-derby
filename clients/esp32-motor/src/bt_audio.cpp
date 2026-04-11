#include "bt_audio.h"
#include <Preferences.h>
#include <Arduino.h>

BtAudio* BtAudio::_instance = nullptr;

bool BtAudio::begin() {
    _instance = this;

    // Attempt to load last paired device address from NVS
    bool hadPaired = _loadPairedAddress();

    _a2dp.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void* obj) {
        BtAudio* self = static_cast<BtAudio*>(obj);
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            self->_connected = true;
            Serial.printf("[BT] Connected to: %s\n", self->_pairedName);
        } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            self->_connected = false;
            Serial.println("[BT] Disconnected");
        }
    }, this);

    // Register audio data callback
    _a2dp.set_data_callback_in_frames(_staticAudioCb);

    _a2dp.start("DerbyMotor", false);   // false = do not auto-connect

    if (hadPaired && _pairedAddress[0] != '\0') {
        Serial.printf("[BT] Auto-connecting to saved device: %s (%s)\n",
                      _pairedName, _pairedAddress);
        // Convert address string to esp_bd_addr_t
        uint8_t addr[6];
        if (sscanf(_pairedAddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) == 6) {
            _a2dp.connect(addr);
        }
    }

    _available = true;
    Serial.println("[BT] A2DP Source started");
    return true;
}

void BtAudio::loop() {
    // BluetoothA2DPSource is callback-driven; nothing needed here for now.
    // Future: handle reconnect timer.
}

uint8_t BtAudio::scan(uint8_t timeoutSec) {
    _scanCount = 0;
    Serial.printf("[BT] Scanning for %u s...\n", timeoutSec);

    // Use the ESP-IDF discovery API via the a2dp wrapper
    // Note: ESP32-A2DP library scan is configured via startDiscovery() on some versions;
    // using a simpler blocking approach here for admin UI use.
    _a2dp.discover_bluetooth_devices(timeoutSec * 1000);

    auto* results = _a2dp.get_discovered_devices();
    int count     = _a2dp.get_discovered_devices_count();

    for (int i = 0; i < count && _scanCount < BT_SCAN_MAX_RESULTS; ++i) {
        strlcpy(_scanResults[_scanCount].name,    results[i].name,    32);
        strlcpy(_scanResults[_scanCount].address, results[i].address, 18);
        _scanResults[_scanCount].rssi = results[i].rssi;
        _scanCount++;
    }

    Serial.printf("[BT] Scan found %u device(s)\n", (unsigned)_scanCount);
    return _scanCount;
}

bool BtAudio::connect(const char* address) {
    if (!address) return false;
    uint8_t addr[6];
    if (sscanf(address, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) != 6) {
        Serial.println("[BT] connect: invalid address format");
        return false;
    }

    // Find device name from scan results
    const char* devName = "";
    for (uint8_t i = 0; i < _scanCount; ++i) {
        if (strcasecmp(_scanResults[i].address, address) == 0) {
            devName = _scanResults[i].name;
            break;
        }
    }

    strlcpy(_pairedAddress, address, sizeof(_pairedAddress));
    strlcpy(_pairedName, devName, sizeof(_pairedName));
    _persistPairedAddress(address, devName);

    _a2dp.connect(addr);
    Serial.printf("[BT] Connecting to %s (%s)...\n", devName, address);
    return true;
}

void BtAudio::disconnect(bool forget) {
    _a2dp.disconnect();
    _connected = false;
    if (forget) {
        _pairedAddress[0] = '\0';
        _pairedName[0]    = '\0';
        _persistPairedAddress("", "");
        Serial.println("[BT] Unpaired and forgotten");
    } else {
        Serial.println("[BT] Disconnected (pairing retained)");
    }
}

const BtDevice* BtAudio::scanResult(uint8_t i) const {
    if (i >= _scanCount) return nullptr;
    return &_scanResults[i];
}

void BtAudio::setAudioCallback(AudioCallback cb) {
    _audioCb = cb;
}

int32_t BtAudio::_staticAudioCb(Frame* frame, int32_t frame_count) {
    if (_instance && _instance->_audioCb) {
        return _instance->_audioCb(frame, frame_count);
    }
    // Silence
    memset(frame, 0, frame_count * sizeof(Frame));
    return frame_count;
}

void BtAudio::_persistPairedAddress(const char* address, const char* name) {
    Preferences prefs;
    prefs.begin("bt", false);
    prefs.putString("addr", address);
    prefs.putString("name", name);
    prefs.end();
}

bool BtAudio::_loadPairedAddress() {
    Preferences prefs;
    prefs.begin("bt", true);
    String addr = prefs.getString("addr", "");
    String name = prefs.getString("name", "");
    prefs.end();

    if (addr.length() > 0) {
        strlcpy(_pairedAddress, addr.c_str(), sizeof(_pairedAddress));
        strlcpy(_pairedName,    name.c_str(), sizeof(_pairedName));
        Serial.printf("[BT] Loaded paired device: %s (%s)\n",
                      _pairedName, _pairedAddress);
        return true;
    }
    return false;
}
