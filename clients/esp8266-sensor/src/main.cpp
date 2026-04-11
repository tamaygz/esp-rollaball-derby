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
static LedManager      ledManager;
static ESP8266WebServer httpServer(HTTP_CONFIG_PORT);

// Flag set by the /config handler so the reboot happens after the HTTP response
// has been flushed to the client.
static bool g_pendingRestart = false;

// ─── Runtime Config (loaded from LittleFS, populated by WiFiManager) ──────────
static char g_serverIp  [40] = "192.168.1.200";
static char g_serverPort[ 6] = "3000";
static char g_playerName[21] = "";

// ─── Persisted Runtime State (survives reboots) ──────────────────────────────
static char     g_playerId[48]     = "";    // UUID from server registration
static LedConfig g_savedLedConfig  = {};    // Last LED config from server
static bool     g_hasLedConfig     = false; // true once a server LED config has been saved
static bool     g_stateDirty       = false; // true when state needs to be flushed to flash
static unsigned long g_stateLastSave = 0;   // millis() of last state save
static const unsigned long STATE_SAVE_DEBOUNCE_MS = 2000; // Min interval between flash writes

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

// ─── Runtime State Persistence ────────────────────────────────────────────────
// Persists server-assigned state (playerId, LED config, device color) to a
// separate file so the device boots with the correct identity and LED setup.
// Uses atomic write (temp → rename) to prevent corruption on power loss.

static void loadState() {
    if (!LittleFS.exists(STATE_FILE)) {
        Serial.println("[STATE] No state file — using defaults");
        return;
    }

    File f = LittleFS.open(STATE_FILE, "r");
    if (!f) {
        Serial.println("[STATE] Failed to open state file");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[STATE] Parse error (%s) — using defaults\n", err.c_str());
        return;
    }

    if (doc["player_id"].is<const char*>()) {
        strlcpy(g_playerId, doc["player_id"], sizeof(g_playerId));
    }

    if (doc["led_count"].is<int>()) {
        g_savedLedConfig.ledCount   = static_cast<uint16_t>(doc["led_count"] | LED_DEFAULT_COUNT);
        g_savedLedConfig.pin        = static_cast<uint8_t>(doc["led_pin"] | LED_DEFAULT_PIN);
        g_savedLedConfig.brightness = static_cast<uint8_t>(doc["led_brightness"] | LED_DEFAULT_BRIGHTNESS);
        g_savedLedConfig.topology   = LedTopology::STRIP;
        g_savedLedConfig.matrixRows = 8;
        g_savedLedConfig.matrixCols = 8;

        const char* topo = doc["led_topology"] | "strip";
        if      (strcmp(topo, "ring")               == 0) g_savedLedConfig.topology = LedTopology::RING;
        else if (strcmp(topo, "matrix_zigzag")      == 0) g_savedLedConfig.topology = LedTopology::MATRIX_ZIGZAG;
        else if (strcmp(topo, "matrix_progressive") == 0) g_savedLedConfig.topology = LedTopology::MATRIX_PROGRESSIVE;

        g_hasLedConfig = true;
    }

    if (doc["device_color_r"].is<int>()) {
        g_savedLedConfig.deviceColorR   = static_cast<uint8_t>(doc["device_color_r"] | 0);
        g_savedLedConfig.deviceColorG   = static_cast<uint8_t>(doc["device_color_g"] | 0);
        g_savedLedConfig.deviceColorB   = static_cast<uint8_t>(doc["device_color_b"] | 0);
        g_savedLedConfig.hasDeviceColor = true;
    }

    Serial.printf("[STATE] Loaded: playerId=%s ledCount=%u hasColor=%d\n",
                  g_playerId,
                  g_hasLedConfig ? g_savedLedConfig.ledCount : 0,
                  g_savedLedConfig.hasDeviceColor ? 1 : 0);
}

