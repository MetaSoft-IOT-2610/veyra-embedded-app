#ifndef LCD1602_H
#define LCD1602_H

/**
 * @file Lcd1602.h
 * @brief Declares the Lcd1602 class.
 *
 * A concrete actuator class in the Modest IoT Nano-framework for a 16x2 I2C LCD
 * with PCF8574 backpack. Uses a dedicated I2C bus (Wire1 on ESP32).
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

#include "Actuator.h"
#include <cstdint>

class Lcd1602 : public Actuator {
private:
    static const int COLS = 16;
    static const int ROWS = 2;

    char lines[ROWS][COLS + 1];
    bool initialized;
    int sclPin; ///< GPIO pin for I2C SCL.
    uint8_t i2cAddress;

    void initDisplay();
    void writeToHardware();
    void sendCommand(uint8_t value);
    void sendData(uint8_t value);
    void write4Bits(uint8_t value);
    void pulseEnable(uint8_t value);
    void expanderWrite(uint8_t value);
    void setCursor(uint8_t col, uint8_t row);

public:
    static const int CLEAR_COMMAND_ID = 0;
    static const int REFRESH_COMMAND_ID = 1;
    static const Command CLEAR_COMMAND;
    static const Command REFRESH_COMMAND;

    /**
     * @brief Constructs an Lcd1602 actuator.
     * @param sdaPin The GPIO pin for I2C SDA.
     * @param sclPin The GPIO pin for I2C SCL.
     * @param commandHandler Optional handler to receive commands (default: nullptr).
     * @param i2cAddress I2C address of the PCF8574 backpack (default: 0x27).
     */
    Lcd1602(int sdaPin, int sclPin, CommandHandler* commandHandler = nullptr, uint8_t i2cAddress = 0x27);

    /**
     * @brief Initializes I2C and the display hardware. Call from setup().
     */
    void begin();

    /**
     * @brief Handles commands to control the LCD.
     * @param command The command to execute.
     */
    void handle(Command command) override;

    /**
     * @brief Sets the text content for a display row.
     * @param row Row index (0 or 1).
     * @param text Text to display (truncated to 16 characters).
     */
    void setLine(uint8_t row, const char* text);

    /**
     * @brief Clears the internal text buffer.
     */
    void clearBuffer();

    /**
     * @brief Writes the buffer to the LCD hardware.
     */
    void refresh();

    /**
     * @brief Indicates whether the LCD initialized successfully.
     */
    bool isInitialized() const;
};

#endif // LCD1602_H
