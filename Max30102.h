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
#include <cstdint>

class Max30102 : public Sensor {
private:
    int sclPin; ///< GPIO pin for I2C SCL.
    bool initialized; ///< Whether the sensor responded to initialization.
    int lastHeartRate; ///< Last smoothed heart rate reading in BPM.
    int lastSpO2; ///< Last smoothed SpO2 reading in percent.
    bool lastReadingValid; ///< Whether the last heart rate reading is valid.
    bool lastSpO2Valid; ///< Whether the last SpO2 reading is valid.
    unsigned long lastReportMillis; ///< Timestamp of the last reported reading.

    static const int SAMPLE_BUFFER_SIZE = 100; ///< 4 s window at 25 effective samples/s (Maxim algorithm).
    uint32_t irBuffer[SAMPLE_BUFFER_SIZE];
    uint32_t redBuffer[SAMPLE_BUFFER_SIZE];
    int bufferCount;
    uint32_t lastIrAverage; ///< Average IR level for finger detection diagnostics.
    uint32_t lastIrVariation; ///< Peak-to-peak IR variation in the last sample window.
    bool lastSignalSaturated; ///< Whether the last window was clipped at ADC full scale.

    static const int SMOOTH_WINDOW_SIZE = 5;
    static const int HR_OUTLIER_DELTA = 25;
    static const int SPO2_OUTLIER_DELTA = 4;

    int hrSmoothBuffer[SMOOTH_WINDOW_SIZE];
    int spo2SmoothBuffer[SMOOTH_WINDOW_SIZE];
    int hrSmoothCount;
    int spo2SmoothCount;

    void resetSmoothing();
    static int averageSamples(const int* samples, int count);
    bool smoothHeartRate(int rawHr, int& smoothedHr);
    bool smoothSpO2(int rawSpO2, int& smoothedSpO2);

    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool reset();
    bool configure();
    int availableFifoSamples();
    bool readFifoSample(uint32_t& red, uint32_t& ir);
    void appendSample(uint32_t red, uint32_t ir);
    void calculateMetrics();
    void shiftSampleWindow();

public:
    static const int PPG_READ_EVENT_ID = 3; ///< Unique ID for PPG read event.
    static const Event PPG_READ_EVENT; ///< Predefined event for PPG readings.

    /**
     * @brief Constructs a Max30102 sensor.
     * @param sdaPin The GPIO pin for I2C SDA.
     * @param sclPin The GPIO pin for I2C SCL.
     * @param eventHandler Optional handler to receive PPG events (default: nullptr).
     */
    Max30102(int sdaPin, int sclPin, EventHandler* eventHandler = nullptr);

    /**
     * @brief Initializes I2C and configures the sensor. Call from setup().
     */
    void begin();

    /**
     * @brief Polls the FIFO over I2C and triggers an event when metrics are ready.
     */
    void update();

    /**
     * @brief Gets the last heart rate reading in BPM.
     */
    int getLastHeartRate() const;

    /**
     * @brief Gets the last SpO2 reading in percent.
     */
    int getLastSpO2() const;

    /**
     * @brief Indicates whether the last SpO2 reading is valid.
     */
    bool isLastSpO2Valid() const;

    /**
     * @brief Indicates whether the last PPG reading is valid.
     */
    bool isLastReadingValid() const;

    /**
     * @brief Indicates whether the sensor initialized successfully.
     */
    bool isInitialized() const;

    /**
     * @brief Gets the average IR level from the last sample window (for diagnostics).
     */
    uint32_t getLastIrAverage() const;

    /**
     * @brief Gets peak-to-peak IR variation from the last sample window.
     */
    uint32_t getLastIrVariation() const;

    /**
     * @brief Indicates whether IR level suggests finger contact.
     */
    bool isFingerDetected() const;

    /**
     * @brief Indicates whether the PPG signal was saturated or too flat to measure pulse.
     */
    bool isSignalSaturated() const;
};

#endif // MAX30102_H
