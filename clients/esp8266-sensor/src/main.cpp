#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
using DerbyWebServer = ESP8266WebServer;
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
using DerbyWebServer = WebServer;
#else
#error "Unsupported board: define ESP8266 or ESP32 target"
#endif
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "websocket.h"
#include "sensors.h"
#include "led.h"
#include "status_led.h"
#include <device_info.h>

// ─── Global Instances ─────────────────────────────────────────────────────────
static WSClient        wsClient;
static Sensors         sensors;
static LedManager      ledManager;
static StatusLed       statusLed;
static DerbyWebServer  httpServer(HTTP_CONFIG_PORT);

// Flag set by the /config handler so the reboot happens after the HTTP response
// has been flushed to the client.
static bool g_pendingRestart = false;

// ─── Runtime Config (loaded from LittleFS, populated by WiFiManager) ──────────
// Global state: embedded firmware convention — no DI framework on-target.
// Mutable state is intentionally module-level; functions operate on these globals directly.
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
        DERBY_LOG_LN("[CFG] No config file — using defaults");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) {
        DERBY_LOG_LN("[CFG] Failed to open config file");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        DERBY_LOG_F("[CFG] Parse error (%s) — using defaults\n", err.c_str());
        return;
    }

    if (doc["server_ip"  ].is<const char*>()) strlcpy(g_serverIp,   doc["server_ip"],   sizeof(g_serverIp));
    if (doc["server_port"].is<const char*>()) strlcpy(g_serverPort,  doc["server_port"], sizeof(g_serverPort));
    if (doc["player_name"].is<const char*>()) strlcpy(g_playerName,  doc["player_name"], sizeof(g_playerName));

    DERBY_LOG_F("[CFG] Loaded: ip=%s port=%s name=%s\n",
                  g_serverIp, g_serverPort, g_playerName);
}

static void saveConfig() {
    JsonDocument doc;
    doc["server_ip"  ] = g_serverIp;
    doc["server_port"] = g_serverPort;
    doc["player_name"] = g_playerName;

    File f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) {
        DERBY_LOG_LN("[CFG] Failed to write config file");
        return;
    }

    serializeJson(doc, f);
    f.close();
    DERBY_LOG_LN("[CFG] Config saved");
}

// ─── Runtime State Persistence ────────────────────────────────────────────────
// Persists server-assigned state (playerId, LED config, device color) to a
// separate file so the device boots with the correct identity and LED setup.
// Uses atomic write (temp → rename) to prevent corruption on power loss.

static bool isValidStateFile(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    return !err;
}



