# Roll-a-Ball Derby — ESP32 Motor Controller Hardware Guide

## Table of Contents

- [Overview](#overview)
- [Supported Board Configurations](#supported-board-configurations)
  - [ESP32 DevKit v1 (Generic)](#esp32-devkit-v1-generic)
  - [ESP32-S3 DevKit-C](#esp32-s3-devkit-c)
- [Pin Configuration Summary](#pin-configuration-summary)
- [Hardware Components](#hardware-components)
- [Board-Specific Wiring Diagrams](#board-specific-wiring-diagrams)
  - [ESP32 DevKit v1 — 4-Motor Configuration](#esp32-devkit-v1--4-motor-configuration)
  - [ESP32-S3 DevKit-C — 4-Motor Configuration](#esp32-s3-devkit-c--4-motor-configuration)
- [Power Requirements](#power-requirements)
- [Component Sourcing](#component-sourcing)
- [Assembly Instructions](#assembly-instructions)
- [Troubleshooting](#troubleshooting)
- [Configuration Files](#configuration-files)

---

## Overview

The ESP32 Motor Controller manages stepper motors, LED displays, buttons, and audio feedback for the Roll-a-Ball Derby game. This document covers hardware requirements, wiring diagrams, and pin configurations for all supported ESP32 board variants.

## Supported Board Configurations

### ESP32 DevKit v1 (Generic)
- **Chip**: ESP32-WROOM-32
- **Flash**: 4MB
- **Features**: Wi-Fi + Classic Bluetooth (BR/EDR)
- **Upload Speed**: 921600 baud
- **PlatformIO Board ID**: `esp32dev`

### ESP32-S3 DevKit-C
- **Chip**: ESP32-S3-WROOM-1
- **Flash/RAM**: 16MB Flash + 8MB OctalSPI PSRAM
- **Features**: Wi-Fi + BLE 5 Mesh — **NO Classic Bluetooth / NO A2DP**
- **Upload Speed**: 921600 baud
- **PlatformIO Board ID**: `esp32-s3-devkitc-1`

## Pin Configuration Summary

| Function | GPIO (ESP32 DevKit) | GPIO (ESP32-S3) | Notes |
|----------|---------------------|-----------------|-------|
| Status LED | 2 | 2 | Onboard LED (active-high) |
| LED Matrix (WS2812B) | 4 | 4 | Data line |
| Button 1 (Start/Reset) | 34 | 3 | S3: internal pull-up OK (TOUCH3) |
| Button 2 (Pause/Resume) | 35 | 46 | S3: strapping pin — safe with INPUT_PULLUP, avoid pressing during boot |
| Motor 0 (IN1–IN4) | 16, 17, 18, 19 | 5, 6, 7, 15 | S3: GPIO15 = XTAL_32K_P alias (unused on DevKitC) |
| Motor 1 (IN1–IN4) | 21, 22, 23, 25 | 16, 17, 18, 8 | S3: pins are physically consecutive on left header |
| Motor 2 (IN1–IN4) | 26, 27, 32, 33 | 9, 10, 11, 12 | S3: pins are physically consecutive on left header |
| Motor 3 (IN1–IN4) | 12, 13, 14, 15 | 13, 14, 21, 39 | S3: IN1/IN2 left header, IN3/IN4 right header |
| Limit Switch 0 | 36 | 1 | S3: full bidirectional GPIO (not input-only) |
| Limit Switch 1 | 39 | 42 | S3: full bidirectional GPIO |

**Legend**: ✅ = Fully supported, ⚠️ = Use with care, ➖ = Not available on that board

## Hardware Components

### 1. Stepper Motors (28BYJ-48)
- **Quantity**: 1-4 motors (up to 8 with GPIO expanders)
- **Type**: Unipolar stepper motor with gear reduction (64:1)
- **Steps per revolution**: 4096 half-steps
- **Voltage**: 5V DC
- **Current**: ~150mA per motor

### 2. Motor Drivers (ULN2003)
- **Quantity**: 1 per motor
- **Type**: Darlington transistor array
- **Connections**: 4 control pins (IN1-IN4) to ESP32 GPIOs

### 3. Control Buttons
- **Quantity**: 2 (Start/Reset, Pause/Resume)
- **Type**: Momentary push button (normally open)
- **Wiring**: One side to GPIO, other side to GND
- **Configuration**: INPUT_PULLUP (internal pull-up resistors)

### 4. LED Display
- **Type**: WS2812B LED strip or matrix
- **Default Count**: Configurable via server
- **Power**: 5V DC (separate power supply recommended for >10 LEDs)
- **Data**: Single GPIO pin with level shifter recommended

### 5. Limit Switches (Optional)
- **Quantity**: 1 per motor lane (up to 2 by default)
- **Type**: Micro switch or optical sensor
- **Wiring**: NO (normally open) contact
- **Configuration**: INPUT_PULLUP

## Board-Specific Wiring Diagrams

### ESP32 DevKit v1 — 4-Motor Configuration

```
ESP32 DevKit v1 Pin Layout:
                     ┌─────────────┐
               3V3 ──┤1          30├── VIN
               GND ──┤2          29├── GND  
                TX ──┤3          28├── 13 ── Motor 3 IN1
                RX ──┤4          27├── 12 ── Motor 3 IN4
        Button 1 ── 34┤5          26├── 14 ── Motor 3 IN2
        Button 2 ── 35┤6          25├── 27 ── Motor 2 IN2
    Limit Switch 0 ── 36┤7          24├── 26 ── Motor 2 IN1
    Limit Switch 1 ── 39┤8          23├── 25 ── Motor 1 IN4
                EN ──┤9          22├── 33 ── Motor 2 IN4
      Status LED ──  2┤10         21├── 32 ── Motor 2 IN3
        Unused ──  0┤11         20├── 35 (input only)
        Unused ──  4┤12         19├── 34 (input only) 
     LED Matrix ──  4┤13         18├── 5 ── (Flash)
      Motor 0 IN1 ── 16┤14         17├── 18 ── Motor 0 IN3
      Motor 0 IN2 ── 17┤15         16├── 19 ── Motor 0 IN4
      Motor 1 IN1 ── 21┤16         15├── 15 ── Motor 3 IN3
      Motor 1 IN2 ── 22┤17         14├── 2 ── Status LED
      Motor 1 IN3 ── 23┤18         13├── 4 ── LED Matrix
                     └─────────────┘
```

**Motor Wiring (per ULN2003 driver):**
```
ESP32 GPIO → ULN2003 → 28BYJ-48
      IN1  →   IN1   →   Pink
      IN2  →   IN2   →   Orange  
      IN3  →   IN3   →   Yellow
      IN4  →   IN4   →   Blue
      GND  →   GND   →   GND
      5V   →   VCC   →   Red
```

### ESP32-S3 DevKit-C — 4-Motor Configuration

> ⚠️ **No A2DP audio** — see [board configuration note](#esp32-s3-devkit-c) above.

The S3 left header is used for all 4 motor groups plus buttons/LED Matrix. Motor groups are
physically consecutive rows on the left header for clean cable runs. Motor 3 IN3/IN4 use the right
header (GPIO21 and GPIO39).

```
ESP32-S3-DevKitC-1 Pin Layout:
                                 ┌──────────────────┐
                          3V3 ──┤                  ├── GND
                          3V3 ──┤                  ├── 43  (U0TX — debug serial)
                          RST ──┤                  ├── 44  (U0RX — debug serial)
               LED Matrix ──  4 ┤                  ├── 1   ── Limit Switch 0
             Motor 0 IN1 ──  5 ─┤                  ├── 2   ── Status LED
             Motor 0 IN2 ──  6 ─┤   ESP32-S3       ├── 42  ── Limit Switch 1
             Motor 0 IN3 ──  7 ─┤   S3-WROOM-1     ├── 41
             Motor 0 IN4 ── 15 ─┤                  ├── 40
             Motor 1 IN1 ── 16 ─┤   BLE 5 only     ├── 39  ── Motor 3 IN4
             Motor 1 IN2 ── 17 ─┤                  ├── 38  (RGB LED — onboard)
             Motor 1 IN3 ── 18 ─┤   [RGB @ IO38]   ├── 37  (PSRAM on R8 — avoid)
             Motor 1 IN4 ──  8 ─┤                  ├── 36  (PSRAM on R8 — avoid)
               Button 1  ──  3 ─┤                  ├── 35  (PSRAM on R8 — avoid)
               Button 2  ── 46 ─┤                  ├── 34  (PSRAM on R8 — avoid)
             Motor 2 IN1 ──  9 ─┤                  ├── 0   (BOOT — reserved)
             Motor 2 IN2 ── 10 ─┤                  ├── 45
             Motor 2 IN3 ── 11 ─┤                  ├── 48
             Motor 2 IN4 ── 12 ─┤                  ├── 47
             Motor 3 IN1 ── 13 ─┤                  ├── 21  ── Motor 3 IN3
             Motor 3 IN2 ── 14 ─┤                  ├── 20  (USB_D+ — reserved)
                          5V0 ──┤                  ├── 19  (USB_D- — reserved)
                          GND ──┤                  ├── GND
                                 └──────────────────┘
```

**Notes on the S3 pin assignment:**

- **GPIO15 (Motor 0 IN4)**: Labelled `XTAL_32K_P` on the board silkscreen — this is the 32kHz crystal input but is free for GPIO use on the DevKitC-1 (no external crystal fitted).
- **GPIO16–17 (Motor 1 IN1–IN2)**: Labelled `XTAL_32K_N` / `U1TXD` — usable as GPIO when UART1 is not in use (it isn't in this firmware).
- **GPIO46 (Button 2)**: Strapping pin that sets ROM log level on boot. With `INPUT_PULLUP` it is HIGH during boot (safe state). Only avoid pressing it during a reset.
- **GPIO34–37**: Connected to OctalSPI PSRAM on the WROOM-1-N16R8 module — do not use.
- **GPIO38**: Drives the onboard RGB LED on the DevKitC-1 board — avoid for other functions.
- **GPIO19–20**: USB D–/D+ — do not use if USB OTG is active.

**Motor Wiring (per ULN2003 driver):**
```
ESP32-S3 GPIO → ULN2003 → 28BYJ-48
          IN1  →   IN1   →   Pink
          IN2  →   IN2   →   Orange
          IN3  →   IN3   →   Yellow
          IN4  →   IN4   →   Blue
          GND  →   GND   →   GND
          5V   →   VCC   →   Red
```

## Power Requirements

### Power Supply Sizing
- **ESP32 board**: 500mA @ 3.3V (via USB or VIN pin)
- **Per 28BYJ-48 motor**: 150mA @ 5V
- **LED strip**: ~60mA per WS2812B LED @ 5V (full white)
- **Total 5V requirements**: (150mA × motor_count) + (60mA × led_count)

### Recommended Setup
- **USB Power**: Suitable for 1-2 motors + small LED strip (<10 LEDs)
- **External 5V PSU**: Required for 3+ motors or large LED displays
- **Power Distribution**: Use breadboard power supply or dedicated 5V rail

## Component Sourcing

### Essential Components
| Component | Quantity | Supplier | Notes |
|-----------|----------|----------|--------|
| ESP32 DevKit v1 | 1 | AliExpress, Amazon | Generic clone OK |
| 28BYJ-48 + ULN2003 | 1-4 sets | AliExpress | Often sold as complete kit |
| WS2812B LED strip | 1m (30 LEDs) | AliExpress | Individual or strip |
| Momentary buttons | 2 | Local electronics | 6mm tactile switches |
| Micro limit switches | 2 | AliExpress | Roller lever type preferred |
| Jumper wires | 1 kit | AliExpress | Male-female, male-male |
| Breadboard | 1 large | AliExpress | Half-size minimum |

### Optional Components  
| Component | Use Case | Notes |
|-----------|----------|--------|
| Level shifter (3.3V→5V) | LED strips >10 count | 74HCT245 or similar |
| External 5V PSU | Multiple motors + LEDs | 2-5A capacity |
| GPIO expander | >4 motors | MCP23017 via I2C |

## Assembly Instructions

### Basic 2-Motor Setup

1. **Power connections**:
   - Connect ESP32 VIN to 5V power supply positive
   - Connect ESP32 GND to power supply negative
   - Connect ULN2003 VCC to 5V positive
   - Connect all GND pins together

2. **Motor driver connections**:
   - Motor 0: Connect GPIOs 16,17,18,19 to ULN2003 IN1,IN2,IN3,IN4
   - Motor 1: Connect GPIOs 21,22,23,25 to second ULN2003 IN1,IN2,IN3,IN4

3. **Button connections**:
   - Button 1: One side to GPIO34, other side to GND
   - Button 2: One side to GPIO35, other side to GND

4. **LED connection**:
   - WS2812B data wire to GPIO4
   - LED strip power to 5V (with level shifter if needed)
   - LED strip ground to common GND

5. **Limit switches (optional)**:
   - Limit switch 0: NO contact between GPIO36 and GND
   - Limit switch 1: NO contact between GPIO39 and GND

### Testing Procedure

1. **Upload firmware** using PlatformIO: `pio run -t upload`
2. **Monitor serial output**: `pio device monitor`  
3. **Verify WiFi connection** and server discovery
4. **Test motor movement** using web admin interface
5. **Verify button responses** and LED feedback
6. **Test homing sequence** if limit switches installed

## Troubleshooting

### Common Issues

| Problem | Likely Cause | Solution |
|---------|--------------|----------|
| Motors don't turn | Loose connections, insufficient power | Check wiring, ensure 5V supply |
| Buttons don't respond | Wrong GPIO, missing pullup | Verify pins, check INPUT_PULLUP |  
| LEDs don't light | Level mismatch, power issue | Add level shifter, check 5V power |
| WiFi connection fails | Wrong credentials, signal weak | Use WiFiManager portal, check signal |
| Homing doesn't work | Limit switch wiring | Verify NO contact to GND |

### GPIO Pin Conflicts

**Avoid these ESP32 DevKit v1 pins**:
- GPIO 0: Boot strapping (LOW during boot → download mode)
- GPIO 1, 3: UART0 TX/RX (programming + serial monitor)
- GPIO 6–11: Connected to SPI flash (do not use)
- GPIO 2, 15: Strapping pins (may affect boot mode)

**Avoid these ESP32-S3 pins**:
- GPIO 0: Boot strapping (do not drive LOW during boot)
- GPIO 19–20: USB D–/D+ (reserved when USB OTG active)
- GPIO 26–32: Internal OctalSPI flash — do not use
- GPIO 33–37: OctalSPI PSRAM on WROOM-1-R8 variant — do not use
- GPIO 38: Onboard RGB LED on DevKitC-1
- GPIO 43–44: UART0 TXD/RXD (serial debug)
- GPIO 45: Strapping pin (VDD_SPI voltage select)
- GPIO 46: Strapping pin (ROM log level) — safe as INPUT_PULLUP after boot

## Configuration Files

The firmware reads hardware configuration from these files:
- `src/config.h`: Pin definitions and motor counts  
- `data/led-config.json`: LED strip/matrix layout
- Runtime configuration via WebSocket from game server

## Next Steps

After hardware assembly, see the main project README for:
- Server setup and network configuration
- Game administration via web interface  
- Motor calibration and tuning procedures