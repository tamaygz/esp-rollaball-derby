# LED Configuration Flow

Server-side LED configuration management with REST API and WebSocket synchronization.

## Architecture Overview

```
┌─────────────────┐         ┌──────────────────┐         ┌────────────────┐
│  Web Admin UI   │◄───────►│   Node.js Server │◄───────►│  ESP8266/32    │
│  (Browser)      │  REST   │  (Central Hub)   │  WS     │  (Sensor/Motor)│
└─────────────────┘         └──────────────────┘         └────────────────┘
                                     │
                                     │ File I/O
                                     ▼
                           ┌──────────────────┐
                           │ led-config.json  │
                           │ (Persistent)     │
                           └──────────────────┘
```

## Components

### 1. LedConfigManager (`server/src/config/LedConfigManager.js`)
- **Purpose**: Central configuration persistence and validation
- **Storage**: `server/data/led-config.json`
- **Pattern**: EventEmitter for change notifications
- **Key Methods**:
  - `loadConfig()` — Load from disk with fallback to defaults
  - `saveConfig(config)` — Atomic write with temp file technique
  - `getConfigForDeviceType(type)` — Retrieve device-specific config
  - `updateDeviceConfig(type, partial)` — Merge and validate update
  - `validateDeviceLedCount(type, reported, chipType)` — Check LED count match
- **Events**: `configChanged(deviceType, config)` when configuration updates

### 2. LED Routes (`server/src/routes/leds.js`)
- **Purpose**: REST API for configuration management
- **Endpoints**:
  - `GET /api/leds/config` — All configurations
  - `GET /api/leds/config/:deviceType` — Specific type (sensor/motor/display)
  - `PUT /api/leds/config/:deviceType` — Update config (triggers broadcast)
  - `POST /api/leds/effects/test` — Test effect on device (rate-limited 1/sec)
- **Validation**: LED count (0-1000), brightness (0-255), topology enum, effect names
- **Rate Limiting**: Per-device throttling on test endpoint

### 3. ConnectionManager (`server/src/ws/ConnectionManager.js`)
- **Purpose**: WebSocket message routing and broadcasting
- **Extensions**:
  - `broadcastLedConfig(deviceType, config)` — Send to all devices of type
  - `sendTestEffect(deviceId, effect, params)` — Send to specific device
  - `getDeviceById(deviceId)` — Lookup connected device
- **Registration**: Extracts `ledCount` and `chipType` from register messages
- **Validation**: Auto-validates reported LED count against config on registration

### 4. led-config.json (`server/data/led-config.json`)
- **Purpose**: Persistent storage for device configurations
- **Format**: JSON with per-device-type configs
- **Defaults**: sensor (10 LEDs, rainbow), motor (10 LEDs, chase), display (0 LEDs)
- **Human-Editable**: Can be manually edited (server reloads on startup)

## Flow Diagrams

### Device Registration Flow

```
┌────────────┐                ┌──────────────┐                ┌────────────┐
│  ESP8266   │                │    Server    │                │   Config   │
└─────┬──────┘                └──────┬───────┘                └─────┬──────┘
      │                              │                              │
      │  register                    │                              │
      │  {type:"sensor",             │                              │
      │   ledCount:10,               │                              │
      │   chipType:"ESP8266"}        │                              │
      ├─────────────────────────────►│                              │
      │                              │ getConfigForDeviceType       │
      │                              ├─────────────────────────────►│
      │                              │◄─────────────────────────────┤
      │                              │ {ledCount:10, brightness:255}│
      │                              │                              │
      │                              │ validateDeviceLedCount       │
      │                              │  (10 vs 10) ✓ PASS           │
      │                              │                              │
      │  registered                  │                              │
      │  {id:"abc", name:"Sensor 1"} │                              │
      │◄─────────────────────────────┤                              │
      │                              │                              │
      │  led_config                  │                              │
      │  {ledCount:10,               │                              │
      │   topology:"strip",          │                              │
      │   brightness:255,            │                              │
      │   defaultEffect:"rainbow"}   │                              │
      │◄─────────────────────────────┤                              │
      │                              │                              │
```

### Configuration Update Flow (REST API)

```
┌────────────┐                ┌──────────────┐                ┌────────────┐
│  Web Admin │                │    Server    │                │ ESP8266 x3 │
└─────┬──────┘                └──────┬───────┘                └─────┬──────┘
      │                              │                              │
      │  PUT /api/leds/config/sensor │                              │
      │  {ledCount:20,               │                              │
      │   defaultEffect:"pulse"}     │                              │
      ├─────────────────────────────►│                              │
      │                              │ updateDeviceConfig           │
      │                              │  (merge + validate)          │
      │                              │                              │
      │                              │ saveConfig (atomic write)    │
      │                              │                              │
      │                              │ emit('configChanged')        │
      │                              │──┐                            │
      │                              │  │ event                     │
      │                              │◄─┘                            │
      │                              │                              │
      │                              │ broadcastLedConfig('sensor') │
      │                              ├──────────────────────────────►
      │                              │  led_config (broadcast x3)   │
      │  200 OK                      │                              │
      │  {success:true}              │                              │
      │◄─────────────────────────────┤                              │
      │                              │                              │
```

### Effect Test Flow (Rate Limited)

