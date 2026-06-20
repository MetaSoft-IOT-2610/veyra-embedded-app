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

void (*Max30102::wifiSuspendFn)() = nullptr;
bool (*Max30102::wifiResumeFn)() = nullptr;

static const uint8_t I2C_ADDRESS = 0x57;
static const uint8_t REG_INTR_STATUS_1 = 0x00;
static const uint8_t REG_FIFO_WR_PTR = 0x04;
static const uint8_t REG_OVF_COUNTER = 0x05;
static const uint8_t REG_FIFO_RD_PTR = 0x06;
static const uint8_t REG_FIFO_DATA = 0x07;
static const uint8_t REG_FIFO_CONFIG = 0x08;
static const uint8_t REG_MODE_CONFIG = 0x09;
static const uint8_t REG_SPO2_CONFIG = 0x0A;
static const uint8_t REG_LED1_PA = 0x0C;
static const uint8_t REG_LED2_PA = 0x0D;
static const uint8_t REG_PILOT_PA = 0x10;
static const uint8_t REG_INTR_ENABLE_1 = 0x02;
static const uint8_t REG_INTR_ENABLE_2 = 0x03;
static const uint8_t REG_PART_ID = 0xFF;

static const uint8_t EXPECTED_PART_ID = 0x15;
// SparkFun / Maxim wearable defaults: 100 sps, 4-sample FIFO avg (~25 Hz), 4096 nA ADC, 411 us pulse.
static const uint8_t FIFO_CONFIG_VALUE = 0x4F;
static const uint8_t MODE_SPO2_VALUE = 0x03;
static const uint8_t SPO2_CONFIG_VALUE = 0x27;
static const uint8_t LED_PULSE_AMPLITUDE = 0x50; // ~16 mA; lower if IR clips at 262143 when pressing hard.
static const unsigned long I2C_READ_TIMEOUT_MS = 50;
static const unsigned long REPORT_INTERVAL_MS = 1000;
static const int MIN_FINGER_IR = 5000;
static const int MAX_FINGER_IR = 240000;
static const int MIN_IR_VARIATION = 400;
static const int CHANNEL_DETECT_MIN_SAMPLES = 25;
static const int MIN_SAMPLES_FOR_METRICS = BUFFER_SIZE;
static const int SAMPLE_SHIFT = FreqS;

namespace {
// Post-processing after Maxim MAXREFDES117 algorithm (peak distance in spo2_algorithm.h).
constexpr int HR_HARMONIC_CORRECT_MIN = 118;
constexpr int SPO2_CALIBRATION_OFFSET = -3;
constexpr uint32_t TARGET_IR_AC_MIN = 600;
constexpr uint32_t TARGET_IR_AC_MAX = 50000;
}

Max30102::Max30102(int sdaPin, int sclPin, EventHandler* eventHandler)
    : Sensor(sdaPin, eventHandler),
      sclPin(sclPin),
      i2cBus(&Wire),
      activeSdaPin(sdaPin),
      activeSclPin(sclPin),
      lastFifoPending(0),
      initialized(false),
      lastHeartRate(0),
      lastSpO2(0),
      lastReadingValid(false),
      lastSpO2Valid(false),
      lastReportMillis(0),
      bufferCount(0),
      swapRedIrChannels(false),
      channelsMapped(false),
      lastIrAverage(0),
      lastRedAverage(0),
      lastIrVariation(0),
      lastSignalSaturated(false),
      hrSmoothCount(0),
      spo2SmoothCount(0) {}

void Max30102::setWifiCoexistenceHooks(WifiSuspendHook suspend, WifiResumeHook resume) {
    wifiSuspendFn = suspend;
    wifiResumeFn = resume;
}

bool Max30102::usesAdc2Bus() const {
    return activeSdaPin == 32 || activeSdaPin == 33
        || activeSclPin == 32 || activeSclPin == 33;
}

void Max30102::beginBusSession() {
    if (usesAdc2Bus() && wifiSuspendFn != nullptr) {
        wifiSuspendFn();
    }

    i2cBus->begin(activeSdaPin, activeSclPin);
    i2cBus->setClock(100000);
    i2cBus->setTimeout(1000);
}

void Max30102::endBusSession() {
    if (usesAdc2Bus() && wifiResumeFn != nullptr) {
        wifiResumeFn();
    }
}

