#include "ShiftRegisterLed.h"

ShiftRegisterLed::ShiftRegisterLed(uint8_t ds, uint8_t latch, uint8_t clock) : dsPin(ds), latchPin(latch), clockPin(clock) {
    pinMode(ds, OUTPUT);
    pinMode(latch, OUTPUT);
    pinMode(clock, OUTPUT);

    lastChecked = millis();
}

void ShiftRegisterLed::loop(unsigned long now) {
    if (!stopped && (now - lastChecked > 1000)) {
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

void ShiftRegisterLed::stop() {
    stopped = true;
    data1 = 0x00;
    data2 = 0x00;

    digitalWrite(latchPin, LOW);
    shiftOut(dsPin, clockPin, MSBFIRST, data1);
    shiftOut(dsPin, clockPin, MSBFIRST, data2);
    digitalWrite(latchPin, HIGH);
}

void ShiftRegisterLed::resume() {
    stopped = false;
}