```
┌────────────┐                ┌──────────────┐                ┌────────────┐
│  Web Admin │                │    Server    │                │  ESP8266   │
└─────┬──────┘                └──────┬───────┘                └─────┬──────┘
      │                              │                              │
      │  POST /api/leds/effects/test │                              │
      │  {deviceId:"abc",            │                              │
      │   effectName:"rainbow",      │                              │
      │   params:{duration:3000}}    │                              │
      ├─────────────────────────────►│                              │
      │                              │ rate limit check             │
      │                              │  (1 req/sec per device) ✓    │
      │                              │                              │
      │                              │ getDeviceById("abc")         │
      │                              │                              │
      │                              │ sendTestEffect               │
      │                              ├──────────────────────────────►
      │                              │  test_effect                 │
      │                              │  {effectName:"rainbow",      │
      │                              │   params:{duration:3000}}    │
      │  200 OK                      │                              │
      │  {success:true}              │                              │
      │◄─────────────────────────────┤                              │
      │                              │                              │
      │  POST (2nd request < 1s)     │                              │
      ├─────────────────────────────►│                              │
      │                              │ rate limit exceeded ✗        │
      │  429 Too Many Requests       │                              │
      │  {error:"Rate limit..."}     │                              │
      │◄─────────────────────────────┤                              │
      │                              │                              │
```

## WebSocket Protocol

### Device → Server

**Register** (with LED metadata):
```json
{
  "type": "register",
  "payload": {
    "type": "sensor",
    "playerName": "ESP-001",
    "ledCount": 10,
    "chipType": "ESP8266"
  }
}
```

### Server → Device

**Registration Confirmation** (with validation warning):
```json
{
  "type": "registered",
  "payload": {
    "id": "abc123",
    "name": "Sensor 1",
    "playerType": "sensor",
    "warning": "Configured LED count (20) does not match reported count (10). Update config or check wiring."
  }
}
```

**LED Configuration**:
```json
{
  "type": "led_config",
  "timestamp": 1704123456789,
  "payload": {
    "ledCount": 10,
    "topology": "strip",
    "gpioPin": 4,
    "brightness": 255,
    "defaultEffect": "rainbow"
  }
}
```

**Test Effect**:
```json
{
  "type": "test_effect",
  "payload": {
    "effectName": "rainbow",
    "params": {
      "duration": 3000
    }
  }
}
```

## Validation Rules

### LED Count Validation
- **Range**: 0–1000
- **Platform Limits**:
  - ESP8266: Max 300 LEDs (SRAM constraints)
  - ESP32: Max 1000 LEDs (more SRAM available)
- **Tolerance**: ±5 LEDs for mismatch detection (allows for manufacturing variance)
- **Action**: Server logs warning, includes in `registered` response

### Topology Validation
- **Valid Values**: `"strip"`, `"ring"`, `"matrix"`
- **Default**: `"strip"`

### Brightness Validation
- **Range**: 0–255 (8-bit PWM)
- **Default**: 255 (100%)

### Effect Validation
- **Valid Effects**: `solid`, `blink`, `pulse`, `rainbow`, `chase`, `sparkle`
- **Context**: Validated in REST API and test effect endpoint

## Error Handling

### REST API Errors
- **400 Bad Request**: Invalid field values, missing required fields
- **404 Not Found**: Device type not found, device not connected
- **429 Too Many Requests**: Test effect rate limit exceeded (1/sec per device)
- **500 Internal Server Error**: Config save failure, unexpected errors

### WebSocket Errors
- **Registration Error**: Invalid device type, malformed payload
- **Broadcast Failure**: Logged to console, does not disconnect device

## File Persistence

### Atomic Write Strategy
1. Write new config to temporary file: `led-config.json.tmp`
2. If write succeeds, rename temp file to `led-config.json` (atomic operation)
3. If write fails, temp file is discarded, original config remains intact

**Benefits**:
- No config corruption on power loss during write
- Original config always readable until write completes
- OS-level atomicity guarantees

### Default Fallback
If `led-config.json` is missing or corrupted:
1. LedConfigManager loads embedded default configuration
2. Server logs warning: `[LedConfigManager] Config file not found, using defaults`
3. Default config written to disk on first manual update

## Performance Characteristics

### Broadcast Performance
- **Target**: <500ms for LED config broadcast
- **Logging**: `[ConnectionManager] LED config broadcast: 3 sensor device(s) updated in 12ms`
- **Filtered Broadcast**: Only devices matching `deviceType` receive message

### Rate Limiting
- **Implementation**: `express-rate-limit` middleware
- **Strategy**: Per-device ID (extracted from request body)
- **Limit**: 1 request/second per device
- **Window**: Rolling 1-second window

## Security Considerations

1. **Input Validation**: All user-supplied values validated before persistence
2. **XSS Prevention**: Device names sanitized (HTML tag removal, 20 char limit)
3. **Rate Limiting**: Prevents DoS via rapid test effect requests
4. **No Authentication**: LAN-only deployment, no internet exposure assumed
5. **File Permissions**: Config file readable/writable by server process only

## Testing Strategy

### Unit Tests
- LedConfigManager: load, save, validate, update
- LED Routes: endpoint validation, error handling
- ConnectionManager: broadcast filtering, device lookup

### Integration Tests
1. Register device with LED metadata → verify config auto-sent
2. Update config via REST → verify broadcast to all matching devices
3. Test effect rate limiting → verify 429 response on exceed
4. LED count mismatch → verify warning in registration response
5. Platform limit validation → verify ESP8266 300 LED cap

### Manual Testing
1. Edit `led-config.json` manually → restart server → verify loaded
2. Corrupt config file → verify default fallback
3. Update config via admin UI → verify devices reflect change
4. Fast-click test effect button → verify rate limit blocks

## Future Enhancements

- **Configuration History**: Track config changes with timestamps and author
- **Multi-GPIO Support**: Configure multiple LED strips per device
- **Effect Presets**: Predefined effect combinations for game events
- **Dynamic Brightness**: Auto-adjust brightness based on game state
- **Firmware Version Check**: Validate device firmware supports configured features
