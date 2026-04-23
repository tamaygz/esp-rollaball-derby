# Roll-a-Ball Derby — Sensor Node Hardware Guide (ESP8266 + ESP32)

## Overview

The sensor node detects balls passing through scoring holes using IR photodiodes and transmits score events to the game server via WebSocket. This document covers hardware requirements, wiring diagrams, and pin configurations for supported ESP8266 and ESP32 board variants.

## Supported Board Configurations

### Wemos D1 Mini
- **Chip**: ESP8266EX (ESP-12F module)
- **Flash**: 4MB
- **USB**: CP2104 USB-to-Serial (921600 baud support)
- **Form Factor**: Compact breadboard-friendly
- **PlatformIO Board ID**: `d1_mini`

### NodeMCU v2/v3 (DevKit 1.0)
- **Chip**: ESP8266EX (ESP-12E module) 
- **Flash**: 4MB
- **USB**: CH340 USB-to-Serial (115200 baud max)
- **Form Factor**: Larger with onboard voltage regulator
- **PlatformIO Board ID**: `nodemcuv2`
- **Notes**: v3 hardware identical to v2, different USB chip only

### ESP32 DevKit (ESP32-WROOM)
- **Chip**: ESP32 dual-core
- **Flash**: 4MB (typical)
- **USB**: CP210x/CH340 (varies by board vendor)
- **PlatformIO Board ID**: `esp32dev`

## Pin Configuration Summary

| Function | ESP8266 GPIO / Pin | ESP32 GPIO | Notes |
|----------|---------------------|------------|-------|
| +1 Sensor | GPIO5 / D1 | GPIO25 | IR photodiode (+1 points) |
| +2 Sensor | GPIO14 / D5 | GPIO26 | IR photodiode (+2 points) |
| +3 Sensor | GPIO4 / D2 | GPIO27 | IR photodiode (+3 points) |
| LED Data | GPIO2 / D4 | **GPIO4** | WS2812B strip data (optional) |
| Status LED | GPIO2 / D4 | GPIO2 | Onboard LED — blinks on game events |

**Pin Mapping Reference**:
- D1 Mini and NodeMCU use the same GPIO numbering
- All sensor pins configured as `INPUT_PULLUP`
- Internal pull-ups eliminate need for external resistors

## Hardware Components

### 1. IR Photodiodes
- **Quantity**: 3 (for +1, +2, +3 scoring holes)
- **Type**: Infrared photodiode sensors
- **Recommended models**: 
  - 3mm or 5mm IR photodiodes (e.g., BPW21, SFH203, or similar)
  - Fast response time preferred for ball detection
  - Peak sensitivity around 950nm wavelength
- **Operating voltage**: 3.3V
- **Output**: Analog voltage that decreases when IR light is detected
- **Typical resistance**: 10kΩ - 1MΩ depending on IR illumination

### 2. Status LED Strip (Optional)
- **Type**: WS2812B addressable LED strip
- **Default count**: 30 LEDs (configurable)
- **Power**: 5V DC (separate supply for >10 LEDs)
- **Data pin**: GPIO2 (D4) on ESP8266 · **GPIO4** on ESP32
- **Purpose**: Visual feedback and game state indication

> **ESP32 note:** The default data pin was changed from GPIO2 to GPIO4 so that
> GPIO2 (the DevKit onboard LED) can be used as an explicit secondary status
> indicator. If you must use GPIO2 for the strip, push `gpioPin: 2` via the
> admin panel — the status LED will fall back gracefully to the UART1
> side-effect mode (always-on between frames, same as ESP8266).

### 3. Power Supply
- **USB**: 5V from USB port (500mA typical)
- **External**: 5-12V via VIN pin (onboard regulator)
- **Consumption**: ~80mA (ESP8266) + LED strip + sensors

## Board-Specific Wiring Diagrams

### Wemos D1 Mini — Standard Configuration

```
D1 Mini Pin Layout:
         ┌───────────┐
    RST ─┤1        8├─ A0
     A0 ─┤2        7├─ D0 (GPIO16)
     D0 ─┤3        6├─ D1 (GPIO5)  ─── +1 Photodiode Signal + 10kΩ Pull-up
     D1 ─┤4        5├─ D2 (GPIO4)  ─── +3 Photodiode Signal + 10kΩ Pull-up
     D2 ─┤5        4├─ D3 (GPIO0)
     D3 ─┤6        3├─ D4 (GPIO2)  ─── LED Strip Data
     D4 ─┤7        2├─ GND ─────────── Photodiode Cathodes + Power GND
    3V3 ─┤8        1├─ 5V  ─────────── Pull-up Resistors + LED Strip VCC
         └───────────┘
              USB
         ┌─────────────┐
    D5 ─┤13        12├─ D6
    D6 ─┤14        11├─ D7  
    D7 ─┤15        10├─ D8
    D8 ─┤16         9├─ RX
         └─────────────┘
         
D5 (GPIO14) ─── +2 Photodiode Signal + 10kΩ Pull-up
```

