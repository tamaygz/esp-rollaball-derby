#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "websocket.h"
#include "sensors.h"
#include "led.h"

// ─── Global Instances ─────────────────────────────────────────────────────────
static WSClient        wsClient;
static Sensors         sensors;
static StatusLed       led;
static ESP8266WebServer httpServer(HTTP_CONFIG_PORT);

// Flag set by the /config handler so the reboot happens after the HTTP response
// has been flushed to the client.
static bool g_pendingRestart = false;

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

// ─── HTTP Config Server ───────────────────────────────────────────────────────

// POST /config  { "server_ip": "...", "server_port": "3000", "player_name": "..." }
// Updates LittleFS config and schedules a reboot so the sensor reconnects to
// the new server.  Any omitted JSON fields keep their current values.
static void handleHttpConfig() {
    if (httpServer.method() != HTTP_POST) {
        httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
        return;
    }

    String body = httpServer.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    bool changed = false;
    if (doc["server_ip"].is<const char*>()) {
        const char* serverIp = doc["server_ip"];
        if (strcmp(serverIp, g_serverIp) != 0) {
            strlcpy(g_serverIp, serverIp, sizeof(g_serverIp));
            changed = true;
        }
    }
    if (doc["server_port"].is<const char*>()) {
        const char* serverPort = doc["server_port"];
        if (strcmp(serverPort, g_serverPort) != 0) {
            strlcpy(g_serverPort, serverPort, sizeof(g_serverPort));
            changed = true;
        }
    }
    if (doc["player_name"].is<const char*>()) {
        const char* playerName = doc["player_name"];
        if (strcmp(playerName, g_playerName) != 0) {
            strlcpy(g_playerName, playerName, sizeof(g_playerName));
            changed = true;
        }
    }

    if (changed) {
        saveConfig();
        Serial.printf("[CFG] Remote update — ip=%s port=%s name=%s — rebooting\n",
                      g_serverIp, g_serverPort, g_playerName);
        httpServer.send(200, "application/json", "{\"ok\":true}");
        g_pendingRestart = true;
    } else {
        httpServer.send(200, "application/json", "{\"ok\":true,\"changed\":false}");
    }
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

    // ─── Serial Pre-Configure Window ──────────────────────────────────────────
    // The web flasher sends DERBY_CFG:{json}\n within 3 s of boot to pre-populate
    // server IP/port/name without requiring the WiFiManager captive portal.
    // The settings are saved to LittleFS exactly like the WiFiManager would; on
    // the next step WiFiManager picks them up as pre-filled defaults.
    Serial.println(F("[CFG] Serial pre-config window 3 s — send: DERBY_CFG:{...}"));
    {
        const unsigned long kWindowMs = 3000UL;
        unsigned long deadline = millis() + kWindowMs;
        String buf;
        buf.reserve(128);
        bool done = false;
        while (!done && millis() < deadline) {
            while (!done && Serial.available()) {
                char c = static_cast<char>(Serial.read());
                if (c == '\r') continue;
                if (c == '\n') {
                    if (buf.startsWith(F("DERBY_CFG:"))) {
                        String json = buf.substring(10);
                        JsonDocument doc;
                        DeserializationError err = deserializeJson(doc, json);
                        if (!err) {
                            if (doc["server_ip"  ].is<const char*>())
                                strlcpy(g_serverIp,   doc["server_ip"],   sizeof(g_serverIp));
                            if (doc["server_port"].is<const char*>())
                                strlcpy(g_serverPort, doc["server_port"], sizeof(g_serverPort));
                            if (doc["player_name"].is<const char*>())
                                strlcpy(g_playerName, doc["player_name"], sizeof(g_playerName));
                            saveConfig();
                            Serial.println(F("[CFG] DERBY_CFG_ACK:OK"));
                            done = true;
                        } else {
                            Serial.printf("[CFG] DERBY_CFG_ACK:ERR_JSON %s\n", err.c_str());
                        }
                    }
                    buf = "";
                } else if (buf.length() < 256) {
                    buf += c;
                }
            }
            if (!done) delay(1);   // yield to ESP8266 background tasks
        }
        if (!done) Serial.println(F("[CFG] Serial config window expired"));
    }

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

    // HTTP config server — lets the Node.js admin push new config without
    // needing to re-open the WiFiManager captive portal.
    httpServer.on("/config", handleHttpConfig);
    httpServer.begin();
    Serial.printf("[HTTP] Config server listening on port %d\n", HTTP_CONFIG_PORT);
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

    httpServer.handleClient();

    // Reboot after the HTTP response has been flushed (flagged by handleHttpConfig).
    if (g_pendingRestart) {
        delay(200);
        ESP.restart();
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
