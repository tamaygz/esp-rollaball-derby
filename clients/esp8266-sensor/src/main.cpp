#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "websocket.h"
#include "sensors.h"
#include "led.h"

// ─── Global Instances ─────────────────────────────────────────────────────────
static WSClient   wsClient;
static Sensors    sensors;
static StatusLed  led;

// ─── Runtime Config (loaded from LittleFS, populated by WiFiManager) ──────────
static char g_serverIp  [40] = "192.168.1.200";
static char g_serverPort[ 6] = "3000";
static char g_playerName[21] = "";

// ─── WiFiManager ──────────────────────────────────────────────────────────────
static WiFiManager          wifiManager;
static WiFiManagerParameter* param_ip;
static WiFiManagerParameter* param_port;
static WiFiManagerParameter* param_name;

// ─── LittleFS Config Helpers ──────────────────────────────────────────────────

static void loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("[CFG] No config file — using defaults");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        Serial.println("[CFG] Failed to open config file");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[CFG] Parse error (%s) — using defaults\n", err.c_str());
        return;
    }

    if (doc["server_ip"  ].is<const char*>()) strlcpy(g_serverIp,   doc["server_ip"],   sizeof(g_serverIp));
    if (doc["server_port"].is<const char*>()) strlcpy(g_serverPort,  doc["server_port"], sizeof(g_serverPort));
    if (doc["player_name"].is<const char*>()) strlcpy(g_playerName,  doc["player_name"], sizeof(g_playerName));

    Serial.printf("[CFG] Loaded: ip=%s port=%s name=%s\n",
                  g_serverIp, g_serverPort, g_playerName);
}

static void saveConfig() {
    JsonDocument doc;
    doc["server_ip"  ] = g_serverIp;
    doc["server_port"] = g_serverPort;
    doc["player_name"] = g_playerName;

    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        Serial.println("[CFG] Failed to write config file");
        return;
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[CFG] Config saved");
}

// ─── WiFiManager Save Callback ────────────────────────────────────────────────

static void onSaveParams() {
    strlcpy(g_serverIp,   param_ip->getValue(),   sizeof(g_serverIp));
    strlcpy(g_serverPort, param_port->getValue(),  sizeof(g_serverPort));
    strlcpy(g_playerName, param_name->getValue(),  sizeof(g_playerName));
    saveConfig();
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println("\n[BOOT] Roll-a-Ball Derby — Sensor Client");

    led.begin(PIN_LED);
    led.setState(LedState::NO_WIFI);

    sensors.begin();

    // Mount LittleFS; format if mount fails (first boot or corruption).
    if (!LittleFS.begin()) {
        Serial.println("[CFG] LittleFS mount failed — formatting...");
        LittleFS.format();
        if (!LittleFS.begin()) {
            Serial.println("[CFG] LittleFS mount failed after format — continuing without persistent config");
        }
    }
    loadConfig();

    // Build unique AP name: "Derby-Sensor-XXXX" using last 4 hex digits of chip ID.
    char apName[32];
    snprintf(apName, sizeof(apName), "%s%04X",
             WIFIMANAGER_AP_PREFIX, ESP.getChipId() & 0xFFFF);

    // WiFiManager custom parameters — pre-filled from saved config.
    param_ip   = new WiFiManagerParameter("server_ip",   "Server IP",   g_serverIp,   39);
    param_port = new WiFiManagerParameter("server_port", "Server Port", g_serverPort,  5);
    param_name = new WiFiManagerParameter("player_name", "Player Name", g_playerName, 20);

    wifiManager.addParameter(param_ip);
    wifiManager.addParameter(param_port);
    wifiManager.addParameter(param_name);
    wifiManager.setSaveParamsCallback(onSaveParams);

    // If no saved credentials, open the captive-portal AP for 3 minutes.
    // If credentials are saved, connect automatically without opening the portal.
    wifiManager.setConfigPortalTimeout(180);

    Serial.printf("[WiFi] Auto-connecting (AP: %s if needed)\n", apName);
    if (!wifiManager.autoConnect(apName)) {
        // autoConnect() returns false on timeout; reboot and try again.
        Serial.println("[WiFi] autoConnect timed out — rebooting");
        ESP.restart();
    }

    Serial.printf("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    led.setState(LedState::WIFI_ONLY);

    uint16_t port = static_cast<uint16_t>(atoi(g_serverPort));
    wsClient.begin(g_serverIp, port, g_playerName);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

static bool s_wifiWasConnected = false;

void loop() {
    led.loop();

    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    if (!wifiOk) {
        if (s_wifiWasConnected) {
            s_wifiWasConnected = false;
            wsClient.onWiFiLost();
            Serial.println("[WiFi] Connection lost");
        }
        led.setState(LedState::NO_WIFI);
        return;
    }

    if (!s_wifiWasConnected) {
        s_wifiWasConnected = true;
        Serial.printf("[WiFi] Reconnected — IP: %s\n", WiFi.localIP().toString().c_str());
    }

    wsClient.loop();

    int points    = sensors.check();
    GameEvent ev  = wsClient.pollEvent();

    if (wsClient.isConnected()) {
        led.setState(LedState::WS_CONNECTED);

        // Game event LED feedback
        if (ev == GameEvent::COUNTDOWN_TICK) {
            led.triggerCountdownTick();
        } else if (ev == GameEvent::WINNER_SELF) {
            led.triggerWinner();
        } else if (ev == GameEvent::WINNER_OTHER) {
            led.triggerLoser();
        }

        // Only send score events once a playerId has been assigned by the server.
        if (!wsClient.getPlayerId().isEmpty()) {
            if (points > 0) {
                Serial.printf("[SENSOR] Triggered: +%d\n", points);
                wsClient.sendScore(points);
            }
        } else if (points > 0) {
            Serial.printf("[SENSOR] Dropped trigger while waiting for player assignment: +%d\n", points);
        }
    } else {
        led.setState(LedState::WIFI_ONLY);
        if (points > 0) {
            Serial.printf("[SENSOR] Dropped offline trigger: +%d\n", points);
        }
    }
}
