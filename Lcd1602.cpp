/**
 * @file Lcd1602.cpp
 * @brief Implements the Lcd1602 class.
 *
 * Drives a 16x2 HD44780 LCD with PCF8574 I2C backpack over Wire1.
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

#include "Lcd1602.h"
#include <Arduino.h>
#include <Wire.h>
#include <cstring>

const Command Lcd1602::CLEAR_COMMAND = Command(CLEAR_COMMAND_ID);
const Command Lcd1602::REFRESH_COMMAND = Command(REFRESH_COMMAND_ID);

static const uint8_t LCD_BACKLIGHT = 0x08;
static const uint8_t LCD_ENABLE = 0x04;
static const uint8_t LCD_RS = 0x01;

Lcd1602::Lcd1602(int sdaPin, int sclPin, CommandHandler* commandHandler, uint8_t address)
    : Actuator(sdaPin, commandHandler),
      initialized(false),
      sclPin(sclPin),
      i2cAddress(address) {
    clearBuffer();
}

void Lcd1602::begin() {
    Wire1.begin(pin, sclPin);
    Wire1.setClock(100000);
    Wire1.setTimeout(100);
    delay(50);

    initDisplay();
    initialized = true;
    setLine(0, "Veyra IoT");
    setLine(1, "Initializing...");
    handle(REFRESH_COMMAND);
}

void Lcd1602::handle(Command command) {
    if (!initialized) {
        return;
    }
    if (command == CLEAR_COMMAND) {
        clearBuffer();
        writeToHardware();
    } else if (command == REFRESH_COMMAND) {
        writeToHardware();
    }
    Actuator::handle(command);
}

void Lcd1602::setLine(uint8_t row, const char* text) {
    if (row >= ROWS || text == nullptr) {
        return;
    }

    strncpy(lines[row], text, COLS);
    lines[row][COLS] = '\0';
}

void Lcd1602::clearBuffer() {
    for (int row = 0; row < ROWS; row++) {
        memset(lines[row], ' ', COLS);
        lines[row][COLS] = '\0';
    }
}

void Lcd1602::refresh() {
    handle(REFRESH_COMMAND);
}

bool Lcd1602::isInitialized() const {
    return initialized;
}

void Lcd1602::initDisplay() {
    expanderWrite(LCD_BACKLIGHT);
    delay(50);

    write4Bits(0x03 << 4);
    delayMicroseconds(4500);
    write4Bits(0x03 << 4);
    delayMicroseconds(4500);
    write4Bits(0x03 << 4);
    delayMicroseconds(150);
    write4Bits(0x02 << 4);

    sendCommand(0x28);
    sendCommand(0x0C);
    sendCommand(0x06);
    sendCommand(0x01);
    delay(2);
}

void Lcd1602::writeToHardware() {
    sendCommand(0x01);
    delay(2);
    sendCommand(0x02);
    delay(2);

    for (uint8_t row = 0; row < ROWS; row++) {
        setCursor(0, row);
        for (int col = 0; col < COLS; col++) {
            sendData(static_cast<uint8_t>(lines[row][col]));
        }
    }
}

void Lcd1602::sendCommand(uint8_t value) {
    write4Bits((value & 0xF0));
    write4Bits((value << 4) & 0xF0);
}

void Lcd1602::sendData(uint8_t value) {
    write4Bits((value & 0xF0) | LCD_RS);
    write4Bits(((value << 4) & 0xF0) | LCD_RS);
}

void Lcd1602::write4Bits(uint8_t value) {
    expanderWrite(value | LCD_BACKLIGHT);
    pulseEnable(value | LCD_BACKLIGHT);
}

void Lcd1602::pulseEnable(uint8_t value) {
    expanderWrite(value | LCD_ENABLE | LCD_BACKLIGHT);
    delayMicroseconds(1);
    expanderWrite((value & ~LCD_ENABLE) | LCD_BACKLIGHT);
    delayMicroseconds(50);
}

void Lcd1602::expanderWrite(uint8_t value) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(value);
    Wire1.endTransmission();
}

void Lcd1602::setCursor(uint8_t col, uint8_t row) {
    static const uint8_t rowOffsets[] = {0x00, 0x40};
    sendCommand(0x80 | (col + rowOffsets[row]));
}