static void saveState() {
    JsonDocument doc;

    if (strlen(g_playerId) > 0) {
        doc["player_id"] = g_playerId;
    }

    if (g_hasLedConfig) {
        doc["led_count"]      = g_savedLedConfig.ledCount;
        doc["led_pin"]        = g_savedLedConfig.pin;
        doc["led_brightness"] = g_savedLedConfig.brightness;

        const char* topo = "strip";
        switch (g_savedLedConfig.topology) {
            case LedTopology::RING:               topo = "ring"; break;
            case LedTopology::MATRIX_ZIGZAG:      topo = "matrix_zigzag"; break;
            case LedTopology::MATRIX_PROGRESSIVE:  topo = "matrix_progressive"; break;
            default: break;
        }
        doc["led_topology"] = topo;
    }

    if (g_savedLedConfig.hasDeviceColor) {
        doc["device_color_r"] = g_savedLedConfig.deviceColorR;
        doc["device_color_g"] = g_savedLedConfig.deviceColorG;
        doc["device_color_b"] = g_savedLedConfig.deviceColorB;
    }

    // Atomic write: write to temp file, then rename to avoid corruption.
    File f = LittleFS.open(STATE_TMP, "w");
    if (!f) {
        Serial.println("[STATE] Failed to write temp state file");
        return;
    }
    serializeJson(doc, f);
    f.close();

    LittleFS.remove(STATE_FILE);
    LittleFS.rename(STATE_TMP, STATE_FILE);

    g_stateDirty     = false;
    g_stateLastSave  = millis();
    Serial.println("[STATE] State saved");
}

// Mark state as dirty; actual write is debounced in loop().
static void markStateDirty() {
    g_stateDirty = true;
}

