#pragma once

#include <EEPROM.h>

#define EEPROM_SIZE (1 + 1 + 1 + 1)
#define EEPROM_ADDR_VOLUME 0
#define EEPROM_ADDR_COEF (EEPROM_ADDR_VOLUME + 1)
#define EEPROM_ADDR_SENSE (EEPROM_ADDR_COEF + 1)
#define EEPROM_ADDR_BRIGHT (EEPROM_ADDR_SENSE + 1)

class EEPROMManager {
public:
    static void init() {
        EEPROM.begin(EEPROM_SIZE);
    }

    static uint8_t readVolume() {
        uint8_t volume = EEPROM.read(EEPROM_ADDR_VOLUME);
        if (volume == 0) {
            volume = 10;
        }
        return volume;
    }

    static void writeVolume(uint8_t vol) {
        EEPROM.writeUChar(EEPROM_ADDR_VOLUME, vol);
        EEPROM.commit();
    }

    static uint8_t readCoefficient() {
        uint8_t coefficient = EEPROM.read(EEPROM_ADDR_COEF);
        if (coefficient < 1) { // Must not Zero!
            coefficient = 1;
        }
        return coefficient;
    }

    static void writeCoefficient(uint8_t coef) {
        EEPROM.writeUChar(EEPROM_ADDR_COEF, coef);
        EEPROM.commit();
    }

    static uint8_t readSensitivity() {
        return EEPROM.read(EEPROM_ADDR_SENSE);
    }

    static void writeSensitivity(uint8_t sense) {
        EEPROM.writeUChar(EEPROM_ADDR_SENSE, sense);
        EEPROM.commit();
    }

    static uint8_t readBrightness() {
        uint8_t bright = EEPROM.read(EEPROM_ADDR_BRIGHT);
        if (bright == 0) {
            bright = 30;
        }
        return bright;
    }

    static void writeBrightness(uint8_t bright) {
        EEPROM.writeUChar(EEPROM_ADDR_BRIGHT, bright);
        EEPROM.commit();
    }
};