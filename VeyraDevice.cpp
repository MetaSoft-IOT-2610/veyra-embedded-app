/**
 * @file VeyraDevice.cpp
 * @brief Implements the VeyraDevice class.
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

#include "VeyraDevice.h"
#include <Arduino.h>
#include <stdio.h>
#include <WiFi.h>

static const char* max30102DiagnosticStatus(const Max30102& max30102) {
    switch (max30102.getPhase()) {
        case Max30102::PpgPhase::NotDetected:
            return "not_detected";
        case Max30102::PpgPhase::PressTooHard:
            return "signal_saturated";
        case Max30102::PpgPhase::WaitingFinger:
            return "waiting_for_finger";
        case Max30102::PpgPhase::Ready:
            return "ok";
        default:
            return "measuring";
    }
}

static const char* lm35DiagnosticStatus(bool bodyContact) {
    return bodyContact ? "body_contact_ok" : "ambient_only";
}

static const char* gpsDiagnosticStatus(bool receivingNmea, bool fixValid, int satellitesInView) {
    if (!receivingNmea) {
        return "no_data";
    }
    if (fixValid) {
        return "fix_ok";
    }
    if (satellitesInView > 0) {
        return "searching_fix";
    }
    return "no_signal";
}

static SensorDiagnostics buildSensorDiagnostics(
    const Max30102& max30102,
    const Lm35& lm35,
    const Neo6m& neo6m,
    const Lcd1602& lcd,
    const EdgeHttpClient& edgeHttp
) {
    SensorDiagnostics diagnostics = {};
    diagnostics.max30102Initialized = max30102.isInitialized();
    diagnostics.max30102HeartRateValid = max30102.isLastReadingValid();
    diagnostics.max30102SpO2Valid = max30102.isLastSpO2Valid();
    diagnostics.max30102FingerDetected = max30102.isFingerDetected();
    diagnostics.max30102SignalSaturated = max30102.isSignalSaturated();
    diagnostics.max30102IrAverage = max30102.getLastIrAverage();
    diagnostics.max30102IrVariation = max30102.getLastIrVariation();
    diagnostics.max30102Status = max30102DiagnosticStatus(max30102);

    diagnostics.lm35ReadingCelsius = lm35.getLastReading();
    diagnostics.lm35BodyContact = lm35.isBodyTemperatureValid();
    diagnostics.lm35Status = lm35DiagnosticStatus(diagnostics.lm35BodyContact);

    diagnostics.gpsReceivingNmea = neo6m.isReceivingData();
    diagnostics.gpsFixValid = neo6m.isLastReadingValid();
    diagnostics.gpsSatelliteCount = neo6m.getSatelliteCount();
    diagnostics.gpsSatellitesInView = neo6m.getSatellitesInView();
    diagnostics.gpsBytesReceived = neo6m.getBytesReceived();
    diagnostics.gpsBaudRate = neo6m.getActiveBaudRate();
    diagnostics.gpsStatus = gpsDiagnosticStatus(
        diagnostics.gpsReceivingNmea,
        diagnostics.gpsFixValid,
        diagnostics.gpsSatellitesInView
    );

    diagnostics.lcdInitialized = lcd.isInitialized();
    diagnostics.wifiConnected = edgeHttp.isConnected();
    diagnostics.wifiRssiDbm = diagnostics.wifiConnected ? WiFi.RSSI() : 0;

    return diagnostics;
}

namespace {

const char* ppgPhaseMessage(Max30102::PpgPhase phase) {
    switch (phase) {
        case Max30102::PpgPhase::NotDetected:
            return "sensor no detectado - revisa cableado";
        case Max30102::PpgPhase::WarmingUp:
            return "iniciando, espera unos segundos";
        case Max30102::PpgPhase::WaitingFinger:
            return "apoya el dedo en el sensor";
        case Max30102::PpgPhase::Measuring:
            return "midiendo, manten el dedo quieto";
        case Max30102::PpgPhase::PressTooHard:
            return "presiona menos el dedo";
        default:
            return nullptr;
    }
}

const char* ppgPhaseLcdHint(Max30102::PpgPhase phase) {
    switch (phase) {
        case Max30102::PpgPhase::NotDetected:
            return "Sin sensor";
        case Max30102::PpgPhase::WarmingUp:
            return "Espera...";
        case Max30102::PpgPhase::WaitingFinger:
            return "Apoya el dedo";
        case Max30102::PpgPhase::Measuring:
            return "Midiendo...";
        case Max30102::PpgPhase::PressTooHard:
            return "Menos presion";
        default:
            return nullptr;
    }
}

void printSerialReadings(const Lm35& lm35, const Max30102& max30102, const Neo6m& neo6m) {
    Serial.println();
    Serial.println(F("------ Lecturas Veyra ------"));

    if (lm35.isBodyTemperatureValid()) {
        Serial.printf("Temperatura piel:     %.1f C\n", lm35.getLastReading());
    } else {
        Serial.printf("Temperatura ambiente: %.1f C\n", lm35.getLastReading());
        Serial.println(F("Temperatura piel:     - apoya el sensor en la piel"));
    }

    if (max30102.isLastReadingValid()) {
        Serial.printf("Pulso:                %d lat/min\n", max30102.getLastHeartRate());
    }
    if (max30102.isLastSpO2Valid()) {
        Serial.printf("Oxigeno (SpO2):       %d %%\n", max30102.getLastSpO2());
    }
    if (!max30102.isLastReadingValid() && !max30102.isLastSpO2Valid()) {
        const char* hint = ppgPhaseMessage(max30102.getPhase());
        if (hint != nullptr) {
            Serial.printf("Pulso / SpO2:         %s\n", hint);
        }
    }

    if (neo6m.isLastReadingValid()) {
        Serial.printf("Ubicacion GPS:        %.5f, %.5f\n", neo6m.getLastLatitude(), neo6m.getLastLongitude());
        Serial.printf("Satelites GPS:        %d en uso\n", neo6m.getSatelliteCount());
    } else if (neo6m.isReceivingData()) {
        if (neo6m.getSatellitesInView() > 0) {
            Serial.printf(
                "GPS:                  buscando senal (%d en uso, %d visibles)\n",
                neo6m.getSatelliteCount(),
                neo6m.getSatellitesInView()
            );
        } else {
            Serial.println(F("GPS:                  buscando senal - prueba al aire libre"));
        }
    } else {
        Serial.println(F("GPS:                  sin datos - revisa conexion TX/RX"));
    }

    Serial.println(F("----------------------------"));
}

void formatLcdPage(
    uint8_t page,
    const Lm35& lm35,
    const Max30102& max30102,
    const Neo6m& neo6m,
    char* line0,
    char* line1
) {
    switch (page % 3) {
        case 0:
            if (lm35.isBodyTemperatureValid()) {
                snprintf(line0, 17, "Piel: %.1f C", lm35.getLastReading());
            } else {
                snprintf(line0, 17, "Amb: %.1f C", lm35.getLastReading());
            }
            if (max30102.isLastReadingValid() && max30102.isLastSpO2Valid()) {
                snprintf(
                    line1,
                    17,
                    "%d/min SpO2 %d",
                    max30102.getLastHeartRate(),
                    max30102.getLastSpO2()
                );
            } else if (max30102.isLastReadingValid()) {
                snprintf(line1, 17, "Pulso %d/min", max30102.getLastHeartRate());
            } else if (max30102.isLastSpO2Valid()) {
                snprintf(line1, 17, "SpO2: %d %%", max30102.getLastSpO2());
            } else {
                const char* hint = ppgPhaseLcdHint(max30102.getPhase());
                snprintf(line1, 17, "%s", hint != nullptr ? hint : "...");
            }
            break;
        case 1:
            if (lm35.isBodyTemperatureValid()) {
                snprintf(line0, 17, "Piel: %.1f C", lm35.getLastReading());
            } else {
                snprintf(line0, 17, "Amb: %.1f C", lm35.getLastReading());
                snprintf(line1, 17, "Apoya en piel");
            }
            if (lm35.isBodyTemperatureValid()) {
                if (max30102.getPhase() == Max30102::PpgPhase::Ready) {
                    snprintf(line1, 17, "Lecturas OK");
                } else {
                    const char* hint = ppgPhaseLcdHint(max30102.getPhase());
                    snprintf(line1, 17, "%s", hint != nullptr ? hint : "...");
                }
            }
            break;
        default:
            if (neo6m.isLastReadingValid()) {
                snprintf(line0, 17, "GPS: ubicacion");
                snprintf(line1, 17, "%d satelites", neo6m.getSatelliteCount());
            } else if (neo6m.isReceivingData()) {
                snprintf(line0, 17, "GPS: buscando");
                snprintf(
                    line1,
                    17,
                    "%d visibles",
                    neo6m.getSatellitesInView() > 0 ? neo6m.getSatellitesInView() : neo6m.getSatelliteCount()
                );
            } else {
                snprintf(line0, 17, "GPS: sin senal");
                snprintf(line1, 17, "Revisa cableado");
            }
            break;
    }
}

EdgeHttpClient* g_edgeHttpClient = nullptr;

void suspendWifiForMax30102() {
    if (g_edgeHttpClient != nullptr) {
        g_edgeHttpClient->suspendWifi();
    }
}

bool resumeWifiForMax30102() {
    return g_edgeHttpClient != nullptr && g_edgeHttpClient->resumeWifi();
}

}

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
    g_edgeHttpClient = &edgeHttp;
    Max30102::setWifiCoexistenceHooks(suspendWifiForMax30102, resumeWifiForMax30102);

    lm35.begin();
    neo6m.begin();
    lcd.begin();
    max30102.begin();

    edgeHttp.begin();
    max30102.onWifiReady();

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
    if (event == Max30102::PPG_READ_EVENT
        || event == Lm35::TEMPERATURE_READ_EVENT
        || event == Neo6m::LOCATION_READ_EVENT) {
        maybePublishTelemetry();
    }
}

void VeyraDevice::handle(Command command) {
    if (command == Lcd1602::REFRESH_COMMAND) {
        return;
    }
}

void VeyraDevice::refreshStatus() {
    char line0[17] = "";
    char line1[17] = "";

    printSerialReadings(lm35, max30102, neo6m);
    formatLcdPage(lcdPage, lm35, max30102, neo6m, line0, line1);

    lcdPage++;
    lcd.setLine(0, line0);
    lcd.setLine(1, line1);
    lcd.refresh();

    maybePublishTelemetry();
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

EdgeHttpClient& VeyraDevice::getEdgeHttp() {
    return edgeHttp;
}

void VeyraDevice::maybePublishTelemetry() {
    TelemetrySnapshot snapshot = {};
    snapshot.heartRate = max30102.getLastHeartRate();
    snapshot.heartRateValid = max30102.isLastReadingValid();
    snapshot.oxygenSaturation = max30102.getLastSpO2();
    snapshot.oxygenSaturationValid = max30102.isLastSpO2Valid();

    const float lm35Reading = lm35.getLastReading();
    if (lm35.isBodyTemperatureValid()) {
        snapshot.bodyTemperatureCelsius = lm35Reading;
        snapshot.bodyTemperatureValid = true;
    } else {
        snapshot.ambientTemperatureCelsius = lm35Reading;
        snapshot.ambientTemperatureValid = true;
    }

    if (neo6m.isLastReadingValid()) {
        snapshot.latitude = neo6m.getLastLatitude();
        snapshot.longitude = neo6m.getLastLongitude();
        snapshot.locationValid = true;
    }

    if (neo6m.isReceivingData()) {
        snapshot.satelliteCount = neo6m.getSatelliteCount();
        snapshot.satelliteCountValid = true;
        snapshot.satellitesInView = neo6m.getSatellitesInView();
        snapshot.satellitesInViewValid = true;
    }

    edgeHttp.publishSnapshot(snapshot, buildSensorDiagnostics(max30102, lm35, neo6m, lcd, edgeHttp));
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
