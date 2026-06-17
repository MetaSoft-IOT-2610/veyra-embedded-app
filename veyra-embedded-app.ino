/**
 * @file veyra-embedded-app.ino
 * @brief Veyra Embedded App — sensors and LCD with Modest IoT Nano-framework.
 *
 * @author Metasoft
 * @date June 2026
 * @version 0.1
 */

/*
 * Veyra Embedded App
 * Copyright (c) 2026 Metasoft
 *
 * Developed by Metasoft for the Veyra health-monitoring device.
 * Built on the Modest IoT Nano-framework (C++ Edition) by Angel Velasquez.
 * Framework source files retain their original copyright and CC BY-ND 4.0 license.
 */

#include "ModestIoT.h"
#include "VeyraDevice.h"

VeyraDevice device;

void setup() {
    Serial.begin(115200);
    delay(500);
    device.begin();

    if (!device.getMax30102().isInitialized()) {
        Serial.println("MAX30102: sensor not detected on I2C (SDA33 SCL32)");
    }
    if (!device.getLcd().isInitialized()) {
        Serial.println("LCD1602: display not detected on I2C (SDA21 SCL22)");
    }
    Serial.printf(
        "GPS UART: %ld baud, %lu bytes at startup (GPS TX -> ESP32 GPIO%d)\n",
        device.getNeo6m().getActiveBaudRate(),
        device.getNeo6m().getBytesReceived(),
        VeyraDevice::GPS_RX_PIN
    );
}

void loop() {
    device.update();
}
