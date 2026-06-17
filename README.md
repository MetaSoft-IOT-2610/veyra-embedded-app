# Veyra Embedded App

**Version**: 0.1  
**Date**: June 2026

## Overview

Embedded health-monitoring application for ESP32 built on the **Modest IoT Nano-framework (C++ Edition)**. The project combines the framework core with sensors and an actuator:

| Component | Role |
|-----------|------|
| **LM35** | Ambient and body temperature (skin contact) |
| **NEO-6M** | GPS location |
| **MAX30102** | Heart rate and SpO2 (PPG) |
| **LCD 16×2 I2C** | Live status display |

Values are also reported over Serial at 115200 baud.

The design is object-oriented and CQRS-inspired: sensors emit framework events, `VeyraDevice` orchestrates polling and display, and the LCD actuator responds to commands. **Serial and LCD refresh run on a 2 s timer** in `VeyraDevice::refreshStatus()`; sensors still emit events for framework extensibility, but `VeyraDevice::on()` is currently a no-op.

## Sensor responsibilities

| Measurement | Sensor | Notes |
|-------------|--------|-------|
| Ambient temperature | LM35 | Sensor in free air (~room temperature); shown when reading &lt; 30 °C |
| Body temperature | LM35 | Same sensor; valid when held against skin (≥ 30 °C, ≤ 42 °C) |
| Heart rate / SpO2 | MAX30102 | Finger on the sensor window; Maxim algorithm + smoothing |
| Location | NEO-6M | Requires clear sky view; baud autodetected at boot (9600 / 115200 / 4800) |

The MAX30102 does **not** measure temperature in this project.

## Hardware

- ESP32 development board
- LM35 linear temperature sensor
  - **VCC** → 3.3 V
  - **GND** → GND
  - **OUT** → GPIO **4**
- NEO-6M GPS module
  - **VCC** → 3.3 V
  - **GND** → GND
  - **TX** → GPIO **17** (ESP32 RX)
  - **RX** → GPIO **16** (ESP32 TX)
- GY-MAX30102 pulse oximeter (I2C, no INT pin)
  - **VCC** → 3.3 V
  - **GND** → GND
  - **SCL** → GPIO **32**
  - **SDA** → GPIO **33**
- LCD 16×2 I2C (PCF8574 backpack)
  - **VCC** → 3.3 V
  - **GND** → GND
  - **SDA** → GPIO **21**
  - **SCL** → GPIO **22**
  - I2C address: **0x27** (try **0x3F** if blank)

Use short jumper wires; secure GND and power on every module.

## Software

- Arduino IDE with ESP32 board support (Espressif)
- Serial Monitor at **115200 baud**
- No external Arduino libraries required

## Flashing (Arduino IDE)

1. Open the folder `veyra-embedded-app` in Arduino IDE.
2. Install the **esp32** board package via Boards Manager if needed.
3. Select board **ESP32 Dev Module** and the correct COM port.
4. Click **Verify**, then **Upload**.

## Project structure

```
veyra-embedded-app/
├── veyra-embedded-app.ino   # Entry point (setup + device.update())
├── VeyraDevice.h / .cpp     # Application device
├── Lm35.h / .cpp            # Temperature sensor
├── Neo6m.h / .cpp           # GPS sensor
├── Max30102.h / .cpp        # Pulse oximeter (HR / SpO2)
├── spo2_algorithm.h / .cpp  # Maxim HR/SpO2 algorithm
├── Lcd1602.h / .cpp         # LCD actuator
├── ModestIoT.h              # Framework umbrella header
├── EventHandler.h / CommandHandler.h
├── Sensor.h / Sensor.cpp
├── Actuator.h / Actuator.cpp
├── Device.h / Device.cpp
├── README.md
├── user-stories.md
└── class-diagram.puml
```

## Framework

| Component | Role |
|-----------|------|
| `Event` / `EventHandler` | Event-driven input handling |
| `Command` / `CommandHandler` | Command-driven output handling |
| `Sensor` | Base class for input devices |
| `Actuator` | Base class for output devices |
| `Device` | Combines event and command handling |

