/**
 * @file VeyraDevice.cpp
 * @brief Implements the VeyraDevice class.
 *
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.1
 */

/*
 * This file is part of the Veyra Embedded App project.
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 */

#include "VeyraDevice.h"
#include <Arduino.h>
#include <stdio.h>

VeyraDevice::VeyraDevice(
    int lm35Pin,
    int gpsRxPin,
    int gpsTxPin,
    int max30102SdaPin,
    int max30102SclPin,
    int lcdSdaPin,
    int lcdSclPin
)
    : lm35(lm35Pin, this),
      neo6m(gpsRxPin, gpsTxPin, this),
      max30102(max30102SdaPin, max30102SclPin, this),
      lcd(lcdSdaPin, lcdSclPin, this),
      lastStatusRefreshMs(0),
      lastTemperatureReadMs(0),
      lcdPage(0) {}

void VeyraDevice::begin() {
    lm35.begin();
    neo6m.begin();
    max30102.begin();
    lcd.begin();

    readTemperature();
    refreshStatus();
    lastStatusRefreshMs = millis();
    lastTemperatureReadMs = millis();
}

void VeyraDevice::update() {
    updateGps();
    updateMax30102();

    unsigned long now = millis();

    if (now - lastTemperatureReadMs >= TEMPERATURE_READ_INTERVAL_MS) {
        readTemperature();
        lastTemperatureReadMs = now;
    }

    if (now - lastStatusRefreshMs >= STATUS_REFRESH_MS) {
        refreshStatus();
        lastStatusRefreshMs = now;
    }
}

void VeyraDevice::on(Event event) {
    (void)event;
}

void VeyraDevice::handle(Command command) {
    if (command == Lcd1602::REFRESH_COMMAND) {
        return;
    }
}