void Max30102::begin() {
    initialized = false;
    lastFifoPending = 0;

    struct BusCandidate {
        TwoWire* bus;
        int sda;
        int scl;
        const char* label;
    };

    const BusCandidate candidates[] = {
        {&Wire, pin, sclPin, "Wire config"},
        {&Wire, sclPin, pin, "Wire swapped"},
        {&Wire1, 21, 22, "Wire1 LCD bus"},
        {&Wire1, 22, 21, "Wire1 swapped"},
    };

    bool found = false;
    for (const BusCandidate& candidate : candidates) {
        if (!probePartId(*candidate.bus, candidate.sda, candidate.scl)) {
            continue;
        }

        i2cBus = candidate.bus;
        activeSdaPin = candidate.sda;
        activeSclPin = candidate.scl;
        found = true;
        Serial.printf("Sensor pulso: detectado (I2C SDA%d SCL%d)\n", activeSdaPin, activeSclPin);
        break;
    }

    if (!found) {
        Serial.println(F("Sensor pulso: no detectado - revisa cableado I2C"));
        return;
    }

    i2cBus->begin(activeSdaPin, activeSclPin);
    i2cBus->setClock(100000);
    i2cBus->setTimeout(1000);

    if (!reset() || !configure()) {
        Serial.println(F("Sensor pulso: error de inicializacion"));
        return;
    }

    if (!wakeSensor()) {
        return;
    }

    initialized = true;
}

void Max30102::onWifiReady() {
    if (!initialized) {
        return;
    }

    beginBusSession();
    wakeSensor();
    drainFifo();
    refreshIrDiagnostics();
    endBusSession();

}

void Max30102::update() {
    if (!initialized) {
        return;
    }

    beginBusSession();

    readRegister(REG_INTR_STATUS_1);

    drainFifo();
    refreshIrDiagnostics();

    if (!channelsMapped && bufferCount >= CHANNEL_DETECT_MIN_SAMPLES) {
        detectChannelMapping();
    }

    unsigned long now = millis();
    if (bufferCount >= MIN_SAMPLES_FOR_METRICS && now - lastReportMillis >= REPORT_INTERVAL_MS) {
        calculateMetrics();
        shiftSampleWindow();
        lastReportMillis = now;
        on(PPG_READ_EVENT);
    }

    endBusSession();
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

int Max30102::getBufferCount() const {
    return bufferCount;
}

int Max30102::getActiveSdaPin() const {
    return activeSdaPin;
}

int Max30102::getActiveSclPin() const {
    return activeSclPin;
}

uint8_t Max30102::getLastFifoPending() const {
    return lastFifoPending;
}

uint32_t Max30102::getLastIrAverage() const {
    return lastIrAverage;
}

uint32_t Max30102::getLastRedAverage() const {
    return lastRedAverage;
}

uint32_t Max30102::getLastIrVariation() const {
    return lastIrVariation;
}

bool Max30102::isFingerDetected() const {
    const uint32_t signal = lastIrAverage > lastRedAverage ? lastIrAverage : lastRedAverage;
    return signal >= static_cast<uint32_t>(MIN_FINGER_IR);
}

bool Max30102::isSignalSaturated() const {
    return lastSignalSaturated;
}

Max30102::PpgPhase Max30102::getPhase() const {
    if (!initialized) {
        return PpgPhase::NotDetected;
    }
    if (isSignalSaturated()) {
        return PpgPhase::PressTooHard;
    }
    if (!isFingerDetected()) {
        return bufferCount < MIN_SAMPLES_FOR_METRICS ? PpgPhase::WarmingUp : PpgPhase::WaitingFinger;
    }
    if (lastReadingValid || lastSpO2Valid) {
        return PpgPhase::Ready;
    }
    return PpgPhase::Measuring;
}

bool Max30102::writeRegister(uint8_t reg, uint8_t value) {
    if (i2cBus == nullptr) {
        return false;
    }

    i2cBus->beginTransmission(I2C_ADDRESS);
    i2cBus->write(reg);
    i2cBus->write(value);
    return i2cBus->endTransmission() == 0;
}

uint8_t Max30102::readRegister(uint8_t reg) {
    uint8_t value = 0;
    if (!readBytes(reg, &value, 1)) {
        return 0;
    }
    return value;
}

bool Max30102::readBytes(uint8_t reg, uint8_t* buffer, size_t length) {
    if (i2cBus == nullptr || buffer == nullptr || length == 0) {
        return false;
    }

    i2cBus->beginTransmission(I2C_ADDRESS);
    i2cBus->write(reg);
    if (i2cBus->endTransmission(false) != 0) {
        return false;
    }

    i2cBus->requestFrom(I2C_ADDRESS, static_cast<uint8_t>(length));
    const unsigned long deadline = millis() + I2C_READ_TIMEOUT_MS;
    while (i2cBus->available() < static_cast<int>(length) && millis() < deadline) {
        delay(1);
    }

    if (i2cBus->available() < static_cast<int>(length)) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        buffer[i] = i2cBus->read();
    }
    return true;
}

