#include "ShiftRegisterLed.h"

ShiftRegisterLed::ShiftRegisterLed(uint8_t ds, uint8_t latch, uint8_t clock) : dsPin(ds), latchPin(latch),
                                                                               clockPin(clock) {
    pinMode(ds, OUTPUT);
    pinMode(latch, OUTPUT);
    pinMode(clock, OUTPUT);

    lastChecked = millis();
}

void ShiftRegisterLed::loop(unsigned long now) {
    if (!stopped && (now - lastChecked > interval)) {
        lastChecked = now;

        digitalWrite(latchPin, LOW);

        for (int i = 0; i < ALL_SR_PIN_COUNT; i++) {
            boolean *seq = sequences[i];
            if (seq != nullptr) {
                int subIdx = subIndex[i];
                if (i < SR_PIN_COUNT) {
                    if (seq[subIdx]) {
                        data1 = bitSet(data1, i);
                    } else {
                        data1 = bitClear(data1, i);
                    }
                } else {
                    if (seq[subIdx]) {
                        data2 = bitSet(data2, i - SR_PIN_COUNT);
                    } else {
                        data2 = bitClear(data2, i - SR_PIN_COUNT);
                    }
                }
                subIndex[i] = (subIdx + 1) % itemCount[i];
            }
        }

        shiftOut(dsPin, clockPin, MSBFIRST, data2);
        shiftOut(dsPin, clockPin, MSBFIRST, data1);

        digitalWrite(latchPin, HIGH);
    }
}

void ShiftRegisterLed::stop() {
    stopped = true;
    data1 = 0x00;
    data2 = 0x00;

    digitalWrite(latchPin, LOW);
    shiftOut(dsPin, clockPin, MSBFIRST, data2);
    shiftOut(dsPin, clockPin, MSBFIRST, data1);
    digitalWrite(latchPin, HIGH);
}

void ShiftRegisterLed::resume() {
    stopped = false;
}

void ShiftRegisterLed::setInterval(unsigned long ms) {
    interval = ms;
}

void ShiftRegisterLed::setSeq(int idx, boolean *seq, int count) {
    sequences[idx] = seq;
    itemCount[idx] = count;
    subIndex[idx] = 0;
}
