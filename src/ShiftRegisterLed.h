#pragma once

#include <Arduino.h>

class ShiftRegisterLed {
public:
    uint8_t dsPin;
    uint8_t latchPin;
    uint8_t clockPin;

    byte data1{};
    byte data2{};

    boolean isOn{false};
    unsigned long lastChecked;

public:
    ShiftRegisterLed(uint8_t ds, uint8_t latch, uint8_t clock) : dsPin(ds), latchPin(latch), clockPin(clock) {
        pinMode(ds, OUTPUT);
        pinMode(latch, OUTPUT);
        pinMode(clock, OUTPUT);

        lastChecked = millis();
    }

    void loop(unsigned long now) {
        if (now - lastChecked > 1000) {
            lastChecked = now;
            isOn = !isOn;

            digitalWrite(latchPin, LOW);

            data1 = isOn ? 0xFF : 0x00;
            data2 = isOn ? 0xFF : 0x00;

            shiftOut(dsPin, clockPin, MSBFIRST, data1);
            shiftOut(dsPin, clockPin, MSBFIRST, data2);

            digitalWrite(latchPin, HIGH);
        }
    }
};
