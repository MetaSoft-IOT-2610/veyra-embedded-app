# User Stories — Veyra Embedded App

**Maintained by**: Metasoft  
**Project**: Veyra health-monitoring firmware  
**Last updated**: June 2026

User stories for the Veyra health-monitoring application developed by Metasoft, built on the Modest IoT Nano-framework (C++ Edition) by Angel Velasquez.

Personas:

- **Device User** — interacts with the physical device and reads the LCD / Serial output.
- **Device Maker** — integrates and extends the firmware using the framework.

Acceptance criteria use the **Given-When-Then** format.

---

## Device User Stories

### US01: Monitor ambient temperature

- **As a** Device User, **I want** ambient temperature from the LM35, **so that** I know the environmental temperature.
- **Acceptance Criteria**:
  - **Given** the LM35 is in free air on GPIO 34, **when** the device runs, **then** Serial shows `Ambient (LM35): XX.X C`.
  - **Given** the reading is below 30 °C, **when** status is refreshed, **then** body temperature is shown as `--`.
  - **Given** the reading reaches ≥ 30 °C (valid skin contact), **when** status is refreshed, **then** Serial shows `Body (LM35)` only and does not print the ambient line.

### US02: Monitor body temperature

- **As a** Device User, **I want** body temperature when the LM35 touches my skin, **so that** I can estimate skin/body temperature.
- **Acceptance Criteria**:
  - **Given** the LM35 is held against skin until the reading reaches ≥ 30 °C, **when** status refreshes, **then** Serial shows `Body (LM35): XX.X C`.
  - **Given** the LCD is on page 1, **when** body temperature is valid, **then** line 0 shows `Body:XX.XC`.
  - **Given** body temperature is valid, **when** the LCD is on page 2, **then** line 0 shows `Skin contact OK` instead of ambient.

### US03: Monitor heart rate and SpO2

- **As a** Device User, **I want** heart rate and blood oxygen from the MAX30102, **so that** I can check basic vitals.
- **Acceptance Criteria**:
  - **Given** my finger covers the MAX30102 window with light pressure, **when** I hold still for several seconds, **then** Serial shows `Heart rate` and `SpO2` lines.
  - **Given** the signal is unstable, **when** readings are processed, **then** smoothed values are shown (not raw spikes).
  - **Given** no valid HR/SpO2 yet, **when** status refreshes, **then** Serial shows a PPG diagnostic line (`waiting for finger`, `measuring pulse`, `saturated`, or `sensor not detected`).

### US04: Monitor GPS status

- **As a** Device User, **I want** GPS status on Serial and LCD, **so that** I know whether location is available.
- **Acceptance Criteria**:
  - **Given** the GPS module is wired (TX → GPIO 17), **when** no fix is available, **then** status shows `searching fix` or `no data`.
  - **Given** NMEA is received but there is no fix, **when** GSV sentences are parsed, **then** status may show satellites in view (e.g. `searching fix (0 used, 4 in view)`).
  - **Given** a valid fix outdoors, **when** status refreshes, **then** latitude and longitude appear on Serial and rotating LCD pages.
  - **Given** the module uses a non-default baud rate, **when** the device boots, **then** `Neo6m` autodetects among 9600, 115200, and 4800 baud.

### US05: Continuous LCD feedback

- **As a** Device User, **I want** the LCD to update regularly, **so that** I always see current status without waiting for a sensor event.
- **Acceptance Criteria**:
  - **Given** the device is running, **when** at least 2 seconds elapse, **then** the LCD content updates even if no new GPS fix or PPG reading occurred.
  - **Given** the device is running, **when** multiple pages are configured, **then** the LCD rotates through vitals, temperature/GPS, and GPS detail pages.

---

## Device Maker Stories

### US06: Minimal sketch using the framework

- **As a** Device Maker, **I want** a thin sketch that delegates to `VeyraDevice`, **so that** application logic stays in the device class.
- **Acceptance Criteria**:
  - **Given** `veyra-embedded-app.ino`, **when** I inspect `loop()`, **then** it only calls `device.update()`.
  - **Given** `VeyraDevice` extends `Device`, **when** sensors emit events, **then** they propagate through `Sensor::on()` to the device handler.
  - **Given** the current application, **when** `VeyraDevice::on()` is called, **then** it is a no-op; Serial/LCD refresh is driven by the 2 s timer in `refreshStatus()`, not by event handlers.

### US07: Sensor event propagation

- **As a** Device Maker, **I want** each sensor to emit framework events, **so that** I can extend `VeyraDevice::on()` without changing sensor drivers.
- **Acceptance Criteria**:
  - **Given** LM35 polling, **when** `readAndTriggerEvent()` runs, **then** `TEMPERATURE_READ_EVENT` is emitted.
  - **Given** MAX30102 metrics are ready, **when** `update()` completes a window, **then** `PPG_READ_EVENT` is emitted.
  - **Given** a valid NMEA fix, **when** `Neo6m::update()` parses it, **then** `LOCATION_READ_EVENT` is emitted.

### US08: Separate sensor concerns

- **As a** Device Maker, **I want** each sensor to own a single responsibility, **so that** the codebase stays maintainable.
- **Acceptance Criteria**:
  - **Given** `Max30102`, **when** I inspect the public API, **then** it exposes HR/SpO2 only (no temperature API).
  - **Given** `Lm35`, **when** I inspect the public API, **then** it exposes temperature readings and body-contact detection.
  - **Given** `spo2_algorithm`, **when** used, **then** it is called only from `Max30102` (not from the application layer).

### US09: Startup diagnostics

- **As a** Device Maker, **I want** startup messages for missing I2C devices and GPS UART status, **so that** I can debug wiring quickly.
- **Acceptance Criteria**:
  - **Given** MAX30102 is not detected, **when** `setup()` completes, **then** Serial prints a MAX30102 I2C warning.
  - **Given** the LCD is not detected, **when** `setup()` completes, **then** Serial prints an LCD I2C warning.
  - **Given** GPS UART was probed at boot, **when** `setup()` completes, **then** Serial prints baud rate and byte count.

---

## Attribution

| Component | Author |
|-----------|--------|
| Veyra firmware, drivers, documentation | Metasoft |
| Modest IoT Nano-framework core | Angel Velasquez ([CC BY-ND 4.0](https://creativecommons.org/licenses/by-nd/4.0/legalcode)) |
| `spo2_algorithm` | Maxim Integrated |
