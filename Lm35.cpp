/**
 * @file Lm35.cpp
 * @brief Implements the Lm35 class.
 *
 * Reads linear centigrade temperature from an LM35 analog output in the Modest IoT Nano-framework.
 * Event generation (TEMPERATURE_READ_EVENT) is typically triggered externally via polling in user code.
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

#include "Lm35.h"
#include <Arduino.h>

const Event Lm35::TEMPERATURE_READ_EVENT = Event(TEMPERATURE_READ_EVENT_ID);
const float Lm35::BODY_CONTACT_THRESHOLD_C = 30.0f;
const float Lm35::BODY_TEMP_MAX_C = 42.0f;

static const float ADC_VREF = 3.3f;
static const int ADC_MAX = 4095;
static const float LM35_MV_PER_C = 10.0f;

Lm35::Lm35(int pin, EventHandler* eventHandler)
    : Sensor(pin, eventHandler), lastReading(0.0f) {}

void Lm35::begin() {
    pinMode(pin, INPUT);
#if defined(ESP32)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
#endif
}

float Lm35::readTemperatureCelsius() {
#if defined(ESP32)
    // Use the ESP32 factory (eFuse) ADC calibration to read true millivolts.
    // The previous raw * VREF / 4095 conversion assumed a perfect 3.3 V, linear
    // ADC and read far too low in the LM35's low-mV band (ambient ~200 mV).
    long millivoltSum = 0;
    for (int i = 0; i < 4; i++) {
        millivoltSum += analogReadMilliVolts(pin);
        delay(2);
    }
    float millivolts = millivoltSum / 4.0f;
#else
    long rawSum = 0;
    for (int i = 0; i < 4; i++) {
        rawSum += analogRead(pin);
        delay(2);
    }
    float millivolts = ((rawSum / 4.0f) * ADC_VREF * 1000.0f) / ADC_MAX;
#endif
    return millivolts / LM35_MV_PER_C;
}

float Lm35::getLastReading() const {
    return lastReading;
}

bool Lm35::isBodyTemperatureValid() const {
    return lastReading >= BODY_CONTACT_THRESHOLD_C && lastReading <= BODY_TEMP_MAX_C;
}

void Lm35::readAndTriggerEvent() {
    lastReading = readTemperatureCelsius();
    on(TEMPERATURE_READ_EVENT);
}
