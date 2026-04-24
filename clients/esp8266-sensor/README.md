# Roll-a-Ball Derby — Sensor Client (ESP8266 + ESP32)

PlatformIO firmware for sensor nodes on **ESP8266** and **ESP32** boards. Reads three IR break-beam sensors (+1, +2, and +3 scoring holes) and sends score events to the game server via WebSocket.

## Hardware

| Signal | ESP8266 (D1 Mini/NodeMCU) | ESP32 DevKit | Notes |
|--------|----------------------------|--------------|-------|
| +1 sensor | GPIO5 (D1) | GPIO25 | Active-LOW (INPUT_PULLUP) |
| +2 sensor | GPIO14 (D5) | GPIO26 | Active-LOW (INPUT_PULLUP) |
| +3 sensor | GPIO4 (D2) | GPIO27 | Active-LOW (INPUT_PULLUP) |
| LED strip data | GPIO2 (D4) | GPIO4 | WS2812B data pin |

Connect the collector/output of each IR break-beam receiver to the pin, and GND to the sensor GND. The emitter side is powered from 3.3 V. All three sensor pins are configured as `INPUT_PULLUP` so **no external pull-up resistors are required** — the board's internal pull-ups hold the line HIGH; the sensor pulls it LOW when the beam is broken.

Each scoring hole has its own sensor input: breaking the `+1`, `+2`, or `+3` beam causes the firmware to report the corresponding `points` value (`1`, `2`, or `3`) to the server.
## Requirements

- Wemos D1 Mini (ESP8266, 4 MB flash), NodeMCU v2/v3 (ESP8266), or ESP32 DevKit (esp32dev)
- USB cable

## ⚡ Flash from Your Browser (No Toolchain Needed)

The easiest way to flash the firmware — works on macOS, Windows, and Linux without installing PlatformIO or any C++ tools. Requires **Chrome or Edge 89+** (Web Serial API).

1. Start the game server (`npm start` in `server/`).
2. Open the flashing page in Chrome or Edge:
   - If the browser is running on the **same computer** as the server, use **`http://localhost:3000/flash-sensor`**.
   - If you are opening it from **another device** on your LAN, serve the game server over **HTTPS** and open **`https://<server-host>/flash-sensor`** instead.
3. Plug the sensor board (ESP8266 or ESP32 DevKit) into a USB port on the computer running the browser.
4. Click **Install Derby Sensor Firmware** and select the serial port.
5. Wait ~30 s — the latest pre-built binary is downloaded from [GitHub Releases](https://github.com/tamaygz/esp-rollaball-derby/releases/latest) and flashed automatically.

> **Web Serial secure-context requirement:** Web Serial only works over **`https://`** or **`http://localhost`**. Opening `http://<LAN-IP>:3000/flash-sensor` on a different machine will show an "insecure context" warning and the Install button will be disabled.

> **Safari / Firefox are not supported** — Web Serial is only available in Chromium-based browsers.

> **Forking this repo?** Update the firmware URLs in `web-install/manifest*.json` to point to your own repository's GitHub Releases.

## Build & Flash with PlatformIO (Advanced)

For development or building from source, [PlatformIO](https://platformio.org/) CLI or VSCode extension is required.

```bash
cd clients/esp8266-sensor

# Compile
pio run

# Flash firmware
pio run -t upload

# Upload filesystem (LittleFS) — only needed on first flash or after format
pio run -t uploadfs

# Open serial monitor (115200 baud)
pio device monitor
```

## First-Boot WiFi Setup

On first boot (no saved credentials) the device opens a WiFi access point named **`Derby-Sensor-XXXX`** (XXXX = last 4 hex digits of the chip ID). Connect a phone or laptop to that AP and navigate to `192.168.4.1`. Fill in:

| Field | Description | Default |
|-------|-------------|---------|
| Server IP | LAN IP of the game server | `192.168.1.200` |
| Server Port | Port the server listens on | `3000` |
| Player Name | Optional label for this player lane | _(empty — server assigns one)_ |

Save the settings; the device connects to your network and saves the config to LittleFS. Subsequent boots auto-connect without opening the portal.
### mDNS Autodiscovery

After connecting to WiFi, the sensor queries `_derby._tcp.local` to auto-discover the game server’s IP and port. If found, the manually configured Server IP/Port are bypassed. If no server is advertising via mDNS, the stored config values are used as fallback.

The sensor also registers itself on mDNS as **`derby-sensor-XXXX.local`**, making it reachable by hostname on the LAN.

On WiFi reconnect (e.g. after dropout), autodiscovery is re-attempted so the sensor can follow a server that changed IP.
## Status LED

| Pattern | Meaning |
|---------|---------|
| Fast blink (5 Hz) | No WiFi |
| Slow blink (1 Hz) | WiFi connected, WebSocket disconnected |
| Solid on | WebSocket connected and registered |

## Source Files

| File | Purpose |
|------|---------|
| `platformio.ini` | Board, framework, library dependencies |
| `src/config.h` | Pin definitions, timing constants |
| `src/main.cpp` | `setup()` / `loop()`, WiFiManager, LittleFS config |
| `src/websocket.h/.cpp` | WebSocket client: connect, register, score, reconnect |
| `src/sensors.h/.cpp` | IR sensor ISRs + 500 ms debounce |
| `src/led.h/.cpp` | Non-blocking LED state machine |

## Dependencies (managed by PlatformIO)

| Library | Version | Purpose |
|---------|---------|---------|
| `gilmaimon/ArduinoWebsockets` | ^0.5.4 | WebSocket client (RFC-6455) |
| `tzapu/WiFiManager` | ^2.0.17 | WiFi config captive portal |
| `bblanchon/ArduinoJson` | ^7.0.0 | JSON serialisation |
| `ESP8266mDNS` / `ESPmDNS` | _(built-in)_ | mDNS responder + DNS-SD service discovery |
## WebSocket Protocol

Connects to `ws://<server_ip>:<server_port>/`.

**On connect** — sends register:
```json
{ "type": "register", "payload": { "type": "sensor", "playerName": "Alice" } }
```

**Server responds** with `registered`:
```json
{ "type": "registered", "payload": { "id": "<uuid>", "name": "Alice", "playerType": "sensor" } }
```
The `id` is stored as `playerId` for subsequent score messages.

**On sensor trigger** — sends score:
```json
{ "type": "score", "payload": { "playerId": "<uuid>", "points": 1 } }
```
`points` is `1` (D1 / +1 sensor), `2` (D2 / +2 sensor), or `3` (D5 / +3 sensor).

## Reconnect Behaviour

WebSocket reconnects with exponential backoff: 1 s → 2 s → 4 s → 8 s → 16 s → 30 s (max). The stored `playerId` is included in the re-registration payload so the server can restore the existing player entry without creating a duplicate.
