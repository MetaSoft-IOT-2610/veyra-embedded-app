/**
 * @file Neo6m.cpp
 * @brief Implements the Neo6m class.
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

#include "Neo6m.h"
#include <Arduino.h>
#include <cstring>

const Event Neo6m::LOCATION_READ_EVENT = Event(LOCATION_READ_EVENT_ID);

static bool fieldHasValue(const char* field) {
    return field != nullptr && field[0] != '\0';
}

static char hemisphereChar(const char* field) {
    return fieldHasValue(field) ? field[0] : '\0';
}

static const long GPS_BAUD_RATES[] = {9600, 115200, 4800};
static const int GPS_BAUD_COUNT = 3;
static const unsigned long GPS_PROBE_MS = 1000;

Neo6m::Neo6m(int rxPin, int txPin, EventHandler* eventHandler)
    : Sensor(rxPin, eventHandler),
      txPin(txPin),
      serial(Serial2),
      lastLatitude(0.0f),
      lastLongitude(0.0f),
      lastReadingValid(false),
      receivingData(false),
      satelliteCount(0),
      satellitesInView(0),
      bytesReceived(0),
      activeBaudRate(9600),
      lineIndex(0) {}

void Neo6m::begin() {
    receivingData = false;
    bytesReceived = 0;
    lineIndex = 0;

    for (int i = 0; i < GPS_BAUD_COUNT; i++) {
        long baud = GPS_BAUD_RATES[i];
        if (i > 0) {
            serial.end();
            delay(50);
        }

        serial.begin(baud, SERIAL_8N1, pin, txPin);
        serial.setTimeout(20);
        activeBaudRate = baud;

        unsigned long probeStart = millis();
        while (millis() - probeStart < GPS_PROBE_MS) {
            while (serial.available()) {
                char c = serial.read();
                bytesReceived++;
                if (c == '$') {
                    receivingData = true;
                }
            }
        }

        if (receivingData) {
            return;
        }
    }
}

void Neo6m::update() {
    while (serial.available()) {
        char c = serial.read();
        receivingData = true;
        bytesReceived++;

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            lineBuffer[lineIndex] = '\0';
            if (lineIndex > 0 && parseNmeaSentence(lineBuffer)) {
                on(LOCATION_READ_EVENT);
            }
            lineIndex = 0;
            continue;
        }

        if (lineIndex < static_cast<int>(sizeof(lineBuffer)) - 1) {
            lineBuffer[lineIndex++] = c;
        } else {
            lineIndex = 0;
        }
    }
}

float Neo6m::getLastLatitude() const {
    return lastLatitude;
}

float Neo6m::getLastLongitude() const {
    return lastLongitude;
}

bool Neo6m::isLastReadingValid() const {
    return lastReadingValid;
}

bool Neo6m::isReceivingData() const {
    return receivingData;
}

int Neo6m::getSatelliteCount() const {
    return satelliteCount;
}

int Neo6m::getSatellitesInView() const {
    return satellitesInView;
}

unsigned long Neo6m::getBytesReceived() const {
    return bytesReceived;
}

long Neo6m::getActiveBaudRate() const {
    return activeBaudRate;
}

bool Neo6m::parseNmeaSentence(const char* sentence) {
    if (sentence[0] != '$') {
        return false;
    }

    if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        return parseRmcSentence(sentence);
    }

    if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        return parseGgaSentence(sentence);
    }

    if (strncmp(sentence, "$GPGSV", 6) == 0 || strncmp(sentence, "$GNGSV", 6) == 0) {
        parseGsvSentence(sentence);
        return false;
    }

    return false;
}

bool Neo6m::parseRmcSentence(const char* sentence) {
    char buffer[128];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* fields[12] = {nullptr};
    int fieldCount = 0;
    char* token = strtok(buffer, ",");

    while (token != nullptr && fieldCount < 12) {
        fields[fieldCount++] = token;
        token = strtok(nullptr, ",");
    }

    if (fieldCount < 7 || !fieldHasValue(fields[2]) || fields[2][0] != 'A') {
        return false;
    }

    if (!fieldHasValue(fields[3]) || !fieldHasValue(fields[5])) {
        return false;
    }

    lastLatitude = nmeaToDecimalDegrees(fields[3], hemisphereChar(fields[4]));
    lastLongitude = nmeaToDecimalDegrees(fields[5], hemisphereChar(fields[6]));
    lastReadingValid = true;
    return true;
}

bool Neo6m::parseGgaSentence(const char* sentence) {
    char buffer[128];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* fields[12] = {nullptr};
    int fieldCount = 0;
    char* token = strtok(buffer, ",");

    while (token != nullptr && fieldCount < 12) {
        fields[fieldCount++] = token;
        token = strtok(nullptr, ",");
    }

    if (fieldCount < 8) {
        return false;
    }

    if (fieldHasValue(fields[7])) {
        satelliteCount = atoi(fields[7]);
    }

    int fixQuality = fieldHasValue(fields[6]) ? atoi(fields[6]) : 0;
    if (fixQuality < 1) {
        lastReadingValid = false;
        return false;
    }

    if (!fieldHasValue(fields[2]) || !fieldHasValue(fields[4])) {
        return false;
    }

    lastLatitude = nmeaToDecimalDegrees(fields[2], hemisphereChar(fields[3]));
    lastLongitude = nmeaToDecimalDegrees(fields[4], hemisphereChar(fields[5]));
    lastReadingValid = true;
    return true;
}

void Neo6m::parseGsvSentence(const char* sentence) {
    char buffer[128];
    strncpy(buffer, sentence, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* fields[8] = {nullptr};
    int fieldCount = 0;
    char* token = strtok(buffer, ",");

    while (token != nullptr && fieldCount < 8) {
        fields[fieldCount++] = token;
        token = strtok(nullptr, ",");
    }

    if (fieldCount < 5) {
        return;
    }

    int messageNumber = fieldHasValue(fields[2]) ? atoi(fields[2]) : 0;
    if (messageNumber != 1) {
        return;
    }

    if (fieldHasValue(fields[3])) {
        satellitesInView = atoi(fields[3]);
    }
}

float Neo6m::nmeaToDecimalDegrees(const char* value, char direction) {
    float raw = atof(value);
    int degrees = static_cast<int>(raw / 100.0f);
    float minutes = raw - (degrees * 100.0f);
    float decimal = degrees + (minutes / 60.0f);

    if (direction == 'S' || direction == 'W') {
        decimal = -decimal;
    }

    return decimal;
}
