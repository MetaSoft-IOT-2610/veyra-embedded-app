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

    Serial.println();
    Serial.println(F("=== Inicio Veyra ==="));

    device.begin();

    if (!device.getMax30102().isInitialized()) {
        Serial.println(F("Sensor pulso: no detectado al arrancar"));
    } else {
        for (int i = 0; i < 30; i++) {
            device.update();
            delay(50);
        }
        Serial.println(F("Sensor pulso: listo - apoya el dedo para medir"));
    }

    if (!device.getLcd().isInitialized()) {
        Serial.println(F("Pantalla LCD: no detectada (SDA21 SCL22)"));
    } else {
        Serial.println(F("Pantalla LCD: lista"));
    }

    if (device.getNeo6m().isReceivingData()) {
        Serial.println(F("GPS: recibiendo datos"));
    } else {
        Serial.println(F("GPS: sin datos al arrancar - normal en interiores"));
    }

    if (device.getEdgeHttp().isAuthenticated()) {
        Serial.printf("Servidor edge: autenticado (%s)\n", device.getEdgeHttp().getDeviceId());
    } else if (device.getEdgeHttp().isConnected()) {
        Serial.println(F("Servidor edge: Wi-Fi ok, sign-in fallido - revisa secrets.h y nodes.seed.json"));
    } else {
        Serial.println(F("Servidor edge: sin Wi-Fi - revisa secrets.h"));
    }

    Serial.println(F("===================="));
}

void loop() {
    device.update();
}
