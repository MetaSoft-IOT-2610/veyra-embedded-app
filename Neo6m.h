#ifndef NEO6M_H
#define NEO6M_H

/**
 * @file Neo6m.h
 * @brief Declares the Neo6m class.
 *
 * A concrete sensor class in the Modest IoT Nano-framework for reading GPS location
 * from a NEO-6M module via UART. Extends the `Sensor` base class for serial input devices.
 *
 * @author Angel Velasquez
 * @date March 22, 2025
 * @version 0.1
 */

/*
 * This file is part of the Modest IoT Nano-framework (C++ Edition).
 * Copyright (c) 2025 Angel Velasquez
 *
 * Licensed under the Creative Commons Attribution-NoDerivatives 4.0 International (CC BY-ND 4.0).
 * You may use, copy, and distribute this software in its original, unmodified form, provided
 * you give appropriate credit to the original author (Angel Velasquez) and include this notice.
 * Modifications, adaptations, or derivative works are not permitted.
 *
 * Full license text: https://creativecommons.org/licenses/by-nd/4.0/legalcode
 */

#include "Sensor.h"
#include <HardwareSerial.h>
#include <cstdint>

class Neo6m : public Sensor {
private:
    int txPin; ///< GPIO pin connected to the module RX line (ESP32 TX).
    HardwareSerial& serial; ///< UART interface used to communicate with the module.
    float lastLatitude; ///< Last latitude reading in decimal degrees.
    float lastLongitude; ///< Last longitude reading in decimal degrees.
    bool lastReadingValid; ///< Whether the last reading has a valid GPS fix.
    bool receivingData; ///< Whether NMEA bytes have been received.
    int satelliteCount; ///< Satellites used for fix (GGA field 7).
    int satellitesInView; ///< Satellites in view (GSV field 4, message 1).
    unsigned long bytesReceived; ///< Total bytes received from the GPS module.
    long activeBaudRate; ///< Active UART baud rate.
    char lineBuffer[128]; ///< Buffer for accumulating NMEA sentence characters.
    uint8_t lineIndex; ///< Current write index in the line buffer.

    bool parseNmeaSentence(const char* sentence);
    bool parseRmcSentence(const char* sentence);
    bool parseGgaSentence(const char* sentence);
    void parseGsvSentence(const char* sentence);
    static float nmeaToDecimalDegrees(const char* value, char direction);

public:
    static const int LOCATION_READ_EVENT_ID = 2; ///< Unique ID for GPS location read event.
    static const Event LOCATION_READ_EVENT; ///< Predefined event for GPS location readings.

    /**
     * @brief Constructs a Neo6m sensor.
     * @param rxPin The GPIO pin connected to the module TX line (ESP32 RX).
     * @param txPin The GPIO pin connected to the module RX line (ESP32 TX).
     * @param eventHandler Optional handler to receive GPS events (default: nullptr).
     */
    Neo6m(int rxPin, int txPin, EventHandler* eventHandler = nullptr);

    /**
     * @brief Starts UART communication. Call from setup().
     */
    void begin();

    /**
     * @brief Processes incoming NMEA data and triggers an event when a valid fix is received.
     */
    void update();

    /**
     * @brief Gets the last latitude reading in decimal degrees.
     * @return Last latitude, or 0.0 if no valid fix yet.
     */
    float getLastLatitude() const;

    /**
     * @brief Gets the last longitude reading in decimal degrees.
     * @return Last longitude, or 0.0 if no valid fix yet.
     */
    float getLastLongitude() const;

    /**
     * @brief Indicates whether the last reading has a valid GPS fix.
     * @return True if the last reading is valid.
     */
    bool isLastReadingValid() const;

    /**
     * @brief Indicates whether the module is sending NMEA data.
     */
    bool isReceivingData() const;

    /**
     * @brief Gets the satellite count from the last GGA sentence.
     */
    int getSatelliteCount() const;

    /**
     * @brief Gets the number of satellites in view (from GSV sentences).
     */
    int getSatellitesInView() const;

    /**
     * @brief Gets the total number of bytes received from the GPS module.
     */
    unsigned long getBytesReceived() const;

    /**
     * @brief Gets the active UART baud rate.
     */
    long getActiveBaudRate() const;
};

#endif // NEO6M_H
