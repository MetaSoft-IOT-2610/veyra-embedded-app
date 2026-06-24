#ifndef MAX30102_H
#define MAX30102_H

/**
 * @file Max30102.h
 * @brief Declares the Max30102 class.
 *
 * A concrete sensor class in the Modest IoT Nano-framework for reading heart rate
 * and SpO2 from a GY-MAX30102 module via I2C polling (no INT pin required).
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

#include "Sensor.h"
#include <Wire.h>
#include <cstddef>
#include <cstdint>

class Max30102 : public Sensor {
private:
    int sclPin;
    TwoWire* i2cBus;
    int activeSdaPin;
    int activeSclPin;
    uint8_t lastFifoPending;
    bool initialized;
    bool i2cBusStarted;
    int emptyPollStreak;
    unsigned long initMillis;
    int lastHeartRate;
    int lastSpO2;
    bool lastReadingValid;
    bool lastSpO2Valid;
    unsigned long lastReportMillis;

    static const int SAMPLE_BUFFER_SIZE = 100;
    uint32_t irBuffer[SAMPLE_BUFFER_SIZE];
    uint32_t redBuffer[SAMPLE_BUFFER_SIZE];
    int bufferCount;
    bool swapRedIrChannels;
    bool channelsMapped;
    uint32_t lastIrAverage;
    uint32_t lastRedAverage;
    uint32_t lastIrVariation;

    static const int SMOOTH_WINDOW_SIZE = 7;
    static const int HR_OUTLIER_DELTA = 18;
    static const int SPO2_OUTLIER_DELTA = 5;

    static int calibrateHeartRate(int rawHr, uint32_t irVariation);
    static int calibrateSpO2(int rawSpO2);

    int hrSmoothBuffer[SMOOTH_WINDOW_SIZE];
    int spo2SmoothBuffer[SMOOTH_WINDOW_SIZE];
    int hrSmoothCount;
    int spo2SmoothCount;

    static int averageSamples(const int* samples, int count);
    bool smoothHeartRate(int rawHr, int& smoothedHr);
    bool smoothSpO2(int rawSpO2, int& smoothedSpO2);

    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool readBytes(uint8_t reg, uint8_t* buffer, size_t length);
    bool reset();
    bool flushFifo();
    bool configure();
    bool wakeSensor();
    bool probePartId(TwoWire& bus, int sda, int scl);
    int availableFifoSamples();
    void drainFifo();
    void detectChannelMapping();
    bool readFifoSample(uint32_t& red, uint32_t& ir);
    void appendSample(uint32_t red, uint32_t ir);
    void refreshIrDiagnostics();
    void calculateMetrics();
    void shiftSampleWindow();

    bool usesAdc2Bus() const;
    void ensureI2cBus();
    void beginBusSession();
    void endBusSession();
    void recoverSensorIfStalled();

    static void (*wifiSuspendFn)();
    static bool (*wifiResumeFn)();

public:
    enum class PpgPhase : uint8_t {
        NotDetected,
        WarmingUp,
        Measuring,
        Ready
    };

    static const int PPG_READ_EVENT_ID = 3;
    static const Event PPG_READ_EVENT;

    using WifiSuspendHook = void (*)();
    using WifiResumeHook = bool (*)();

    static void setWifiCoexistenceHooks(WifiSuspendHook suspend, WifiResumeHook resume);

    Max30102(int sdaPin, int sclPin, EventHandler* eventHandler = nullptr);

    void begin();
    void onWifiReady();
    void update();
    void serviceBeforeBlocking();

    int getLastHeartRate() const;
    int getLastSpO2() const;
    bool isLastSpO2Valid() const;
    bool isLastReadingValid() const;
    bool isInitialized() const;
    int getBufferCount() const;
    int getActiveSdaPin() const;
    int getActiveSclPin() const;
    uint8_t getLastFifoPending() const;
    uint32_t getLastRedAverage() const;
    uint32_t getLastIrAverage() const;
    uint32_t getLastIrVariation() const;
    bool isFingerDetected() const;
    bool isSignalSaturated() const;
    PpgPhase getPhase() const;
};

#endif // MAX30102_H
