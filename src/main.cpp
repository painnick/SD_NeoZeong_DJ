#include <Arduino.h>

#include "common.h"
#include "DFMiniMp3.h"
#include "Mp3Notify.h"
#include "AdcUtil.h"

#include "Effect.h"

#define MAIN_TAG "Main"

// ============================================================
// Dfplayer
// ------------------------------------------------------------
// Volume : 0~30
// ============================================================
int MP3_VOLUME = 10;

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

// ============================================================

int GlobalBarHeight = 0;

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
        int capturedGlobalBarHeight = GlobalBarHeight;
        if (capturedGlobalBarHeight > lastHeight) {
            ESP_LOGD(MAIN_TAG, "Bar == UP == Height %d (Last %d)", capturedGlobalBarHeight, lastHeight);
            lastHeight = capturedGlobalBarHeight;
            uint32_t color = calcColor(min(lastHeight, STRIP_BAR_MAX_HEIGHT));
            colorHeight(stripBar1, stripBar2, color, lastHeight);
            lastBarChecked = now;
            off = false;
        } else {
            if (now - lastBarChecked > 50) {
                lastHeight--;
                if (lastHeight < 0) {
                    lastHeight = 0;
                    off = true;
                }
                if (!off) {
                    lastHeight = max(lastHeight, 0);
                    ESP_LOGD(MAIN_TAG, "Bar .-down-.  Height %d (Current %d)", lastHeight, capturedGlobalBarHeight);

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
    SerialBT.println("volume");
    SerialBT.println("gain %d %d");
    SerialBT.println("gain");
    SerialBT.println("next");
    SerialBT.println("--- End ---");
}

void processCommand(const String &cmd) {
    if (cmd.equals("help")) {
        sendCommandList();
    } else if (cmd.equals("volume")) {
        uint8_t volume = dfmp3.getVolume();
        SerialBT.printf("Volume is %d\n", volume);
    } else if (cmd.startsWith("volume ")) {
        uint8_t vol = 0;
        sscanf(cmd.c_str(), "volume %u", &vol);
        dfmp3.setVolume(vol);
        dfmp3.loop();
        EEPROM.writeUChar(EEPROM_ADDR_VOLUME, vol);
        EEPROM.commit();
    } else if (cmd.equals("next")) {
        dfmp3.nextTrack();
    } else {
        SerialBT.printf("Unknown command\n");
    }
}

void setup() {

    ESP_LOGI(MAIN_TAG, "Setup!");

    EEPROM.begin(EEPROM_SIZE);
    MP3_VOLUME = EEPROM.read(EEPROM_ADDR_VOLUME);

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

    SerialBT.begin("DJ Drop the BEAT!");
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
            dfmp3.setVolume(MP3_VOLUME);
            dfmp3.delayForResponse(100);

            dfmp3.playRandomTrackFromAll();
            dfmp3.delayForResponse(100);

            firstTime = false;
        } else {
            //
        }
    }
}

#define COEF3 3
#define SENSITIVITY 30 // decrease for better sensitivity

void displayEnvelope() {
    unsigned int signalMax = 0;
    unsigned int signalMin = 4096;
    unsigned long chrono = micros(); // Sample window 10ms
    while (micros() - chrono < 10000ul) {
        int sample = adc1_get_raw(ADC_CHANNEL) / COEF3;
        if (sample > signalMax) {
            signalMax = sample;
        } else if (sample < signalMin) {
            signalMin = sample;
        }
        vTaskDelay(1);
    }

    unsigned int peakToPeak = signalMax - signalMin;
    int amplitude = peakToPeak - SENSITIVITY;
    if (amplitude < 0) amplitude = 0;
    else if (amplitude > 500) amplitude = 500;
//    ESP_LOGD(MAIN_TAG, "amplitude : %3d", amplitude);

    GlobalBarHeight = map(amplitude, 0, 250, 0, STRIP_BAR_MAX_HEIGHT);
    GlobalBarHeight = min(GlobalBarHeight, STRIP_BAR_MAX_HEIGHT);
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
}