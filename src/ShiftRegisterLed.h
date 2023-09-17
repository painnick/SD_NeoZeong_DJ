#pragma once

#include <Arduino.h>

class ShiftRegisterLed {
public:
    uint8_t dsPin;
    uint8_t latchPin;
    uint8_t clockPin;

    byte data1{};
    byte data2{};

    boolean stopped{false};
    boolean isOn{false};
    unsigned long lastChecked;

public:
    ShiftRegisterLed(uint8_t ds, uint8_t latch, uint8_t clock);

    void loop(unsigned long now);
    void stop();
    void resume();
};
