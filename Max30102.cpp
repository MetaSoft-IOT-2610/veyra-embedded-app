/**
 * @file Max30102.cpp
 * @brief Implements the Max30102 class.
 *
 * Polls the MAX30102 FIFO over I2C without using the INT pin.
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

#include "Max30102.h"
#include "spo2_algorithm.h"
#include <Arduino.h>
#include <Wire.h>

const Event Max30102::PPG_READ_EVENT = Event(PPG_READ_EVENT_ID);

static const uint8_t I2C_ADDRESS = 0x57;
static const uint8_t REG_INTR_STATUS_1 = 0x00;
static const uint8_t REG_FIFO_WR_PTR = 0x04;
static const uint8_t REG_FIFO_RD_PTR = 0x06;
static const uint8_t REG_FIFO_DATA = 0x07;
static const uint8_t REG_FIFO_CONFIG = 0x08;
static const uint8_t REG_MODE_CONFIG = 0x09;
static const uint8_t REG_SPO2_CONFIG = 0x0A;
static const uint8_t REG_LED1_PA = 0x0C;
static const uint8_t REG_LED2_PA = 0x0D;
static const uint8_t REG_INTR_ENABLE_1 = 0x02;
static const uint8_t REG_INTR_ENABLE_2 = 0x03;
static const uint8_t REG_PART_ID = 0xFF;

static const uint8_t EXPECTED_PART_ID = 0x15;
static const unsigned long REPORT_INTERVAL_MS = 1000;
static const int MIN_FINGER_IR = 12000;
static const int MAX_FINGER_IR = 240000;
static const int MIN_IR_VARIATION = 800;
static const int MIN_SAMPLES_FOR_METRICS = BUFFER_SIZE;
static const int SAMPLE_SHIFT = FreqS;

Max30102::Max30102(int sdaPin, int sclPin, EventHandler* eventHandler)
    : Sensor(sdaPin, eventHandler),
      sclPin(sclPin),
      initialized(false),
      lastHeartRate(0),
      lastSpO2(0),
      lastReadingValid(false),
      lastSpO2Valid(false),
      lastReportMillis(0),
      bufferCount(0),
      lastIrAverage(0),
      lastIrVariation(0),
      lastSignalSaturated(false),
      hrSmoothCount(0),
      spo2SmoothCount(0) {}

void Max30102::begin() {
    Wire.begin(pin, sclPin);
    Wire.setClock(400000);
    Wire.setTimeout(100);

    if (readRegister(REG_PART_ID) == EXPECTED_PART_ID && reset() && configure()) {
        initialized = true;
    }
}

void Max30102::update() {
    if (!initialized) {
        return;
    }

    readRegister(REG_INTR_STATUS_1);

    int samples = availableFifoSamples();
    for (int i = 0; i < samples; i++) {
        uint32_t red = 0;
        uint32_t ir = 0;
        if (!readFifoSample(red, ir)) {
            break;
        }
        appendSample(red, ir);
    }

    unsigned long now = millis();
    if (bufferCount >= MIN_SAMPLES_FOR_METRICS && now - lastReportMillis >= REPORT_INTERVAL_MS) {
        calculateMetrics();
        shiftSampleWindow();
        lastReportMillis = now;
        on(PPG_READ_EVENT);
    }
}

int Max30102::getLastHeartRate() const {
    return lastHeartRate;
}

int Max30102::getLastSpO2() const {
    return lastSpO2;
}

bool Max30102::isLastReadingValid() const {
    return lastReadingValid;
}

bool Max30102::isLastSpO2Valid() const {
    return lastSpO2Valid;
}

bool Max30102::isInitialized() const {
    return initialized;
}

uint32_t Max30102::getLastIrAverage() const {
    return lastIrAverage;
}

uint32_t Max30102::getLastIrVariation() const {
    return lastIrVariation;
}

bool Max30102::isFingerDetected() const {
    return lastIrAverage >= static_cast<uint32_t>(MIN_FINGER_IR);
}

bool Max30102::isSignalSaturated() const {
    return lastSignalSaturated;
}

bool Max30102::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

uint8_t Max30102::readRegister(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }

    if (Wire.requestFrom(I2C_ADDRESS, static_cast<uint8_t>(1)) != 1) {
        return 0;
    }

    return Wire.read();
}

bool Max30102::reset() {
    if (!writeRegister(REG_MODE_CONFIG, 0x40)) {
        return false;
    }
    delay(100);
    return true;
}

bool Max30102::configure() {
    writeRegister(REG_INTR_ENABLE_1, 0x00);
    writeRegister(REG_INTR_ENABLE_2, 0x00);
    writeRegister(REG_FIFO_WR_PTR, 0x00);
    writeRegister(REG_FIFO_RD_PTR, 0x00);
    writeRegister(REG_FIFO_CONFIG, 0x4F); // 4-sample average -> 25 effective sps at 100 Hz
    writeRegister(REG_MODE_CONFIG, 0x03); // SpO2 mode (red + IR)
    writeRegister(REG_SPO2_CONFIG, 0x27); // 100 sps, 4096 nA, 411 us pulse
    writeRegister(REG_LED1_PA, 0x28);
    writeRegister(REG_LED2_PA, 0x28);
    return true;
}

int Max30102::availableFifoSamples() {
    uint8_t writePtr = readRegister(REG_FIFO_WR_PTR);
    uint8_t readPtr = readRegister(REG_FIFO_RD_PTR);
    return (writePtr - readPtr) & 0x1F;
}

bool Max30102::readFifoSample(uint32_t& red, uint32_t& ir) {
    Wire.beginTransmission(I2C_ADDRESS);
    Wire.write(REG_FIFO_DATA);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    if (Wire.requestFrom(I2C_ADDRESS, static_cast<uint8_t>(6)) != 6) {
        return false;
    }

    uint8_t redBytes[3];
    uint8_t irBytes[3];
    for (int i = 0; i < 3; i++) {
        redBytes[i] = Wire.read();
    }
    for (int i = 0; i < 3; i++) {
        irBytes[i] = Wire.read();
    }

    red = ((redBytes[0] << 16) | (redBytes[1] << 8) | redBytes[2]) & 0x03FFFF;
    ir = ((irBytes[0] << 16) | (irBytes[1] << 8) | irBytes[2]) & 0x03FFFF;
    return true;
}

void Max30102::appendSample(uint32_t red, uint32_t ir) {
    if (bufferCount < SAMPLE_BUFFER_SIZE) {
        redBuffer[bufferCount] = red;
        irBuffer[bufferCount] = ir;
        bufferCount++;
        return;
    }

    for (int i = 1; i < SAMPLE_BUFFER_SIZE; i++) {
        redBuffer[i - 1] = redBuffer[i];
        irBuffer[i - 1] = irBuffer[i];
    }

    redBuffer[SAMPLE_BUFFER_SIZE - 1] = red;
    irBuffer[SAMPLE_BUFFER_SIZE - 1] = ir;
}

void Max30102::shiftSampleWindow() {
    for (int i = SAMPLE_SHIFT; i < SAMPLE_BUFFER_SIZE; i++) {
        irBuffer[i - SAMPLE_SHIFT] = irBuffer[i];
        redBuffer[i - SAMPLE_SHIFT] = redBuffer[i];
    }
    bufferCount = SAMPLE_BUFFER_SIZE - SAMPLE_SHIFT;
}

void Max30102::resetSmoothing() {
    hrSmoothCount = 0;
    spo2SmoothCount = 0;
}

int Max30102::averageSamples(const int* samples, int count) {
    if (count <= 0) {
        return 0;
    }

    long sum = 0;
    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    return static_cast<int>((sum + count / 2) / count);
}

bool Max30102::smoothHeartRate(int rawHr, int& smoothedHr) {
    if (hrSmoothCount > 0) {
        int avg = averageSamples(hrSmoothBuffer, hrSmoothCount);
        if (abs(rawHr - avg) > HR_OUTLIER_DELTA) {
            smoothedHr = avg;
            return hrSmoothCount >= 2;
        }
    }

    if (hrSmoothCount < SMOOTH_WINDOW_SIZE) {
        hrSmoothBuffer[hrSmoothCount++] = rawHr;
    } else {
        for (int i = 1; i < SMOOTH_WINDOW_SIZE; i++) {
            hrSmoothBuffer[i - 1] = hrSmoothBuffer[i];
        }
        hrSmoothBuffer[SMOOTH_WINDOW_SIZE - 1] = rawHr;
    }

    smoothedHr = averageSamples(hrSmoothBuffer, hrSmoothCount);
    return true;
}

bool Max30102::smoothSpO2(int rawSpO2, int& smoothedSpO2) {
    if (spo2SmoothCount > 0) {
        int avg = averageSamples(spo2SmoothBuffer, spo2SmoothCount);
        if (abs(rawSpO2 - avg) > SPO2_OUTLIER_DELTA) {
            smoothedSpO2 = avg;
            return spo2SmoothCount >= 2;
        }
    }

    if (spo2SmoothCount < SMOOTH_WINDOW_SIZE) {
        spo2SmoothBuffer[spo2SmoothCount++] = rawSpO2;
    } else {
        for (int i = 1; i < SMOOTH_WINDOW_SIZE; i++) {
            spo2SmoothBuffer[i - 1] = spo2SmoothBuffer[i];
        }
        spo2SmoothBuffer[SMOOTH_WINDOW_SIZE - 1] = rawSpO2;
    }

    smoothedSpO2 = averageSamples(spo2SmoothBuffer, spo2SmoothCount);
    return true;
}

void Max30102::calculateMetrics() {
    uint32_t irSum = 0;
    uint32_t irMin = UINT32_MAX;
    uint32_t irMax = 0;

    for (int i = 0; i < bufferCount; i++) {
        irSum += irBuffer[i];
        if (irBuffer[i] < irMin) {
            irMin = irBuffer[i];
        }
        if (irBuffer[i] > irMax) {
            irMax = irBuffer[i];
        }
    }

    lastIrAverage = irSum / bufferCount;
    lastIrVariation = irMax - irMin;
    lastSignalSaturated = false;

    if (lastIrAverage < static_cast<uint32_t>(MIN_FINGER_IR)) {
        resetSmoothing();
        lastReadingValid = false;
        lastSpO2Valid = false;
        lastHeartRate = 0;
        lastSpO2 = 0;
        return;
    }

    if (lastIrAverage > static_cast<uint32_t>(MAX_FINGER_IR) ||
        lastIrVariation < static_cast<uint32_t>(MIN_IR_VARIATION)) {
        resetSmoothing();
        lastSignalSaturated = true;
        lastReadingValid = false;
        lastSpO2Valid = false;
        lastHeartRate = 0;
        lastSpO2 = 0;
        return;
    }

    lastSignalSaturated = false;

    int32_t spo2 = 0;
    int8_t spo2Valid = 0;
    int32_t heartRate = 0;
    int8_t hrValid = 0;

    maxim_heart_rate_and_oxygen_saturation(
        irBuffer,
        bufferCount,
        redBuffer,
        &spo2,
        &spo2Valid,
        &heartRate,
        &hrValid
    );

    bool rawHrOk = (hrValid != 0 && heartRate >= 40 && heartRate <= 200);
    bool rawSpO2Ok = (spo2Valid != 0 && spo2 >= 0 && spo2 <= 100);

    if (rawHrOk) {
        int smoothedHr = 0;
        if (smoothHeartRate(static_cast<int>(heartRate), smoothedHr)) {
            lastHeartRate = smoothedHr;
            lastReadingValid = true;
        } else if (hrSmoothCount >= 2) {
            lastHeartRate = averageSamples(hrSmoothBuffer, hrSmoothCount);
            lastReadingValid = true;
        } else {
            lastReadingValid = false;
            lastHeartRate = 0;
        }
    } else if (hrSmoothCount >= 2) {
        lastHeartRate = averageSamples(hrSmoothBuffer, hrSmoothCount);
        lastReadingValid = true;
    } else {
        lastReadingValid = false;
        lastHeartRate = 0;
    }

    if (rawSpO2Ok) {
        int smoothedSpO2 = 0;
        if (smoothSpO2(static_cast<int>(spo2), smoothedSpO2)) {
            lastSpO2 = smoothedSpO2;
            lastSpO2Valid = true;
        } else if (spo2SmoothCount >= 2) {
            lastSpO2 = averageSamples(spo2SmoothBuffer, spo2SmoothCount);
            lastSpO2Valid = true;
        } else {
            lastSpO2Valid = false;
            lastSpO2 = 0;
        }
    } else if (spo2SmoothCount >= 2) {
        lastSpO2 = averageSamples(spo2SmoothBuffer, spo2SmoothCount);
        lastSpO2Valid = true;
    } else {
        lastSpO2Valid = false;
        lastSpO2 = 0;
    }
}