void VeyraDevice::refreshStatus() {
    char line0[17];
    char line1[17];

    Serial.println();
    Serial.println(F("======== Veyra Status ========"));

    if (lm35.isBodyTemperatureValid()) {
        Serial.printf("Body (LM35):     %.1f C\n", lm35.getLastReading());
    } else {
        Serial.printf("Ambient (LM35):  %.1f C\n", lm35.getLastReading());
        Serial.println(F("Body (LM35):     -- (hold sensor flat against skin)"));
    }

    if (max30102.isLastReadingValid()) {
        Serial.printf("Heart rate:      %d bpm\n", max30102.getLastHeartRate());
    }
    if (max30102.isLastSpO2Valid()) {
        Serial.printf("SpO2:            %d %%\n", max30102.getLastSpO2());
    }
    if (!max30102.isLastReadingValid() && !max30102.isLastSpO2Valid()) {
        if (max30102.isSignalSaturated()) {
            Serial.printf(
                "PPG:             saturated (IR avg: %lu) - cover sensor or lighten pressure\n",
                static_cast<unsigned long>(max30102.getLastIrAverage())
            );
        } else if (max30102.isFingerDetected()) {
            Serial.printf(
                "PPG:             measuring pulse (IR avg: %lu, swing: %lu)\n",
                static_cast<unsigned long>(max30102.getLastIrAverage()),
                static_cast<unsigned long>(max30102.getLastIrVariation())
            );
        } else if (max30102.isInitialized()) {
            Serial.printf(
                "PPG:             waiting for finger (IR avg: %lu, need >12000)\n",
                static_cast<unsigned long>(max30102.getLastIrAverage())
            );
        } else {
            Serial.println(F("PPG:             sensor not detected (check SDA33 SCL32)"));
        }
    }

    if (neo6m.isLastReadingValid()) {
        Serial.printf("GPS latitude:    %.6f\n", neo6m.getLastLatitude());
        Serial.printf("GPS longitude:   %.6f\n", neo6m.getLastLongitude());
        Serial.printf("GPS satellites:  %d\n", neo6m.getSatelliteCount());
    } else if (neo6m.isReceivingData()) {
        if (neo6m.getSatellitesInView() > 0) {
            Serial.printf(
                "GPS:             searching fix (%d used, %d in view)\n",
                neo6m.getSatelliteCount(),
                neo6m.getSatellitesInView()
            );
        } else {
            Serial.printf(
                "GPS:             searching fix (%d sats) — try outdoors / clear sky\n",
                neo6m.getSatelliteCount()
            );
        }
    } else {
        Serial.printf(
            "GPS:             no data (%lu bytes @ %ld baud, GPS TX->ESP RX%d)\n",
            neo6m.getBytesReceived(),
            neo6m.getActiveBaudRate(),
            GPS_RX_PIN
        );
    }
    Serial.println(F("=============================="));

    switch (lcdPage % 3) {
        case 0:
            if (lm35.isBodyTemperatureValid()) {
                snprintf(line0, sizeof(line0), "Body:%.1fC", lm35.getLastReading());
            } else {
                snprintf(line0, sizeof(line0), "Body: -- C");
            }
            if (max30102.isLastReadingValid() || max30102.isLastSpO2Valid()) {
                if (max30102.isLastReadingValid() && max30102.isLastSpO2Valid()) {
                    snprintf(
                        line1,
                        sizeof(line1),
                        "HR:%d SpO2:%d%%",
                        max30102.getLastHeartRate(),
                        max30102.getLastSpO2()
                    );
                } else if (max30102.isLastReadingValid()) {
                    snprintf(line1, sizeof(line1), "HR:%d bpm", max30102.getLastHeartRate());
                } else {
                    snprintf(line1, sizeof(line1), "SpO2:%d%%", max30102.getLastSpO2());
                }
            } else {
                snprintf(line1, sizeof(line1), "Finger on MAX30102");
            }
            break;
        case 1:
            if (lm35.isBodyTemperatureValid()) {
                snprintf(line0, sizeof(line0), "Skin contact OK");
            } else {
                snprintf(line0, sizeof(line0), "Amb:%.1fC", lm35.getLastReading());
            }
            if (neo6m.isLastReadingValid()) {
                snprintf(line1, sizeof(line1), "Lat:%.4f", neo6m.getLastLatitude());
            } else if (neo6m.isReceivingData()) {
                snprintf(
                    line1,
                    sizeof(line1),
                    "GPS view:%d",
                    neo6m.getSatellitesInView() > 0 ? neo6m.getSatellitesInView() : neo6m.getSatelliteCount()
                );
            } else {
                snprintf(line1, sizeof(line1), "GPS: no signal");
            }
            break;
        default:
            if (neo6m.isLastReadingValid()) {
                snprintf(line0, sizeof(line0), "Lon:%.4f", neo6m.getLastLongitude());
                snprintf(line1, sizeof(line1), "GPS fix OK");
            } else if (neo6m.isReceivingData()) {
                snprintf(line0, sizeof(line0), "GPS searching");
                snprintf(
                    line1,
                    sizeof(line1),
                    "View:%d Fix:%d",
                    neo6m.getSatellitesInView(),
                    neo6m.getSatelliteCount()
                );
            } else {
                snprintf(line0, sizeof(line0), "GPS no data");
                snprintf(line1, sizeof(line1), "Check TX/RX pins");
            }
            break;
    }

    lcdPage++;
    lcd.setLine(0, line0);
    lcd.setLine(1, line1);
    lcd.refresh();
}

Lm35& VeyraDevice::getLm35() {
    return lm35;
}

Neo6m& VeyraDevice::getNeo6m() {
    return neo6m;
}

Max30102& VeyraDevice::getMax30102() {
    return max30102;
}

Lcd1602& VeyraDevice::getLcd() {
    return lcd;
}

void VeyraDevice::readTemperature() {
    lm35.readAndTriggerEvent();
}

void VeyraDevice::updateGps() {
    neo6m.update();
}

void VeyraDevice::updateMax30102() {
    max30102.update();
}
