#ifndef LM35_H
#define LM35_H

/**
 * @file Lm35.h
 * @brief Declares the Lm35 class.
 *
 * A concrete sensor class in the Modest IoT Nano-framework for reading linear centigrade
 * temperature from an LM35. It serves as an example of extending the `Sensor` base class
 * for analog input devices.
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

class Lm35 : public Sensor {
private:
    float lastReading; ///< Last temperature reading in degrees Celsius.

public:
    static const int TEMPERATURE_READ_EVENT_ID = 1; ///< Unique ID for temperature read event.
    static const Event TEMPERATURE_READ_EVENT; ///< Predefined event for temperature readings.
    static const float BODY_CONTACT_THRESHOLD_C; ///< Reading at/above this suggests skin contact.
    static const float BODY_TEMP_MAX_C; ///< Upper bound for a plausible body temperature.

    /**
     * @brief Constructs an Lm35 sensor.
     * @param pin The analog GPIO pin connected to LM35 OUT.
     * @param eventHandler Optional handler to receive temperature events (default: nullptr).
     */
    Lm35(int pin, EventHandler* eventHandler = nullptr);

    /**
     * @brief Configures the analog input pin. Call from setup().
     */
    void begin();

    /**
     * @brief Reads the current temperature from the LM35 in degrees Celsius.
     * @return Temperature in degrees Celsius.
     */
    float readTemperatureCelsius();

    /**
     * @brief Gets the last temperature reading stored by the sensor.
     * @return Last temperature in degrees Celsius.
     */
    float getLastReading() const;

    /**
     * @brief Indicates whether the last reading looks like body temperature (skin contact).
     */
    bool isBodyTemperatureValid() const;

    /**
     * @brief Reads temperature and propagates a temperature read event.
     */
    void readAndTriggerEvent();
};

#endif // LM35_H