// Flush state to flash if dirty and debounce period has elapsed.
static void flushStateIfNeeded() {
    if (!g_stateDirty) return;
    unsigned long now = millis();
    if (now - g_stateLastSave >= STATE_SAVE_DEBOUNCE_MS) {
        saveState();
    }
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
    loadState();

    // Initialise LEDs with saved config (from previous server session) or defaults.
    if (g_hasLedConfig) {
        ledManager.begin(g_savedLedConfig);
        wsClient.setLedMetadata(g_savedLedConfig.ledCount);
        Serial.println("[BOOT] Using saved LED config from previous session");
    } else {
        ledManager.begin(ledConfigDefaults());
        wsClient.setLedMetadata(LED_DEFAULT_COUNT);
    }

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
    ledManager.setState(LedState::WIFI_ONLY);

    uint16_t port = static_cast<uint16_t>(atoi(g_serverPort));
    wsClient.begin(g_serverIp, port, g_playerName, g_playerId);

    // HTTP config server — lets the Node.js admin push new config without
    // needing to re-open the WiFiManager captive portal.
    httpServer.on("/config", handleHttpConfig);
    httpServer.begin();
    Serial.printf("[HTTP] Config server listening on port %d\n", HTTP_CONFIG_PORT);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

static bool s_wifiWasConnected = false;

void loop() {
    ledManager.loop();

    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    if (!wifiOk) {
        if (s_wifiWasConnected) {
            s_wifiWasConnected = false;
            wsClient.onWiFiLost();
            Serial.println("[WiFi] Connection lost");
        }
        ledManager.setState(LedState::NO_WIFI);
        return;
    }

    if (!s_wifiWasConnected) {
        s_wifiWasConnected = true;
        Serial.printf("[WiFi] Reconnected — IP: %s\n", WiFi.localIP().toString().c_str());
    }

    httpServer.handleClient();

    // Reboot after the HTTP response has been flushed (flagged by handleHttpConfig).
    if (g_pendingRestart) {
        // Flush any pending state before restarting.
        if (g_stateDirty) saveState();
        delay(200);
        ESP.restart();
    }

    wsClient.loop();

    // ── Persist playerId when assigned/changed ───────────────────────────────
    {
        const String& currentId = wsClient.getPlayerId();
        if (currentId.length() > 0 && strcmp(g_playerId, currentId.c_str()) != 0) {
            strlcpy(g_playerId, currentId.c_str(), sizeof(g_playerId));
            markStateDirty();
            Serial.printf("[STATE] playerId changed: %s\n", g_playerId);
        }
    }

    // ── LED config hot-reload ────────────────────────────────────────────────
    LedConfig pendingCfg;
    if (wsClient.pollLedConfig(pendingCfg)) {
        ledManager.applyConfig(pendingCfg);
        wsClient.setLedMetadata(pendingCfg.ledCount);
        // Persist the LED config so next boot starts with the correct setup.
        g_savedLedConfig = pendingCfg;
        g_hasLedConfig   = true;
        markStateDirty();
    }

    // ── LED test effect ──────────────────────────────────────────────────────
    LedTestEffectMessage pendingEffect;
    if (wsClient.pollTestEffect(pendingEffect)) {
        ledManager.playTestEffect(pendingEffect);
    }

    int points      = sensors.check();
    LocalEvent  lev = wsClient.pollLocalEvent();
    GlobalEvent gev = wsClient.pollGlobalEvent();

    if (wsClient.isConnected()) {
        ledManager.setState(LedState::WS_CONNECTED);

        // Global events: all devices react (countdown, lifecycle, winner).
        bool winnerEvent = false;
        switch (gev) {
            case GlobalEvent::COUNTDOWN_TICK: ledManager.onGlobalEvent(GlobalEventType::COUNTDOWN_TICK); break;
            case GlobalEvent::GAME_STARTED:   ledManager.onGlobalEvent(GlobalEventType::GAME_STARTED);   break;
            case GlobalEvent::GAME_PAUSED:    ledManager.onGlobalEvent(GlobalEventType::GAME_PAUSED);    break;
            case GlobalEvent::GAME_RESUMED:   ledManager.onGlobalEvent(GlobalEventType::GAME_RESUMED);   break;
            case GlobalEvent::GAME_RESET:     ledManager.onGlobalEvent(GlobalEventType::GAME_RESET);     break;
            case GlobalEvent::WINNER_SELF:    ledManager.onGlobalEvent(GlobalEventType::WINNER_SELF);    winnerEvent = true; break;
            case GlobalEvent::WINNER_OTHER:   ledManager.onGlobalEvent(GlobalEventType::WINNER_OTHER);   winnerEvent = true; break;
            default: break;
        }

        // Device-local events: only the owning device reacts (scoring, rank, streaks).
        // Skip when a winner event fired — the winning score arrives in the same
        // WS batch and would immediately overwrite the rainbow/pulse effect.
        if (winnerEvent) lev = LocalEvent::NONE;
        switch (lev) {
            case LocalEvent::SCORE_PLUS1:   ledManager.onLocalEvent(LocalEventType::SCORE_PLUS1);   break;
            case LocalEvent::SCORE_PLUS2:   ledManager.onLocalEvent(LocalEventType::SCORE_PLUS2);   break;
            case LocalEvent::SCORE_PLUS3:   ledManager.onLocalEvent(LocalEventType::SCORE_PLUS3);   break;
            case LocalEvent::ZERO_ROLL:     ledManager.onLocalEvent(LocalEventType::ZERO_ROLL);     break;
            case LocalEvent::TOOK_LEAD:     ledManager.onLocalEvent(LocalEventType::TOOK_LEAD);     break;
            case LocalEvent::BECAME_LAST:   ledManager.onLocalEvent(LocalEventType::BECAME_LAST);   break;
            case LocalEvent::STREAK_ZERO:   ledManager.onLocalEvent(LocalEventType::STREAK_ZERO);   break;
            case LocalEvent::STREAK_THREE:  ledManager.onLocalEvent(LocalEventType::STREAK_THREE);  break;
            default: break;
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
        ledManager.setState(LedState::WIFI_ONLY);
        if (points > 0) {
            Serial.printf("[SENSOR] Dropped offline trigger: +%d\n", points);
        }
    }

    // ── Debounced state flush to LittleFS ────────────────────────────────────
    flushStateIfNeeded();
}
