#pragma once

#include <Arduino.h>

#define SR_PIN_COUNT 8
#define ALL_SR_PIN_COUNT (SR_PIN_COUNT * 2)

class ShiftRegisterLed {
public:
    uint8_t dsPin;
    uint8_t latchPin;
    uint8_t clockPin;

    byte data1{0x00};
    byte data2{0x00};

    boolean stopped{false};

    unsigned long interval{100}; // ms
    boolean *sequences[ALL_SR_PIN_COUNT]{nullptr};
    int itemCount[ALL_SR_PIN_COUNT]{0};
    int subIndex[ALL_SR_PIN_COUNT]{0};

    unsigned long lastChecked;

public:
    ShiftRegisterLed(uint8_t ds, uint8_t latch, uint8_t clock);

    void loop(unsigned long now);

    void stop();

    void resume();

    void setInterval(unsigned long ms);

    void setSeq(int idx, boolean seq[], int count);
};