Include the framework with:

```cpp
#include "ModestIoT.h"
```

## Application flow

`VeyraDevice` extends `Device` and composes all sensors and the LCD.

```cpp
void setup() {
    Serial.begin(115200);
    delay(500);
    device.begin();

    // Startup diagnostics (see veyra-embedded-app.ino)
    if (!device.getMax30102().isInitialized()) { /* warn */ }
    if (!device.getLcd().isInitialized()) { /* warn */ }
    // GPS UART: active baud rate and bytes received at boot
}

void loop() {
    device.update();
}
```

**Inside `VeyraDevice::update()`:**

1. Poll GPS and MAX30102 every tick.
2. Read LM35 every 1 s (emits `TEMPERATURE_READ_EVENT`).
3. Refresh Serial + LCD every 2 s (rotating 3 pages), independent of sensor events.

**I2C buses:**

- `Max30102` → **Wire** (SDA 33, SCL 32)
- `Lcd1602` → **Wire1** (SDA 21, SCL 22)

### Serial status output

Status blocks print every 2 s. LM35 lines are mutually exclusive:

- **Below 30 °C** (no skin contact): `Ambient (LM35)` plus `Body (LM35): --`.
- **≥ 30 °C** (valid skin contact): `Body (LM35)` only; ambient line is hidden.

**Example — ambient, finger on MAX30102, GPS searching:**

```
======== Veyra Status ========
Ambient (LM35):  14.0 C
Body (LM35):     -- (hold sensor flat against skin)
Heart rate:      78 bpm
SpO2:            97 %
GPS:             searching fix (0 used, 4 in view)
==============================
```

**Example — no finger, PPG waiting:**

```
======== Veyra Status ========
Ambient (LM35):  18.2 C
Body (LM35):     -- (hold sensor flat against skin)
PPG:             waiting for finger (IR avg: 8421, need >12000)
GPS:             searching fix (0 sats) — try outdoors / clear sky
==============================
```

**PPG diagnostic lines** (shown when HR/SpO2 are not yet valid):

| Message | Meaning |
|---------|---------|
| `waiting for finger` | IR below contact threshold |
| `measuring pulse` | Finger detected; collecting samples |
| `saturated` | IR too high — lighten finger pressure |
| `sensor not detected` | MAX30102 missing on I2C |

**GPS diagnostic lines:**

| Message | Meaning |
|---------|---------|
| `searching fix (N used, M in view)` | NMEA received; GSV reports satellites in view |
| `searching fix (N sats)` | NMEA received; no GSV view count yet |
| `no data (bytes @ baud, GPS TX->ESP RX17)` | No NMEA bytes after baud probe |
| `latitude` / `longitude` / `satellites` | Valid fix |

At boot, `Neo6m::begin()` probes **9600, 115200, and 4800** baud and locks onto the first rate that receives NMEA (`$`).

**LCD pages (rotate every 2 s):**

| Page | Line 0 | Line 1 |
|------|--------|--------|
| 1 | Body temp or `--` | HR / SpO2 or finger hint |
| 2 | `Amb:XX.XC` or `Skin contact OK` | GPS latitude / sats in view |
| 3 | GPS longitude / status | Fix or wiring hint |

## Usage tips

- **MAX30102:** Cover the full sensor window with the finger pad; hold still 5–10 s.
- **LM35 body temp:** Press the metal side against skin for 30–60 s; inner wrist or armpit works better than outer arm.
- **GPS:** Test outdoors with the antenna facing up; indoor fix is often unavailable.
- **Power:** Several modules on USB can trigger brownout resets; use a stable 5 V supply if uploads or runtime are unstable.

## License

Framework components are based on the Modest IoT Nano-framework (C++ Edition) by Angel Velasquez, licensed under [CC BY-ND 4.0](https://creativecommons.org/licenses/by-nd/4.0/legalcode).

The Maxim `spo2_algorithm` sources retain their original license (see file headers).