bool Max30102::probePartId(TwoWire& bus, int sda, int scl) {
    bus.begin(sda, scl);
    bus.setClock(100000);
    bus.setTimeout(1000);
    delay(5);

    bus.beginTransmission(I2C_ADDRESS);
    bus.write(REG_PART_ID);
    if (bus.endTransmission(false) != 0) {
        return false;
    }

    bus.requestFrom(I2C_ADDRESS, static_cast<uint8_t>(1));
    const unsigned long deadline = millis() + I2C_READ_TIMEOUT_MS;
    while (bus.available() < 1 && millis() < deadline) {
        delay(1);
    }

    if (bus.available() < 1) {
        return false;
    }

    return bus.read() == EXPECTED_PART_ID;
}

bool Max30102::wakeSensor() {
    writeRegister(REG_MODE_CONFIG, MODE_SPO2_VALUE);
    delay(10);
    writeRegister(REG_MODE_CONFIG, MODE_SPO2_VALUE);

    const uint8_t fifoBefore = readRegister(REG_FIFO_WR_PTR);
    delay(250);
    const uint8_t fifoAfter = readRegister(REG_FIFO_WR_PTR);
    const uint8_t mode = readRegister(REG_MODE_CONFIG);

    if ((mode & 0x80) != 0) {
        Serial.println(F("Sensor pulso: sigue apagado tras encender"));
        return false;
    }

    if (fifoBefore == fifoAfter) {
        Serial.println(F("Sensor pulso: sin muestras - revisa dedo o cableado"));
    }

    return true;
}

bool Max30102::reset() {
    if (!writeRegister(REG_MODE_CONFIG, 0x40)) {
        return false;
    }

    for (int attempt = 0; attempt < 20; attempt++) {
        delay(10);
        if ((readRegister(REG_MODE_CONFIG) & 0x40) == 0) {
            break;
        }
    }

    delay(50);
    return true;
}

bool Max30102::flushFifo() {
    if (!writeRegister(REG_FIFO_WR_PTR, 0x00)) {
        return false;
    }
    if (!writeRegister(REG_OVF_COUNTER, 0x00)) {
        return false;
    }
    return writeRegister(REG_FIFO_RD_PTR, 0x00);
}

bool Max30102::configure() {
    writeRegister(REG_INTR_ENABLE_1, 0x00);
    writeRegister(REG_INTR_ENABLE_2, 0x00);

    writeRegister(REG_FIFO_CONFIG, FIFO_CONFIG_VALUE);
    writeRegister(REG_MODE_CONFIG, MODE_SPO2_VALUE);
    writeRegister(REG_SPO2_CONFIG, SPO2_CONFIG_VALUE);
    writeRegister(REG_LED1_PA, LED_PULSE_AMPLITUDE);
    writeRegister(REG_LED2_PA, LED_PULSE_AMPLITUDE);
    writeRegister(REG_PILOT_PA, LED_PULSE_AMPLITUDE);

    if (!flushFifo()) {
        return false;
    }

    return (readRegister(REG_MODE_CONFIG) & 0x80) == 0;
}

int Max30102::availableFifoSamples() {
    const uint8_t writePtr = readRegister(REG_FIFO_WR_PTR);
    const uint8_t readPtr = readRegister(REG_FIFO_RD_PTR);
    int count = static_cast<int>(writePtr) - static_cast<int>(readPtr);
    if (count < 0) {
        count += 32;
    }
    return count;
}

void Max30102::drainFifo() {
    int samples = availableFifoSamples();
    lastFifoPending = static_cast<uint8_t>(samples > 0 ? samples : 0);

    if (samples <= 0) {
        if (readRegister(REG_OVF_COUNTER) != 0) {
            flushFifo();
        }
        return;
    }

    if (samples > 32) {
        samples = 32;
    }

    for (int i = 0; i < samples; i++) {
        uint32_t red = 0;
        uint32_t ir = 0;
        if (!readFifoSample(red, ir)) {
            break;
        }

        if (swapRedIrChannels) {
            appendSample(ir, red);
        } else {
            appendSample(red, ir);
        }
    }

    if (readRegister(REG_OVF_COUNTER) != 0) {
        flushFifo();
    }
}

