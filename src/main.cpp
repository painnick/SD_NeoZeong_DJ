#include <Arduino.h>

#include "common.h"
#include "DFMiniMp3.h"
#include "Mp3Notify.h"
#include "AdcUtil.h"

#include "NeoPixelEffects.h"

#define MAIN_TAG "Main"

// ============================================================
// Dfplayer
// ------------------------------------------------------------
// Volume : 0~30
// ============================================================
int Volume = 10;

HardwareSerial mySerial(UART_PLAYER);
DfMp3 dfmp3(mySerial);

// ============================================================
// Neopixel
// ============================================================
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

#define STRIP_STAGE_SIZE 65
#define STRIP_BAR_MAX_HEIGHT 8

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar1 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar2 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR2, NEO_GRB + NEO_KHZ800);

// ============================================================
// Bluetooth
// ============================================================
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ============================================================
// EEPROM
// ============================================================
#include <EEPROM.h>

#define EEPROM_SIZE (1 + 4 + 4)
#define EEPROM_ADDR_VOLUME 0
#define EEPROM_ADDR_COEF (EEPROM_ADDR_VOLUME + 4)
#define EEPROM_ADDR_SENSE (EEPROM_ADDR_COEF + 4)


// ============================================================
// Envelop
// ============================================================
int Coefficient = 3;
int Sensitivity = 10; // decrease for better sensitivity
int MaxAmplitudeOnPlay = 0;
#define MAX_AMPLITUDE 250
#define MAX_TARGET_AMPLITUDE (MAX_AMPLITUDE / 2)

// ============================================================
// Shift Register
// ============================================================
#include "ShiftRegisterLed.h"

#define PIN_SR_DATA 21
#define PIN_SR_LATCH 22
#define PIN_SR_CLOCK 23

ShiftRegisterLed ledController(PIN_SR_DATA, PIN_SR_LATCH, PIN_SR_CLOCK);

// ============================================================

int SuggestBarHeight = 0;
int LastAmplitude = 0;

[[noreturn]] void taskStage(void *params) {
    while (true) {
        theaterChaseRainbow(stripStage, 100);
    }
}

unsigned long lastBarChecked = 0;

[[noreturn]] void taskBar(void *params) {
    int lastHeight = 0;
    bool off = false;

    while (true) {
        unsigned long now = millis();
        int suggestedBarHeight = SuggestBarHeight;
        int lastAmplitude = LastAmplitude;
        if (suggestedBarHeight > lastHeight) {
            ESP_LOGD(MAIN_TAG,
                     "Bar == UP == Height %2d (Amp %3d, Last %2d)",
                     suggestedBarHeight, lastAmplitude, lastHeight);
            lastHeight = suggestedBarHeight;
            uint32_t color = calcColor(min(lastHeight, STRIP_BAR_MAX_HEIGHT));
            colorHeight(stripBar1, stripBar2, color, lastHeight);
            lastBarChecked = now;
            off = false;
        } else {
            if (now - lastBarChecked > 100) {
                lastHeight--;
                if (lastHeight < 0) {
                    lastHeight = 0;
                    off = true;
                }
                if (!off) {
                    lastHeight = max(lastHeight, 0);
//                    ESP_LOGD(MAIN_TAG, "Bar .-down-.  Height %d (Current %d)", lastHeight, suggestedBarHeight);

                    uint32_t color = calcColor(min(lastHeight, STRIP_BAR_MAX_HEIGHT));
                    colorHeight(stripBar1, stripBar2, color, lastHeight);
                    lastBarChecked = now;
                }
            }
        }
        vTaskDelay(1);
    }
}

void sendCommandList() {
    SerialBT.println("--- Commands ---");
    SerialBT.println("help");
    SerialBT.println("volume %d");
    SerialBT.println("coef %d");
    SerialBT.println("sense %d");
    SerialBT.println("status");
    SerialBT.println("next");
    SerialBT.println("bright %d");
    SerialBT.println("--- End ---");
}

