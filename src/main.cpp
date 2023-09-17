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
// EEPROM Manager
// ============================================================
#include "EEPROMManager.h"

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

[[noreturn]] void taskStage(__attribute__((unused)) void *params) {
    while (true) {
        theaterChaseRainbow(stripStage, 100);
    }
}

unsigned long lastBarChecked = 0;

[[noreturn]] void taskBar(__attribute__((unused)) void *params) {
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
    char *pos = nullptr;
    if (cmd.equals("help")) {
        sendCommandList();
    } else if (cmd.startsWith("volume ")) {
        uint8_t vol = strtoul(cmd.c_str() + 7, &pos, 10);
        dfmp3.setVolume(vol);
        dfmp3.loop();
        Volume = vol;
        EEPROMManager::writeVolume(vol);
    } else if (cmd.startsWith("bright ")) {
        uint8_t bright = strtoul(cmd.c_str() + 7, &pos, 10);
        CurrentBright = bright;
        EEPROMManager::writeBrightness(bright);
    } else if (cmd.startsWith("coef ")) {
        uint8_t coef = strtoul(cmd.c_str() + 5, &pos, 10);
        Coefficient = coef;
        EEPROMManager::writeCoefficient(coef);
    } else if (cmd.startsWith("sense ")) {
        uint8_t sense = strtoul(cmd.c_str() + 6, &pos, 10);
        Sensitivity = sense;
        EEPROMManager::writeSensitivity(sense);
    } else if (cmd.equals("status")) {
        SerialBT.printf("Volume : %d\n", Volume);
        SerialBT.printf("Bright : %d\n", CurrentBright);
        SerialBT.printf("Coefficient  : %d\n", Coefficient);
        SerialBT.printf("Sensitivity : %d\n", Sensitivity);
        SerialBT.printf("MaxAmplitudeOnPlay : %d\n", MaxAmplitudeOnPlay);
        SerialBT.printf("* Amp = (PeekToPeek / Coefficient) - Sensitivity\n");
    } else if (cmd.equals("next")) {
        dfmp3.nextTrack();
    } else {
        SerialBT.printf("Unknown command\n");
    }
}

void setup() {

    ESP_LOGI(MAIN_TAG, "Setup!");

    EEPROMManager::init();
    Volume = EEPROMManager::readVolume();
    Coefficient = EEPROMManager::readCoefficient();
    Sensitivity = EEPROMManager::readSensitivity();
    CurrentBright = EEPROMManager::readBrightness();

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

    ledController.setInterval(300);

    ledController.setSeq(0, new boolean[10]{true, false}, 10); // Green
    ledController.setSeq(1, new boolean[7]{true, false}, 7); // Green
    ledController.setSeq(2, new boolean[5]{true, false}, 5); // Red
    ledController.setSeq(3, new boolean[5]{true, false}, 5); // Red
    ledController.setSeq(4, new boolean[12]{true, false}, 12); // Yellow
    ledController.setSeq(5, new boolean[12]{true, false}, 12); // Yellow
    ledController.setSeq(6, new boolean[7]{true, false}, 7); // Blue
    ledController.setSeq(7, new boolean[7]{true, false}, 7); // Blue
    ledController.setSeq(8, new boolean[4]{true, false}, 4); // Blue
    ledController.setSeq(9, new boolean[4]{true, false}, 4); // Blue

    ledController.setSeq(10, new boolean[6]{true, false}, 6);
    ledController.setSeq(11, new boolean[6]{true, false}, 6);
    ledController.setSeq(12, new boolean[15]{true, false}, 15);
    ledController.setSeq(13, new boolean[15]{true, false}, 15);
    ledController.setSeq(14, new boolean[4]{true, false}, 4);
    ledController.setSeq(15, new boolean[4]{true, false}, 4);
}

bool firstTime = true;

void onPlayerBusy() {
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
    int signalMax = 0;
    int signalMin = 4096;
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

    int peakToPeak = signalMax - signalMin;
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

    onPlayerBusy();

    displayEnvelope();

    if (SerialBT.available()) {
        String btCmd = SerialBT.readString();
        processCommand(btCmd);
    }

    ledController.loop(now);
}