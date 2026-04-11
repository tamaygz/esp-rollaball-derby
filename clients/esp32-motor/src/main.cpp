#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "websocket.h"
#include "motor_manager.h"
#include "motor_calibration.h"
#include "buttons.h"
#include "led.h"
#include "matrix_display.h"
#include "bt_audio.h"
#include "sound.h"

// ─── Global Instances ─────────────────────────────────────────────────────────
static WSClient       wsClient;
static MotorManager   motorManager;
static Buttons        buttons;
static StatusLed      statusLed;
static MatrixDisplay  matrixDisplay;
static BtAudio        btAudio;
static SoundManager   soundManager;
static WebServer      httpServer(HTTP_CONFIG_PORT);

static bool           g_pendingRestart = false;

// ─── Runtime Config ───────────────────────────────────────────────────────────
static char    g_serverIp  [40] = "192.168.1.200";
static char    g_serverPort[ 6] = "3000";
static char    g_playerName[21] = "";

// ─── Persisted Runtime State ──────────────────────────────────────────────────
static char      g_playerId[48]    = "";
static LedConfig g_savedLedConfig  = {};
static bool      g_hasLedConfig    = false;
static bool      g_stateDirty      = false;
static unsigned long g_stateLastSave = 0;
static const unsigned long STATE_SAVE_DEBOUNCE_MS = 2000;

static uint8_t  g_motorColors[MOTOR_MAX_LANES] = DEFAULT_MOTOR_COLORS;

// ─── WiFiManager ──────────────────────────────────────────────────────────────
static WiFiManager            wifiManager;
static WiFiManagerParameter*  param_ip;
static WiFiManagerParameter*  param_port;
static WiFiManagerParameter*  param_name;

// ─── Forward Declarations ─────────────────────────────────────────────────────
static void loadConfig();
static void saveConfig();
static void loadState();
static void saveState();
static void markStateDirty();
static void flushStateIfNeeded();
static bool discoverServer(String& host, uint16_t& port);
static void onSaveParams();
static void setupHttpRoutes();

// ─── Config Helpers ───────────────────────────────────────────────────────────

static void loadConfig() {
    if (!LittleFS.exists(CONFIG_FILE)) { return; }
    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    if (!deserializeJson(doc, f)) {
        if (doc["server_ip"  ].is<const char*>()) strlcpy(g_serverIp,  doc["server_ip"],   sizeof(g_serverIp));
        if (doc["server_port"].is<const char*>()) strlcpy(g_serverPort, doc["server_port"], sizeof(g_serverPort));
        if (doc["player_name"].is<const char*>()) strlcpy(g_playerName, doc["player_name"], sizeof(g_playerName));
    }
    f.close();
    Serial.printf("[CFG] Loaded: ip=%s port=%s name=%s\n", g_serverIp, g_serverPort, g_playerName);
}

static void saveConfig() {
    JsonDocument doc;
    doc["server_ip"]   = g_serverIp;
    doc["server_port"] = g_serverPort;
    doc["player_name"] = g_playerName;
    File f = LittleFS.open(CONFIG_FILE, "w");
    if (f) { serializeJson(doc, f); f.close(); }
}

static bool isValidJson(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    JsonDocument tmp;
    bool ok = !deserializeJson(tmp, f);
    f.close();
    return ok;
}