### NodeMCU v2/v3 — Standard Configuration

```
NodeMCU Pin Layout (30-pin):
    ┌─────────────────────────────┐
3V3─┤1                         30├─VIN
GND─┤2                         29├─GND  
 TX─┤3                         28├─3V3
 RX─┤4                         27├─EN
 D8─┤5                         26├─RST
 D7─┤6                         25├─A0
 D6─┤7                         24├─D0
 D5─┤8  ← GPIO14 (+2 Photodiode + 10kΩ) 23├─D1 ← GPIO5 (+1 Photodiode + 10kΩ)
3V3─┤9                         22├─D2 ← GPIO4 (+3 Photodiode + 10kΩ)  
GND─┤10                        21├─D3
 D4─┤11 ← GPIO2 (LED Strip)    20├─D4 (duplicate)
 D3─┤12                        19├─GND
 D2─┤13                        18├─D5 (duplicate) 
 D1─┤14                        17├─D6
 D0─┤15                        16├─D7
    └─────────────────────────────┘
               USB Port
```

## Sensor Wiring Details

### IR Photodiode Connection

Each sensor consists of an **IR photodiode** that generates current when exposed to infrared light. A **pull-up resistor** converts this to a digital signal.

**IR Photodiode Circuit (per sensor):**
```
ESP8266 3.3V ──┬── [10kΩ pull-up resistor] ──┬── ESP8266 GPIO
               │                              │
               └── IR Photodiode Cathode      └── IR Photodiode Anode
ESP8266 GND ──────── IR Photodiode Anode
```

**Alternative Circuit (reverse bias for better sensitivity):**
```
ESP8266 3.3V ──┬── [10kΩ pull-up resistor] ──┬── ESP8266 GPIO  
               │                              │
               └── IR Photodiode Anode        └── IR Photodiode Cathode
ESP8266 GND ──────── IR Photodiode Cathode
```

### Sensor Placement
Position IR photodiodes to detect changes in ambient IR light when balls pass through holes:
- **+1 Point Hole**: IR photodiode connected to D1 (GPIO5)
- **+2 Point Hole**: IR photodiode connected to D5 (GPIO14)  
- **+3 Point Hole**: IR photodiode connected to D2 (GPIO4)

**Mounting Options:**
- **Reflective setup**: Point photodiode at scoring hole with IR reflector on opposite side
- **Ambient detection**: Use natural IR fluctuation when ball blocks ambient light
- **Active IR source**: Add IR LED for consistent illumination (optional)

### Signal Logic
- **No Ball Present**: GPIO reads HIGH (~3.3V) due to pull-up resistor
- **Ball Present**: IR photodiode conducts, pulling GPIO LOW (~0V)
- **Pull-up Resistor**: 10kΩ external resistor required (ESP8266 internal pull-up also enabled)
- **Sensitivity**: Adjust pull-up resistor value (1kΩ-100kΩ) for optimal detection
- **Debounce**: 500ms minimum between triggers per sensor

## LED Strip Connection (Optional)

### Single LED Strip
```
WS2812B Strip:
    VCC ─── D1 Mini 5V (or external 5V supply)
    GND ─── D1 Mini GND  
    DIN ─── D4 (GPIO2) via level shifter (recommended)
```

### Level Shifter (Recommended for >10 LEDs)
```
3.3V Logic → 5V Logic Level Shifter:
ESP8266 3.3V ── Level Shifter LV
ESP8266 GND  ── Level Shifter GND (LV side)
5V Supply    ── Level Shifter HV  
5V GND       ── Level Shifter GND (HV side)
GPIO2 (D4)   ── Level Shifter LV1
LED Strip DIN── Level Shifter HV1
```

## Power Requirements

### Power Consumption Analysis
- **ESP8266**: 80mA @ 3.3V (typical WiFi usage)
- **ESP32**: 120mA @ 3.3V (typical WiFi usage)
- **IR Photodiodes**: <1mA each @ 3.3V (3× photodiodes = <3mA)
- **Pull-up resistors**: ~0.3mA each when activated (3× resistors = ~1mA)
- **LED Strip**: 60mA per LED @ 5V (full white brightness)

### Power Supply Options

**USB Power (Default):**
- Suitable for sensor-only configuration
- Supports small LED strips (<10 LEDs)
- 500mA limit from USB port

**External 5V Supply:**
- Required for large LED strips (>10 LEDs)
- Connect to VIN pin (onboard regulator)
- Recommended: 2A+ for strips with 30+ LEDs

## Component Sourcing

### Essential Components
| Component | Quantity | Supplier | Notes |
|-----------|----------|----------|--------|
| Wemos D1 Mini | 1 | AliExpress, Amazon | Clone versions acceptable |
| IR photodiodes | 3 | AliExpress, Digikey | BPW21, SFH203, or similar |
| 10kΩ resistors | 3 | Local electronics | Pull-up resistors for photodiodes |
| Jumper wires | 1 kit | AliExpress | Male-female preferred |
| Breadboard (optional) | 1 small | Local electronics | For prototyping |