static void loadState() {
    const bool hasState = LittleFS.exists(STATE_FILE);
    const bool hasTemp  = LittleFS.exists(STATE_TMP);

    if (hasTemp) {
        if (!hasState) {
            DERBY_LOG_LN("[STATE] Recovering from interrupted write (temp → state)");
            LittleFS.rename(STATE_TMP, STATE_FILE);
        } else {
            const bool stateValid = isValidStateFile(STATE_FILE);
            const bool tempValid  = isValidStateFile(STATE_TMP);

            if (!stateValid && tempValid) {
                DERBY_LOG_LN("[STATE] Main state invalid; recovering from temp file");
                LittleFS.remove(STATE_FILE);
                LittleFS.rename(STATE_TMP, STATE_FILE);
            } else {
                DERBY_LOG_LN("[STATE] Removing stale temp state file");
                LittleFS.remove(STATE_TMP);
            }
        }
    }

    if (!LittleFS.exists(STATE_FILE)) {
        DERBY_LOG_LN("[STATE] No state file — using defaults");
        return;
    }

    File f = LittleFS.open(STATE_FILE, "r");
    if (!f) {
        DERBY_LOG_LN("[STATE] Failed to open state file");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        DERBY_LOG_F("[STATE] Parse error (%s) — using defaults\n", err.c_str());
        return;
    }

    if (doc["player_id"].is<const char*>()) {
        strlcpy(g_playerId, doc["player_id"], sizeof(g_playerId));
    }

    if (doc["led_count"].is<int>()) {
        const int count = doc["led_count"].as<int>();
        const int pin   = doc["led_pin"]  | static_cast<int>(LED_DEFAULT_PIN);
        const int bri   = doc["led_brightness"] | static_cast<int>(LED_DEFAULT_BRIGHTNESS);

        // Validate / clamp to safe ranges — corrupted state must not brick LEDs.
        const bool isCountValid      = (count >= 1 && count <= LED_MAX_COUNT);
        const bool isPinValid        = ledPinIsValid(pin);  // defined in LedPlatform.h (via config.h)
        const bool isBrightnessValid = (bri >= 0 && bri <= 255);
        if (!isCountValid || !isPinValid || !isBrightnessValid) {
            DERBY_LOG_LN("[STATE] LED config out of range — ignoring saved values");
        } else {
            g_savedLedConfig.ledCount   = static_cast<uint16_t>(count);
            g_savedLedConfig.pin        = static_cast<uint8_t>(pin);
            g_savedLedConfig.brightness = static_cast<uint8_t>(bri);
            g_savedLedConfig.topology   = LedTopology::STRIP;
            g_savedLedConfig.matrixRows = 8;
            g_savedLedConfig.matrixCols = 8;

            const char* topo = doc["led_topology"] | "strip";
            if      (strcmp(topo, "ring")               == 0) g_savedLedConfig.topology = LedTopology::RING;
            else if (strcmp(topo, "matrix_zigzag")      == 0) g_savedLedConfig.topology = LedTopology::MATRIX_ZIGZAG;
            else if (strcmp(topo, "matrix_progressive") == 0) g_savedLedConfig.topology = LedTopology::MATRIX_PROGRESSIVE;

            g_hasLedConfig = true;
        }
    }

    if (doc["device_color_r"].is<int>()) {
        const int r = doc["device_color_r"] | 0;
        const int g = doc["device_color_g"] | 0;
        const int b = doc["device_color_b"] | 0;

        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            g_savedLedConfig.deviceColorR   = static_cast<uint8_t>(r);
            g_savedLedConfig.deviceColorG   = static_cast<uint8_t>(g);
            g_savedLedConfig.deviceColorB   = static_cast<uint8_t>(b);
            g_savedLedConfig.hasDeviceColor = true;
        } else {
            DERBY_LOG_LN("[STATE] Device color out of range — ignoring");
        }
    }

    DERBY_LOG_F("[STATE] Loaded: playerId=%s ledCount=%u hasColor=%d\n",
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
        DERBY_LOG_LN("[STATE] Failed to write temp state file");
        return;
    }
    serializeJson(doc, f);
    f.close();

    // Remove old state file (ignore failure — it may not exist on first write).
    LittleFS.remove(STATE_FILE);
    if (!LittleFS.rename(STATE_TMP, STATE_FILE)) {
        DERBY_LOG_LN("[STATE] WARNING: rename failed — temp file remains");
        return;
    }

    g_stateDirty     = false;
    g_stateLastSave  = millis();
    DERBY_LOG_LN("[STATE] State saved");
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

// ─── mDNS Server Discovery ────────────────────────────────────────────────────
// Queries the LAN for _derby._tcp to auto-discover the game server.
// Returns true and fills host/port on success; false if no service found.