void Max30102::detectChannelMapping() {
    uint32_t redSum = 0;
    uint32_t irSum = 0;

    for (int i = 0; i < bufferCount; i++) {
        redSum += redBuffer[i];
        irSum += irBuffer[i];
    }

    const uint32_t redAvg = redSum / static_cast<uint32_t>(bufferCount);
    const uint32_t irAvg = irSum / static_cast<uint32_t>(bufferCount);

    if (!swapRedIrChannels && redAvg > irAvg * 2 && redAvg >= static_cast<uint32_t>(MIN_FINGER_IR)) {
        swapRedIrChannels = true;
        for (int i = 0; i < bufferCount; i++) {
            const uint32_t temp = redBuffer[i];
            redBuffer[i] = irBuffer[i];
            irBuffer[i] = temp;
        }
        Serial.println(F("Sensor pulso: canales rojo/IR ajustados"));
    }

    channelsMapped = true;
}

bool Max30102::readFifoSample(uint32_t& red, uint32_t& ir) {
    uint8_t fifoBytes[6];
    if (!readBytes(REG_FIFO_DATA, fifoBytes, sizeof(fifoBytes))) {
        return false;
    }

    red = ((fifoBytes[0] << 16) | (fifoBytes[1] << 8) | fifoBytes[2]) & 0x03FFFF;
    ir = ((fifoBytes[3] << 16) | (fifoBytes[4] << 8) | fifoBytes[5]) & 0x03FFFF;
    return true;
}

void Max30102::refreshIrDiagnostics() {
    if (bufferCount <= 0) {
        return;
    }

    uint32_t irSum = 0;
    uint32_t redSum = 0;
    uint32_t irMin = UINT32_MAX;
    uint32_t irMax = 0;

    for (int i = 0; i < bufferCount; i++) {
        irSum += irBuffer[i];
        redSum += redBuffer[i];
        if (irBuffer[i] < irMin) {
            irMin = irBuffer[i];
        }
        if (irBuffer[i] > irMax) {
            irMax = irBuffer[i];
        }
    }

    lastIrAverage = irSum / static_cast<uint32_t>(bufferCount);
    lastRedAverage = redSum / static_cast<uint32_t>(bufferCount);
    lastIrVariation = irMax - irMin;
    lastSignalSaturated = lastIrAverage > static_cast<uint32_t>(MAX_FINGER_IR)
        || (lastIrAverage >= static_cast<uint32_t>(MIN_FINGER_IR)
            && lastIrVariation < static_cast<uint32_t>(MIN_IR_VARIATION));
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

int Max30102::calibrateHeartRate(int rawHr, uint32_t irVariation) {
    if (rawHr < 40 || rawHr > 200) {
        return rawHr;
    }

    if (rawHr >= HR_HARMONIC_CORRECT_MIN
        && irVariation >= TARGET_IR_AC_MIN
        && irVariation <= TARGET_IR_AC_MAX) {
        const int halfRate = rawHr / 2;
        if (halfRate >= 50 && halfRate <= 100) {
            return halfRate;
        }
    }

    return rawHr;
}

int Max30102::calibrateSpO2(int rawSpO2) {
    if (rawSpO2 <= 0) {
        return rawSpO2;
    }

    int calibrated = rawSpO2 + SPO2_CALIBRATION_OFFSET;
    if (calibrated < 70) {
        calibrated = 70;
    }
    if (calibrated > 100) {
        calibrated = 100;
    }
    return calibrated;
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

    if (lastIrAverage < static_cast<uint32_t>(MIN_FINGER_IR) &&
        lastRedAverage < static_cast<uint32_t>(MIN_FINGER_IR)) {
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
        BUFFER_SIZE,
        redBuffer,
        &spo2,
        &spo2Valid,
        &heartRate,
        &hrValid
    );

    if (hrValid != 0) {
        heartRate = calibrateHeartRate(static_cast<int>(heartRate), lastIrVariation);
    }
    if (spo2Valid != 0) {
        spo2 = calibrateSpO2(static_cast<int>(spo2));
    }

    bool rawHrOk = (hrValid != 0 && heartRate >= 40 && heartRate <= 200);
    bool rawSpO2Ok = (spo2Valid != 0 && spo2 >= 70 && spo2 <= 100);

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