static void loadState() {
    // Atomic recovery: prefer STATE_FILE, fall back to STATE_TMP if main is corrupt.
    if (LittleFS.exists(STATE_TMP)) {
        if (!LittleFS.exists(STATE_FILE)) {
            LittleFS.rename(STATE_TMP, STATE_FILE);
        } else if (!isValidJson(STATE_FILE) && isValidJson(STATE_TMP)) {
            LittleFS.remove(STATE_FILE);
            LittleFS.rename(STATE_TMP, STATE_FILE);
        } else {
            LittleFS.remove(STATE_TMP);
        }
    }

    if (!LittleFS.exists(STATE_FILE)) return;
    File f = LittleFS.open(STATE_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) { Serial.printf("[STATE] Parse error: %s\n", err.c_str()); return; }

    if (doc["player_id"].is<const char*>()) strlcpy(g_playerId, doc["player_id"], sizeof(g_playerId));

    if (doc["led_count"].is<int>()) {
        int cnt = doc["led_count"] | 0;
        int pin = doc["led_pin"]   | (int)PIN_LED_MATRIX;
        int bri = doc["led_brightness"] | (int)LED_DEFAULT_BRIGHTNESS;
        if (cnt > 0 && cnt <= 1024) {
            g_savedLedConfig.ledCount   = (uint16_t)cnt;
            g_savedLedConfig.pin        = (uint8_t)pin;
            g_savedLedConfig.brightness = (uint8_t)bri;
            const char* topo = doc["led_topology"] | "matrix_zigzag";
            g_savedLedConfig.topology = LedTopology::MATRIX_ZIGZAG;
            if (strcmp(topo, "strip")               == 0) g_savedLedConfig.topology = LedTopology::STRIP;
            else if (strcmp(topo, "matrix_progressive") == 0) g_savedLedConfig.topology = LedTopology::MATRIX_PROGRESSIVE;
            g_savedLedConfig.matrixRows = (uint8_t)(doc["matrix_rows"] | 8);
            g_savedLedConfig.matrixCols = (uint8_t)(doc["matrix_cols"] | 8);
            g_hasLedConfig = true;
        }
    }
    if (doc["device_color_r"].is<int>()) {
        g_savedLedConfig.deviceColorR = (uint8_t)(doc["device_color_r"] | 0);
        g_savedLedConfig.deviceColorG = (uint8_t)(doc["device_color_g"] | 0);
        g_savedLedConfig.deviceColorB = (uint8_t)(doc["device_color_b"] | 0);
        g_savedLedConfig.hasDeviceColor = true;
    }
    // Motor colors
    if (doc["motor_colors"].is<JsonArrayConst>()) {
        uint8_t i = 0;
        for (JsonVariantConst v : doc["motor_colors"].as<JsonArrayConst>()) {
        if (i >= MOTOR_MAX_LANES) break;
            g_motorColors[i++] = v.as<uint8_t>();
        }
    }
    Serial.printf("[STATE] Loaded: playerId=%s\n", g_playerId);
}