### Optional Components
| Component | Use Case | Notes |
|-----------|----------|--------|
| WS2812B LED strip | Visual feedback | 1m/30 LED typical |
| 3.3V→5V level shifter | LED strip >10 count | 74HCT245 or bi-directional |
| External 5V PSU | Large LED installations | 2-5A capacity |
| IR LEDs (940nm) | Active illumination | For consistent IR source |
| 1kΩ-100kΩ resistors | Sensitivity tuning | Alternative pull-up values |

## Assembly Instructions

### Basic Sensor-Only Setup

1. **Power connections**:
   - Connect D1 Mini 3.3V to breadboard power rail
   - Connect D1 Mini GND to breadboard ground rail

2. **Photodiode circuits** (repeat for each sensor):
   - Connect 10kΩ resistor between 3.3V and GPIO pin
   - Connect photodiode anode to GPIO pin  
   - Connect photodiode cathode to GND
   - **Sensor 1**: GPIO5 (D1) with 10kΩ pull-up
   - **Sensor 2**: GPIO14 (D5) with 10kΩ pull-up
   - **Sensor 3**: GPIO4 (D2) with 10kΩ pull-up

3. **Physical mounting**:
   - Position photodiodes near each scoring hole
   - Ensure photodiodes can detect IR changes when balls pass
   - Test different angles and distances for optimal sensitivity
   - Secure wiring to prevent disconnection during gameplay

### With LED Strip

4. **LED strip connection**:
   - LED strip DIN to D4 (GPIO2)
   - LED strip VCC to external 5V supply (if >10 LEDs)
   - LED strip GND to common ground
   - Add level shifter for reliable data transmission

### Testing Procedure

1. **Upload firmware**: `pio run -t upload -e d1_mini` (or `-e nodemcuv2`)
2. **Monitor serial**: `pio device monitor`
3. **Test photodiode response**: 
   - Cover each photodiode with your hand
   - Verify GPIO state changes in serial output
   - Adjust sensor position if no response detected
4. **Check WiFi connection**: Verify connection to game server
5. **Test LED feedback**: Confirm visual responses to sensor events
6. **Ball detection test**: Drop balls through holes, confirm scoring events

## Troubleshooting

### Common Issues

| Problem | Likely Cause | Solution |
|---------|--------------|----------|
| Sensor not triggering | Poor photodiode positioning, wrong polarity | Check wiring, test different positions/angles |
| Too sensitive/false triggers | Pull-up resistor too low, ambient IR interference | Use higher value pull-up (47kΩ-100kΩ), shield from sunlight |
| Not sensitive enough | Pull-up resistor too high, weak IR signal | Use lower value pull-up (1kΩ-4.7kΩ), add IR LED source |
| WiFi connection fails | Weak signal, wrong credentials | Use WiFiManager portal (192.168.4.1) |
| LED strip not working | Power/level issues | Check 5V power, add level shifter |
| Frequent disconnections | Power supply instability | Use quality USB cable/power supply |

### GPIO Pin Conflicts

**Avoid these ESP8266 pins:**
- GPIO0: Boot mode pin (connected to FLASH button)
- GPIO15: Boot strapping pin (must be LOW at boot)
- GPIO2: Boot strapping pin (must be HIGH at boot, OK for LED after boot)  
- GPIO1, 3: UART TX/RX (used for programming/serial monitor)

### Board-Specific Issues

**Wemos D1 Mini:**
- Generally more reliable due to better voltage regulation
- CP2104 USB chip supports higher baud rates
- Compact form factor may have limited pin access

**NodeMCU v2/v3:**
- May require CH340 driver installation on some systems
- Larger form factor easier for prototyping
- Some clones have poor voltage regulation

## Configuration

### WiFiManager Setup
1. On first boot, device creates AP: `Derby-Sensor-XXXXXX`
2. Connect to AP and open `192.168.4.1` in browser  
3. Enter WiFi credentials and server details
4. Device reboots and connects to game server

### Firmware Configuration
Edit `src/config.h` to customize:
```cpp
#define PIN_SENSOR_1  5     // +1 point photodiode (D1)
#define PIN_SENSOR_2  14    // +2 point photodiode (D5)  
#define PIN_SENSOR_3  4     // +3 point photodiode (D2)
#define PIN_LED       2     // ESP8266: LED strip data (D4)
                            // ESP32:   LED strip data (GPIO4 default)
#define DEBOUNCE_MS   500   // Sensor debounce time
```

## Integration with Game System

After hardware setup:
1. **Server configuration**: Ensure server is running and accessible
2. **Device registration**: Sensor automatically registers with server
3. **Game integration**: Server assigns device to game instance  
4. **Scoring events**: Each photodiode activation sends immediate score update
5. **LED feedback**: Server controls LED colors/patterns based on game state

## Next Steps

After hardware assembly:
- See main project README for complete system setup
- Configure game server and admin interface
- Test full game workflow with multiple sensor nodes
- Adjust photodiode sensitivity and debounce timing as needed
- Fine-tune pull-up resistor values for optimal detection
