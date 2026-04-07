# Roll-a-Ball Derby — ESP8266 Sensor Client

PlatformIO firmware for the **Wemos D1 Mini** (ESP8266) sensor node. Reads two IR break-beam sensors (+1 and +3 scoring holes) and sends score events to the game server via WebSocket.

## Hardware

| Signal | D1 Mini pin | GPIO | Notes |
|--------|-------------|------|-------|
| +1 sensor | D1 | GPIO5 | Active-LOW (INPUT_PULLUP) |
| +3 sensor | D2 | GPIO4 | Active-LOW (INPUT_PULLUP) |
| Status LED | LED_BUILTIN | GPIO2 | Active-LOW (built-in) |

Connect the collector/output of each IR break-beam receiver to the pin, and GND to the sensor GND. The emitter side is powered from 3.3 V. Both sensor pins are configured as `INPUT_PULLUP` so **no external pull-up resistors are required** — the ESP8266 internal pull-ups (~47 kΩ) hold the line HIGH; the sensor pulls it LOW when the beam is broken.

## Requirements

- [PlatformIO](https://platformio.org/) CLI or the VSCode extension
- Wemos D1 Mini (ESP8266, 4 MB flash)

## Build & Flash

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
| Server IP | LAN IP of the game server | `192.168.4.1` |
| Server Port | Port the server listens on | `3000` |
| Player Name | Optional label for this player lane | _(empty — server assigns one)_ |

Save the settings; the device connects to your network and saves the config to LittleFS. Subsequent boots auto-connect without opening the portal.

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
`points` is `1` (D1 sensor) or `3` (D2 sensor).

## Reconnect Behaviour

WebSocket reconnects with exponential backoff: 1 s → 2 s → 4 s → 8 s → 16 s → 30 s (max). The stored `playerId` is included in the re-registration payload so the server can restore the existing player entry without creating a duplicate.