static void saveState() {
    JsonDocument doc;
    if (strlen(g_playerId) > 0) doc["player_id"] = g_playerId;
    if (g_hasLedConfig) {
        doc["led_count"]      = g_savedLedConfig.ledCount;
        doc["led_pin"]        = g_savedLedConfig.pin;
        doc["led_brightness"] = g_savedLedConfig.brightness;
        doc["matrix_rows"]    = g_savedLedConfig.matrixRows;
        doc["matrix_cols"]    = g_savedLedConfig.matrixCols;
        const char* topo = "matrix_zigzag";
        switch (g_savedLedConfig.topology) {
            case LedTopology::STRIP:               topo = "strip";               break;
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
    JsonArray mc = doc["motor_colors"].to<JsonArray>();
    for (uint8_t i = 0; i < MOTOR_MAX_LANES; i++) mc.add(g_motorColors[i]);

    File f = LittleFS.open(STATE_TMP, "w");
    if (!f) { Serial.println("[STATE] Write failed"); return; }
    serializeJson(doc, f); f.close();
    LittleFS.remove(STATE_FILE);
    if (!LittleFS.rename(STATE_TMP, STATE_FILE)) {
        Serial.println("[STATE] Rename failed");
        return;
    }
    g_stateDirty    = false;
    g_stateLastSave = millis();
    Serial.println("[STATE] Saved");
}

static void markStateDirty() { g_stateDirty = true; }

static void flushStateIfNeeded() {
    if (!g_stateDirty) return;
    if (millis() - g_stateLastSave >= STATE_SAVE_DEBOUNCE_MS) saveState();
}

// ─── mDNS Discovery ───────────────────────────────────────────────────────────

static bool discoverServer(String& host, uint16_t& port) {
    char mdnsName[32];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(mdnsName, sizeof(mdnsName), "derby-motor-%04x", (uint16_t)(mac & 0xFFFF));

    if (!MDNS.begin(mdnsName)) return false;

    Serial.println("[mDNS] Querying _derby._tcp...");
    int n = MDNS.queryService("derby", "tcp");
    if (n > 0) {
        host = MDNS.IP(0).toString();
        port = MDNS.port(0);
        Serial.printf("[mDNS] Server at %s:%u\n", host.c_str(), port);
        return true;
    }
    return false;
}

// ─── WiFiManager callback ─────────────────────────────────────────────────────

static void onSaveParams() {
    strlcpy(g_serverIp,   param_ip->getValue(),   sizeof(g_serverIp));
    strlcpy(g_serverPort, param_port->getValue(),  sizeof(g_serverPort));
    strlcpy(g_playerName, param_name->getValue(),  sizeof(g_playerName));
    saveConfig();
}

// ─── HTTP Route Handlers ──────────────────────────────────────────────────────

// GET /api/motor/status
static void handleMotorStatus() {
    JsonDocument doc;
    JsonArray lanes = doc["lanes"].to<JsonArray>();
    for (uint8_t i = 0; i < motorManager.laneCount(); i++) {
        JsonObject lane = lanes.add<JsonObject>();
        lane["id"]         = i;
        lane["position"]   = motorManager.getLaneNormalisedPosition(i);
        lane["calibrated"] = motorManager.isLaneCalibrated(i);
        lane["homed"]      = motorManager.isLaneHomed(i);
        lane["moving"]     = motorManager.isLaneMoving(i);
    }
    doc["motorCount"] = motorManager.laneCount();
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// POST /api/motor/jog   { "lane": 0, "steps": 100 }
static void handleMotorJog() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    int32_t steps = req["steps"] | 0;
    motorManager.jogLane(lane, steps);
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/moveto   { "lane": 0, "position": 0.5 }
static void handleMotorMoveTo() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    float   pos  = req["position"] | 0.0f;
    motorManager.moveLaneToNormalized(lane, pos);
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/home
static void handleMotorHome() {
    motorManager.homeAll();
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/calibrate/start   { "lane": 0 }
static void handleCalibrateStart() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    motorManager.calibration().beginCalibration(lane);
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/calibrate/set_start   { "lane": 0 }
static void handleCalibrateSetStart() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    motorManager.calibration().setStartPosition(lane, motorManager.currentStep(lane));
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/calibrate/set_end   { "lane": 0 }
static void handleCalibrateSetEnd() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    motorManager.calibration().setEndPosition(lane, motorManager.currentStep(lane));
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/calibrate/finish   { "lane": 0 }
static void handleCalibrateFinish() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    if (!motorManager.calibration().finishCalibration(lane)) {
        httpServer.send(400, "application/json", "{\"error\":\"Calibration invalid (start==end)\"}" );
        return;
    }
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /api/motor/calibrate/reset   { "lane": 0 }
static void handleCalibrateReset() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    uint8_t lane = req["lane"] | 0;
    motorManager.calibration().resetCalibration(lane);
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// GET /api/motor/config
static void handleMotorConfigGet() {
    JsonDocument doc;
    JsonArray lanes = doc["lanes"].to<JsonArray>();
    for (uint8_t i = 0; i < motorManager.laneCount(); i++) {
        const LaneCalibration& c = motorManager.calibration().getLane(i);
        JsonObject laneObj = lanes.add<JsonObject>();
        laneObj["id"]              = i;
        laneObj["calibrated"]      = c.calibrated;
        laneObj["startStep"]       = c.startStep;
        laneObj["endStep"]         = c.endStep;
        laneObj["totalTrackSteps"] = c.totalTrackSteps;
        laneObj["stepsPerMm"]      = c.stepsPerMm;
        laneObj["directionReversed"] = c.directionReversed;
        laneObj["maxSpeed"]        = c.maxSpeed;
        laneObj["acceleration"]    = c.acceleration;
    }
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// POST /api/motor/config   { "lanes": [ { "id": 0, "maxSpeed": 800, ... } ] }
static void handleMotorConfigPost() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    if (req["lanes"].is<JsonArrayConst>()) {
        for (JsonVariantConst laneJson : req["lanes"].as<JsonArrayConst>()) {
            uint8_t id = laneJson["id"] | 0;
            if (id < motorManager.laneCount()) {
                motorManager.calibration().setFromJson(id, laneJson.as<JsonObjectConst>());
            }
        }
    }
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// GET /api/bt/status
static void handleBtStatus() {
    JsonDocument doc;
    doc["connected"]    = btAudio.isConnected();
    doc["pairedDevice"] = btAudio.getPairedDeviceName();
    doc["pairedAddress"] = btAudio.getPairedAddress();
    doc["available"]    = btAudio.isAvailable();
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// GET /api/bt/scan   (blocking: may take ~10 s)
static void handleBtScan() {
    uint8_t count = btAudio.scan(10);
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    for (uint8_t i = 0; i < count; i++) {
        const BtDevice* d = btAudio.scanResult(i);
        if (!d) continue;
        JsonObject dev = devices.add<JsonObject>();
        dev["name"]    = d->name;
        dev["address"] = d->address;
        dev["rssi"]    = d->rssi;
    }
    doc["count"] = count;
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// POST /api/bt/pair   { "address": "XX:XX:XX:XX:XX:XX" }
static void handleBtPair() {
    if (httpServer.method() != HTTP_POST) { httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}"); return; }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) { httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    const char* addr = req["address"] | "";
    if (!btAudio.connect(addr)) {
        httpServer.send(400, "application/json", "{\"error\":\"Invalid address\"}");
        return;
    }
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// DELETE /api/bt/unpair
static void handleBtUnpair() {
    btAudio.disconnect(true);
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

// POST /config   { "server_ip": "...", "server_port": "3000", "player_name": "..." }
static void handleHttpConfig() {
    if (httpServer.method() != HTTP_POST) {
        httpServer.send(405, "application/json", "{\"error\":\"Method Not Allowed\"}");
        return;
    }
    JsonDocument req;
    if (deserializeJson(req, httpServer.arg("plain"))) {
        httpServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    bool changed = false;
    if (req["server_ip"].is<const char*>() && strcmp(req["server_ip"], g_serverIp) != 0) {
        strlcpy(g_serverIp, req["server_ip"], sizeof(g_serverIp)); changed = true;
    }
    if (req["server_port"].is<const char*>() && strcmp(req["server_port"], g_serverPort) != 0) {
        strlcpy(g_serverPort, req["server_port"], sizeof(g_serverPort)); changed = true;
    }
    if (req["player_name"].is<const char*>() && strcmp(req["player_name"], g_playerName) != 0) {
        strlcpy(g_playerName, req["player_name"], sizeof(g_playerName)); changed = true;
    }
    if (changed) {
        saveConfig();
        httpServer.send(200, "application/json", "{\"ok\":true}");
        g_pendingRestart = true;
    } else {
        httpServer.send(200, "application/json", "{\"ok\":true,\"changed\":false}");
    }
}

static void setupHttpRoutes() {
    httpServer.on("/api/motor/status",           HTTP_GET,  handleMotorStatus);
    httpServer.on("/api/motor/jog",              HTTP_POST, handleMotorJog);
    httpServer.on("/api/motor/moveto",           HTTP_POST, handleMotorMoveTo);
    httpServer.on("/api/motor/home",             HTTP_POST, handleMotorHome);
    httpServer.on("/api/motor/calibrate/start",     HTTP_POST, handleCalibrateStart);
    httpServer.on("/api/motor/calibrate/set_start", HTTP_POST, handleCalibrateSetStart);
    httpServer.on("/api/motor/calibrate/set_end",   HTTP_POST, handleCalibrateSetEnd);
    httpServer.on("/api/motor/calibrate/finish",    HTTP_POST, handleCalibrateFinish);
    httpServer.on("/api/motor/calibrate/reset",     HTTP_POST, handleCalibrateReset);
    httpServer.on("/api/motor/config",           HTTP_GET,  handleMotorConfigGet);
    httpServer.on("/api/motor/config",           HTTP_POST, handleMotorConfigPost);
    httpServer.on("/api/bt/status",              HTTP_GET,  handleBtStatus);
    httpServer.on("/api/bt/scan",                HTTP_GET,  handleBtScan);
    httpServer.on("/api/bt/pair",                HTTP_POST, handleBtPair);
    httpServer.on("/api/bt/unpair",              HTTP_DELETE, handleBtUnpair);
    httpServer.on("/config",                     handleHttpConfig);
    httpServer.begin();
    Serial.printf("[HTTP] REST API listening on port %d\n", HTTP_CONFIG_PORT);
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println("\n[BOOT] Roll-a-Ball Derby — Motor Client");

    // Status LED: fast blink = no WiFi
    statusLed.begin(PIN_STATUS_LED);
    statusLed.setState(LedState::NO_WIFI);

    // LittleFS
    if (!LittleFS.begin(true)) {  // true = format on fail
        Serial.println("[CFG] LittleFS mount failed — continuing without persistence");
    }
    loadConfig();
    loadState();

    // Motors
    motorManager.begin();

    // Matrix display
    if (g_hasLedConfig) {
        matrixDisplay.begin(g_savedLedConfig);
        wsClient.setLedMetadata(g_savedLedConfig.ledCount);
    } else {
        LedConfig defaultCfg = ledConfigDefaults();
        matrixDisplay.begin(defaultCfg);
        wsClient.setLedMetadata(defaultCfg.ledCount);
    }
    matrixDisplay.showIdle();

    // Buttons — callback sends WS button event
    {
        const uint8_t btnPins[] = { PIN_BUTTON_1, PIN_BUTTON_2 };
        buttons.begin(btnPins, BUTTON_COUNT, [](uint8_t idx, const char* action) {
            wsClient.sendButton(idx, action);
            soundManager.play(SoundEvent::BUTTON_CLICK);
        }, BUTTON_DEBOUNCE_MS);
    }

    // Serial pre-config window (3 s) — same pattern as sensor
    Serial.println(F("[CFG] Serial pre-config window 3 s — send: DERBY_CFG:{...}"));
    {
        const unsigned long kWindowMs = 3000UL;
        unsigned long deadline = millis() + kWindowMs;
        String buf; buf.reserve(128);
        bool done = false;
        while (!done && millis() < deadline) {
            while (!done && Serial.available()) {
                char c = (char)Serial.read();
                if (c == '\r') continue;
                if (c == '\n') {
                    if (buf.startsWith(F("DERBY_CFG:"))) {
                        JsonDocument doc;
                        if (!deserializeJson(doc, buf.substring(10))) {
                            if (doc["server_ip"  ].is<const char*>()) strlcpy(g_serverIp,   doc["server_ip"],   sizeof(g_serverIp));
                            if (doc["server_port"].is<const char*>()) strlcpy(g_serverPort,  doc["server_port"], sizeof(g_serverPort));
                            if (doc["player_name"].is<const char*>()) strlcpy(g_playerName,  doc["player_name"], sizeof(g_playerName));
                            saveConfig();
                            Serial.println(F("[CFG] DERBY_CFG_ACK:OK"));
                            done = true;
                        } else {
                            Serial.println(F("[CFG] DERBY_CFG_ACK:ERR_JSON"));
                        }
                    }
                    buf = "";
                } else if (buf.length() < 256) buf += c;
            }
            if (!done) delay(1);
        }
        if (!done) Serial.println(F("[CFG] Serial config window expired"));
    }

    // WiFiManager
    char apName[32];
    uint64_t mac = ESP.getEfuseMac();
    snprintf(apName, sizeof(apName), "%s%04X", WIFIMANAGER_AP_PREFIX, (uint16_t)(mac & 0xFFFF));

    param_ip   = new WiFiManagerParameter("server_ip",   "Server IP",   g_serverIp,   39);
    param_port = new WiFiManagerParameter("server_port", "Server Port", g_serverPort,  5);
    param_name = new WiFiManagerParameter("player_name", "Player Name", g_playerName, 20);
    wifiManager.addParameter(param_ip);
    wifiManager.addParameter(param_port);
    wifiManager.addParameter(param_name);
    wifiManager.setSaveParamsCallback(onSaveParams);
    wifiManager.setConfigPortalTimeout(180);

    Serial.printf("[WiFi] Auto-connecting (AP: %s if needed)\n", apName);
    if (!wifiManager.autoConnect(apName)) {
        Serial.println("[WiFi] Timeout — rebooting");
        ESP.restart();
    }
    Serial.printf("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    statusLed.setState(LedState::WIFI_ONLY);

    // mDNS publish self as derby-motor-XXXX
    MDNS.addService("http", "tcp", HTTP_CONFIG_PORT);

    // WS: try mDNS discovery, fall back to config
    String wsHost;
    uint16_t wsPort;
    if (discoverServer(wsHost, wsPort)) {
        wsClient.begin(wsHost.c_str(), wsPort, g_playerName,
                       MOTOR_MAX_LANES, g_motorColors, g_playerId);
    } else {
        wsClient.begin(g_serverIp, (uint16_t)atoi(g_serverPort), g_playerName,
                       MOTOR_MAX_LANES, g_motorColors, g_playerId);
    }
    wsClient.setLedMetadata(g_hasLedConfig ? g_savedLedConfig.ledCount : ledConfigDefaults().ledCount);

    // BT audio — non-blocking init
    btAudio.begin();

    // Sound manager — host/port for fetching WAV files from game server
    soundManager.begin(&btAudio, g_serverIp, (uint16_t)atoi(g_serverPort));

    // HTTP REST API
    setupHttpRoutes();
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

static bool s_wifiWasConnected = false;

void loop() {
    statusLed.loop();
    buttons.loop();
    motorManager.loop();
    matrixDisplay.loop();
    soundManager.loop();
    btAudio.loop();

    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    if (!wifiOk) {
        if (s_wifiWasConnected) {
            s_wifiWasConnected = false;
            wsClient.onWiFiLost();
            Serial.println("[WiFi] Lost");
        }
        statusLed.setState(LedState::NO_WIFI);
        return;
    }

    if (!s_wifiWasConnected) {
        s_wifiWasConnected = true;
        Serial.printf("[WiFi] Reconnected — IP: %s\n", WiFi.localIP().toString().c_str());
        // Re-discover server after reconnect in case IP changed
        String wsHost; uint16_t wsPort;
        if (discoverServer(wsHost, wsPort)) {
            wsClient.begin(wsHost.c_str(), wsPort, g_playerName,
                           MOTOR_MAX_LANES, g_motorColors, g_playerId);
        }
    }

    httpServer.handleClient();
    // MDNS.update() — not needed on ESP32; mDNS runs automatically in the background.

    if (g_pendingRestart) {
        if (g_stateDirty) saveState();
        delay(200);
        ESP.restart();
    }

    wsClient.loop();

    // ── Persist playerId when assigned ────────────────────────────────────────
    {
        const String& currentId = wsClient.getPlayerId();
        if (currentId.length() > 0 && strcmp(g_playerId, currentId.c_str()) != 0) {
            strlcpy(g_playerId, currentId.c_str(), sizeof(g_playerId));
            markStateDirty();
        }
    }

    // ── LED config hot-reload ─────────────────────────────────────────────────
    LedConfig pendingCfg;
    if (wsClient.pollLedConfig(pendingCfg)) {
        matrixDisplay.applyConfig(pendingCfg);
        wsClient.setLedMetadata(pendingCfg.ledCount);
        g_savedLedConfig = pendingCfg;
        g_hasLedConfig   = true;
        markStateDirty();
    }

    // ── LED test effect ───────────────────────────────────────────────────────
    LedTestEffectMessage pendingEffect;
    if (wsClient.pollTestEffect(pendingEffect)) {
        // Test effect: fill the matrix with the specified color
        if (strcmp(pendingEffect.effectName, "solid") == 0) {
            matrixDisplay.fillColor(pendingEffect.r, pendingEffect.g, pendingEffect.b);
        }
        // Other effects deferred to future enhancement
    }

    // ── Position updates from state broadcast ─────────────────────────────────
    {
        static PlayerPosition positions[WS_MAX_PLAYERS];
        uint8_t count = 0;
        if (wsClient.pollPositions(positions, count)) {
            // Build lane-indexed position array using colorIndex matching
            float lanePositions[MOTOR_MAX_LANES] = {};
            for (uint8_t p = 0; p < count; p++) {
                for (uint8_t lane = 0; lane < MOTOR_MAX_LANES; lane++) {
                    if (g_motorColors[lane] == positions[p].colorIndex) {
                        lanePositions[lane] = positions[p].position;
                        break;
                    }
                }
            }
            motorManager.setPositions(lanePositions, MOTOR_MAX_LANES);
        }
    }

    // ── Game-global events ────────────────────────────────────────────────────
    GlobalEventType gev = wsClient.pollGlobalEvent();
    bool winnerEvent = false;
    switch (gev) {
        case GlobalEventType::COUNTDOWN_TICK: {
            static int countdown = 3;
            matrixDisplay.showCountdown(countdown);
            soundManager.play(SoundEvent::COUNTDOWN_TICK);
            if (countdown > 0) countdown--;
            else countdown = 3;
            buttons.setGameStatus("running");
            break;
        }
        case GlobalEventType::GAME_STARTED:
            matrixDisplay.showCountdown(0);   // "GO"
            soundManager.play(SoundEvent::GAME_STARTED);
            buttons.setGameStatus("running");
            break;
        case GlobalEventType::GAME_PAUSED:
            buttons.setGameStatus("paused");
            matrixDisplay.showText("PAUSED", 255, 165, 0, 100);
            soundManager.play(SoundEvent::GAME_PAUSED);
            break;
        case GlobalEventType::GAME_RESUMED:
            buttons.setGameStatus("running");
            soundManager.play(SoundEvent::GAME_RESUMED);
            break;
        case GlobalEventType::GAME_RESET:
            motorManager.homeAll();
            matrixDisplay.showIdle();
            buttons.setGameStatus("idle");
            soundManager.play(SoundEvent::GAME_RESET);
            break;
        case GlobalEventType::WINNER_SELF:
        case GlobalEventType::WINNER_OTHER: {
            winnerEvent = true;
            soundManager.play(SoundEvent::WINNER);
            const char* name = wsClient.getPlayerId().c_str();
            matrixDisplay.showWinner(name);
            buttons.setGameStatus("idle");
            break;
        }
        default: break;
    }

    // ── Status LED reflects WS connection ─────────────────────────────────────
    statusLed.setState(wsClient.isConnected() ? LedState::WS_CONNECTED : LedState::WIFI_ONLY);

    flushStateIfNeeded();
}