void processCommand(const String &cmd) {
    if (cmd.equals("help")) {
        sendCommandList();
    } else if (cmd.startsWith("volume ")) {
        uint8_t vol = 0;
        sscanf(cmd.c_str(), "volume %u", &vol);
        dfmp3.setVolume(vol);
        dfmp3.loop();
        Volume = vol;
        EEPROM.writeUChar(EEPROM_ADDR_VOLUME, vol);
        EEPROM.commit();
    } else if (cmd.startsWith("coef ")) {
        uint8_t coef = 0;
        sscanf(cmd.c_str(), "coef %u", &coef);
        Coefficient = coef;
        EEPROM.writeUChar(EEPROM_ADDR_COEF, coef);
        EEPROM.commit();
    } else if (cmd.startsWith("sense ")) {
        uint8_t sense = 0;
        sscanf(cmd.c_str(), "sense %u", &sense);
        Sensitivity = sense;
        EEPROM.writeUChar(EEPROM_ADDR_SENSE, sense);
        EEPROM.commit();
    } else if (cmd.equals("status")) {
        SerialBT.printf("Volume : %d\n", Volume);
        SerialBT.printf("Bright : %d\n", CurrentBright);
        SerialBT.printf("Coefficient  : %d\n", Coefficient);
        SerialBT.printf("Sensitivity : %d\n", Sensitivity);
        SerialBT.printf("MaxAmplitudeOnPlay : %d\n", MaxAmplitudeOnPlay);
        SerialBT.printf("* Amp = (PeekToPeek / Coefficient) - Sensitivity\n");
    } else if (cmd.equals("next")) {
        dfmp3.nextTrack();
    } else if (cmd.startsWith("bright ")) {
        sscanf(cmd.c_str(), "bright %u", &CurrentBright);
    } else {
        SerialBT.printf("Unknown command\n");
    }
}

void setup() {

    ESP_LOGI(MAIN_TAG, "Setup!");

    EEPROM.begin(EEPROM_SIZE);
    Volume = EEPROM.read(EEPROM_ADDR_VOLUME);
    Coefficient = EEPROM.read(EEPROM_ADDR_COEF);
    if (Coefficient < 1) {
        Coefficient = 1;
    }
    Sensitivity = EEPROM.read(EEPROM_ADDR_VOLUME);

    xTaskCreate(
            taskStage,
            "taskStage",
            10000,
            nullptr,
            1,
            nullptr);

    xTaskCreate(
            taskBar,
            "taskBar",
            10000,
            nullptr,
            2,
            nullptr);

    pinMode(PIN_PLAYER_BUSY, INPUT);

    dfmp3.begin(9600, 1000);
    ESP_LOGI(MAIN_TAG, "dfplayer begin");
    dfmp3.delayForResponse(100);

    dfmp3.stop();
    dfmp3.delayForResponse(100);

    setupAudioInput();

    SerialBT.begin("DJ Gundam");
    SerialBT.register_callback([](esp_spp_cb_event_t evt, esp_spp_cb_param_t *param) {
        switch (evt) {
            case ESP_SPP_SRV_OPEN_EVT:
                sendCommandList();
                break;
            default:
                break;
        }
    });

}

bool firstTime = true;

void onPlayerBusy(unsigned long now) {
    int playerBusy = digitalRead(PIN_PLAYER_BUSY);
    if (playerBusy == HIGH) {
        if (firstTime) {
            ESP_LOGI(MAIN_TAG, "Play Start!");
            dfmp3.setVolume(Volume);
            dfmp3.delayForResponse(100);

            dfmp3.playRandomTrackFromAll();
            dfmp3.delayForResponse(100);

            MaxAmplitudeOnPlay = 0;

            firstTime = false;
        } else {
            //
        }
    }
}

void displayEnvelope() {
    unsigned int signalMax = 0;
    unsigned int signalMin = 4096;
    unsigned long chrono = micros(); // Sample window 10ms
    while (micros() - chrono < 10000ul) {
        int sample = adc1_get_raw(ADC_CHANNEL) / Coefficient;
        if (sample > signalMax) {
            signalMax = sample;
        } else if (sample < signalMin) {
            signalMin = sample;
        }
        vTaskDelay(1);
    }

    unsigned int peakToPeak = signalMax - signalMin;
    int amplitude = peakToPeak - Sensitivity;
    if (amplitude < 0) amplitude = 0;
    else if (amplitude > MAX_AMPLITUDE) amplitude = MAX_AMPLITUDE;
//    ESP_LOGD(MAIN_TAG, "amplitude : %3d", amplitude);
    LastAmplitude = amplitude;

    if (LastAmplitude > MaxAmplitudeOnPlay) {
        MaxAmplitudeOnPlay = LastAmplitude;
    }

    SuggestBarHeight = map(amplitude, 0, MAX_TARGET_AMPLITUDE, 0, STRIP_BAR_MAX_HEIGHT);
    SuggestBarHeight = min(SuggestBarHeight, STRIP_BAR_MAX_HEIGHT);
}

void loop() {
    unsigned long now = millis();

    dfmp3.loop();

    onPlayerBusy(now);

    displayEnvelope();

    if (SerialBT.available()) {
        String btCmd = SerialBT.readString();
        processCommand(btCmd);
    }

    ledController.loop(now);
}