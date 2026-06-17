#ifndef VEYRA_DEVICE_H
#define VEYRA_DEVICE_H

/**
 * @file VeyraDevice.h
 * @brief Declares the VeyraDevice class.
 *
 * Application device for the Veyra Embedded App. Composes sensors and actuators
 * using the Modest IoT Nano-framework (C++ Edition).
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

#include "Device.h"
#include "Lm35.h"
#include "Neo6m.h"
#include "Max30102.h"
#include "Lcd1602.h"

class VeyraDevice : public Device {
private:
    Lm35 lm35;
    Neo6m neo6m;
    Max30102 max30102;
    Lcd1602 lcd;

    unsigned long lastStatusRefreshMs;
    unsigned long lastTemperatureReadMs;
    uint8_t lcdPage;

    static const unsigned long STATUS_REFRESH_MS = 2000;
    static const unsigned long TEMPERATURE_READ_INTERVAL_MS = 1000;

    void readTemperature();
    void updateGps();
    void updateMax30102();
    void refreshStatus();

public:
    static const int LM35_PIN = 4;
    static const int GPS_RX_PIN = 17;
    static const int GPS_TX_PIN = 16;
    static const int MAX30102_SDA_PIN = 33;
    static const int MAX30102_SCL_PIN = 32;
    static const int LCD_SDA_PIN = 21;
    static const int LCD_SCL_PIN = 22;

    VeyraDevice(
        int lm35Pin = LM35_PIN,
        int gpsRxPin = GPS_RX_PIN,
        int gpsTxPin = GPS_TX_PIN,
        int max30102SdaPin = MAX30102_SDA_PIN,
        int max30102SclPin = MAX30102_SCL_PIN,
        int lcdSdaPin = LCD_SDA_PIN,
        int lcdSclPin = LCD_SCL_PIN
    );
    void on(Event event) override;
    void handle(Command command) override;

    /**
     * @brief Initializes hardware and performs the first sensor read / display refresh.
     */
    void begin();

    /**
     * @brief Main loop tick: polls sensors and refreshes LCD/Serial on a fixed interval.
     */
    void update();

    Lm35& getLm35();
    Neo6m& getNeo6m();
    Max30102& getMax30102();
    Lcd1602& getLcd();
};

#endif // VEYRA_DEVICE_H
