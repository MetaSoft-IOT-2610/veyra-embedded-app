/**
 * @file veyra-embedded-app.ino
 * @brief Veyra Embedded App — sensors and LCD with Modest IoT Nano-framework.
 *
 * @author Angel Velasquez
 * @date March 23, 2025
 * @version 0.1
 */

/*
 * This file is part of the Veyra Embedded App project.
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 *
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
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