static bool discoverServer(String& host, uint16_t& port) {
    // Build a unique mDNS hostname: "derby-sensor-XXXX"
    char mdnsName[32];
    snprintf(mdnsName, sizeof(mdnsName), "derby-sensor-%04x", derbyChipSuffix16());

    if (!MDNS.begin(mdnsName)) {
        DERBY_LOG_LN("[mDNS] Failed to start responder");
        return false;
    }

    DERBY_LOG_LN("[mDNS] Querying for _derby._tcp ...");
    int n = MDNS.queryService("derby", "tcp");
    if (n <= 0) {
        DERBY_LOG_LN("[mDNS] No server found — falling back to config");
        return false;
    }

    // Pick the first result on the same subnet as the ESP32's WiFi interface.
    // Windows hosts can advertise mDNS from multiple adapters (e.g. WSL2 bridge),
    // so we must reject IPs that are unreachable from the sensor's subnet.
    IPAddress localIp = WiFi.localIP();
    IPAddress mask    = WiFi.subnetMask();

    // On ESP32, subnetMask() can return 0.0.0.0 immediately after connect while
    // DHCP is still settling. Fall back to /24 — covers virtually all home/office
    // networks and is enough to distinguish 192.168.x.x from 172.x.x.x.
    if ((uint32_t)mask == 0) {
        mask = IPAddress(255, 255, 255, 0);
        DERBY_LOG_F("[mDNS] Subnet mask not ready — assuming /24 (local IP: %s)\n",
                      localIp.toString().c_str());
    }

    uint32_t myNet = (uint32_t)localIp & (uint32_t)mask;
    DERBY_LOG_F("[mDNS] Local: %s  mask: %s  net: %d.%d.%d.%d  candidates: %d\n",
                  localIp.toString().c_str(), mask.toString().c_str(),
                  (myNet >> 24) & 0xFF, (myNet >> 16) & 0xFF,
                  (myNet >> 8) & 0xFF, myNet & 0xFF, n);

    for (int i = 0; i < n; i++) {
        IPAddress candidate = MDNS.IP(i);
        uint32_t candNet    = (uint32_t)candidate & (uint32_t)mask;
        DERBY_LOG_F("[mDNS] Candidate[%d]: %s  net match: %s\n",
                      i, candidate.toString().c_str(),
                      candNet == myNet ? "YES" : "NO");
        if (candNet == myNet) {
            host = candidate.toString();
            port = MDNS.port(i);
            DERBY_LOG_F("[mDNS] Selected server at %s:%u\n", host.c_str(), port);
            return true;
        }
    }

    DERBY_LOG_LN("[mDNS] No same-subnet server found — falling back to config");
    return false;
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
        DERBY_LOG_F("[CFG] Remote update — ip=%s port=%s name=%s — rebooting\n",
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
    DERBY_LOG_LN("\n[BOOT] Roll-a-Ball Derby — Sensor Client");

    sensors.begin();

    // Mount LittleFS; format if mount fails (first boot or corruption).
    if (!LittleFS.begin()) {
        DERBY_LOG_LN("[CFG] LittleFS mount failed — formatting...");
        LittleFS.format();
        if (!LittleFS.begin()) {
            DERBY_LOG_LN("[CFG] LittleFS mount failed after format — continuing without persistent config");
        }
    }
    loadConfig();
    loadState();

    // Initialise LEDs with saved config (from previous server session) or defaults.
    if (g_hasLedConfig) {
        ledManager.begin(g_savedLedConfig);
        wsClient.setLedMetadata(g_savedLedConfig.ledCount);
        statusLed.begin(g_savedLedConfig.pin);
        DERBY_LOG_LN("[BOOT] Using saved LED config from previous session");
    } else {
        ledManager.begin(ledConfigDefaults());
        wsClient.setLedMetadata(LED_DEFAULT_COUNT);
        statusLed.begin(LED_DEFAULT_PIN);
    }

    // ─── Serial Pre-Configure Window ──────────────────────────────────────────
    // The web flasher sends DERBY_CFG:{json}\n within 3 s of boot to pre-populate
    // server IP/port/name without requiring the WiFiManager captive portal.
    // The settings are saved to LittleFS exactly like the WiFiManager would; on
    // the next step WiFiManager picks them up as pre-filled defaults.
    DERBY_LOG_F("[CFG] Serial pre-config window 3 s — send: DERBY_CFG:{...}\n");
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
                            DERBY_LOG_F("[CFG] DERBY_CFG_ACK:OK\n");
                            done = true;
                        } else {
                            DERBY_LOG_F("[CFG] DERBY_CFG_ACK:ERR_JSON %s\n", err.c_str());
                        }
                    }
                    buf = "";
                } else if (buf.length() < 256) {
                    buf += c;
                }
            }
            if (!done) delay(1);   // yield to background tasks
        }
        if (!done) DERBY_LOG_F("[CFG] Serial config window expired\n");
    }

    // Build unique AP name: "Derby-Sensor-XXXX" using last 4 hex digits of chip ID.
    char apName[32];
    snprintf(apName, sizeof(apName), "%s%04X",
             WIFIMANAGER_AP_PREFIX, derbyChipSuffix16());

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

    DERBY_LOG_F("[WiFi] Auto-connecting (AP: %s if needed)\n", apName);
    if (!wifiManager.autoConnect(apName)) {
        // autoConnect() returns false on timeout; reboot and try again.
        DERBY_LOG_LN("[WiFi] autoConnect timed out — rebooting");
        ESP.restart();
    }

    DERBY_LOG_F("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    ledManager.setState(LedState::WIFI_ONLY);

    // Try mDNS autodiscovery first; fall back to stored config.
    String discoveredHost;
    uint16_t discoveredPort;
    if (discoverServer(discoveredHost, discoveredPort)) {
        wsClient.begin(discoveredHost.c_str(), discoveredPort, g_playerName, g_playerId);
        DerbyLogger::setSender(&wsClient);
    } else {
        uint16_t port = static_cast<uint16_t>(atoi(g_serverPort));
        wsClient.begin(g_serverIp, port, g_playerName, g_playerId);
        DerbyLogger::setSender(&wsClient);
    }

    // HTTP config server — lets the Node.js admin push new config without
    // needing to re-open the WiFiManager captive portal.
    httpServer.on("/config", handleHttpConfig);
    httpServer.begin();
    DERBY_LOG_F("[HTTP] Config server listening on port %d\n", HTTP_CONFIG_PORT);
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

static bool s_wifiWasConnected = false;

void loop() {
    ledManager.loop();
    statusLed.loop();

    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    if (!wifiOk) {
        if (s_wifiWasConnected) {
            s_wifiWasConnected = false;
            wsClient.onWiFiLost();
            DERBY_LOG_LN("[WiFi] Connection lost");
        }
        ledManager.setState(LedState::NO_WIFI);
        return;
    }

    if (!s_wifiWasConnected) {
        s_wifiWasConnected = true;
        DERBY_LOG_F("[WiFi] Reconnected — IP: %s\n", WiFi.localIP().toString().c_str());

        // Re-attempt mDNS discovery after WiFi reconnect in case server IP changed.
        String rediscoveredHost;
        uint16_t rediscoveredPort;
        if (discoverServer(rediscoveredHost, rediscoveredPort)) {
            wsClient.begin(rediscoveredHost.c_str(), rediscoveredPort, g_playerName, g_playerId);
            DerbyLogger::setSender(&wsClient);
        }
    }

#if defined(ESP8266)
    MDNS.update();
#endif
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
            DERBY_LOG_F("[STATE] playerId changed: %s\n", g_playerId);
        }
    }

    // ── LED config hot-reload ────────────────────────────────────────────────
    LedConfig pendingCfg;
    if (wsClient.pollLedConfig(pendingCfg)) {
        ledManager.applyConfig(pendingCfg);
        wsClient.setLedMetadata(pendingCfg.ledCount);
        statusLed.setStripPin(pendingCfg.pin);
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

    // ── Stop test effect ─────────────────────────────────────────────────────
    if (wsClient.pollStopEffect()) {
        ledManager.restoreAmbient();
    }

    int points          = sensors.check();
    LocalEventType  lev = wsClient.pollLocalEvent();
    GlobalEventType gev = wsClient.pollGlobalEvent();

    if (wsClient.isConnected()) {
        ledManager.setState(LedState::WS_CONNECTED);

        // Global events: all devices react (countdown, lifecycle, winner).
        bool winnerEvent = false;
        switch (gev) {
            case GlobalEventType::COUNTDOWN_TICK: ledManager.onGlobalEvent(gev); statusLed.blink(600); break;
            case GlobalEventType::GAME_STARTED:   ledManager.onGlobalEvent(gev); statusLed.blink(500); break;
            case GlobalEventType::GAME_PAUSED:    ledManager.onGlobalEvent(gev); break;
            case GlobalEventType::GAME_RESUMED:   ledManager.onGlobalEvent(gev); statusLed.blink(300); break;
            case GlobalEventType::GAME_RESET:     ledManager.onGlobalEvent(gev); break;
            case GlobalEventType::WINNER_SELF:    ledManager.onGlobalEvent(gev); statusLed.blink(1000); winnerEvent = true; break;
            case GlobalEventType::WINNER_OTHER:   ledManager.onGlobalEvent(gev); winnerEvent = true; break;
            default: break;
        }

        // Device-local events: only the owning device reacts (scoring, rank, streaks).
        // Skip when a winner event fired — the winning score arrives in the same
        // WS batch and would immediately overwrite the rainbow/pulse effect.
        if (winnerEvent) lev = LocalEventType::NONE;
        switch (lev) {
            case LocalEventType::SCORE_PLUS1:   ledManager.onLocalEvent(lev); statusLed.blink(200); break;
            case LocalEventType::SCORE_PLUS2:   ledManager.onLocalEvent(lev); statusLed.blink(200); break;
            case LocalEventType::SCORE_PLUS3:   ledManager.onLocalEvent(lev); statusLed.blink(300); break;
            case LocalEventType::ZERO_ROLL:     ledManager.onLocalEvent(lev); break;
            case LocalEventType::TOOK_LEAD:     ledManager.onLocalEvent(lev); statusLed.blink(400); break;
            case LocalEventType::BECAME_LAST:   ledManager.onLocalEvent(lev); break;
            case LocalEventType::STREAK_ZERO:   ledManager.onLocalEvent(lev); break;
            case LocalEventType::STREAK_THREE:  ledManager.onLocalEvent(lev); statusLed.blink(400); break;
            default: break;
        }

        // Only send score events once a playerId has been assigned by the server.
        if (!wsClient.getPlayerId().isEmpty()) {
            if (points > 0) {
                DERBY_LOG_F("[SENSOR] Triggered: +%d\n", points);
                wsClient.sendScore(points);
            }
        } else if (points > 0) {
            DERBY_LOG_F("[SENSOR] Dropped trigger while waiting for player assignment: +%d\n", points);
        }
    } else {
        ledManager.setState(LedState::WIFI_ONLY);
        if (points > 0) {
            DERBY_LOG_F("[SENSOR] Dropped offline trigger: +%d\n", points);
        }
    }

    // ── Debounced state flush to LittleFS ────────────────────────────────────
    flushStateIfNeeded();
}
