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
#define DEFAULT_VOLUME 10
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

#define SAMPLE_GAIN_MIN 0 // 800
#define SAMPLE_GAIN_MAX (512 + 128) // 2048

#define SAMPLES_QUEUE_SIZE 1

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar1 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar2 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR2, NEO_GRB + NEO_KHZ800);

int GlobalBarHeight = 0;
int GlobalCurrentSample = 0;

[[noreturn]] void taskStage(void *params) {
    while (true) {
        theaterChaseRainbow(stripStage, 100);
    }
}

uint32_t calcColor(int height) {
    uint32_t color = 0;
    if (height < 3) {
        color = Adafruit_NeoPixel::Color(0, 0, currentBright);
    } else if (height < 5) {
        color = Adafruit_NeoPixel::Color(0, currentBright, 0);
    } else {
        color = Adafruit_NeoPixel::Color(currentBright, 0, 0);
    }
    return color;
}

unsigned long lastBarChecked = 0;

[[noreturn]] void taskBar(void *params) {
    int lastHeight = 0;
    bool off = false;

    while (true) {
        unsigned long now = millis();
        int capturedGlobalBarHeight = GlobalBarHeight;
        if (capturedGlobalBarHeight > lastHeight) {
            ESP_LOGD(MAIN_TAG, "Bar == UP == Height %d (Last %d) (%4d)", capturedGlobalBarHeight, lastHeight,
                     GlobalCurrentSample);
            lastHeight = capturedGlobalBarHeight;
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
                    ESP_LOGD(MAIN_TAG, "Bar .-down-.  Height %d (Current %d) (%4d)", lastHeight,
                             capturedGlobalBarHeight, GlobalCurrentSample);

                    uint32_t color = calcColor(min(lastHeight, STRIP_BAR_MAX_HEIGHT));
                    colorHeight(stripBar1, stripBar2, color, lastHeight);
                    lastBarChecked = now;
                }
            }
        }
        delay(10);
    }
}

void setup() {

    ESP_LOGI(MAIN_TAG, "Setup!");

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
}

bool firstTime = true;

void onPlayerBusy(unsigned long now) {
    int playerBusy = digitalRead(PIN_PLAYER_BUSY);
    if (playerBusy == HIGH) {
        if (firstTime) {
            ESP_LOGI(MAIN_TAG, "Play Start!");
            dfmp3.setVolume(DEFAULT_VOLUME);
            dfmp3.delayForResponse(100);

            dfmp3.playRandomTrackFromAll();
            dfmp3.delayForResponse(100);

            firstTime = false;
        } else {
            //
        }
    }
}

void calcBarHeight(int diff) {
    int barValue = map(diff, SAMPLE_GAIN_MIN, SAMPLE_GAIN_MAX, 0, STRIP_BAR_MAX_HEIGHT);
    GlobalBarHeight = max(0, min(barValue, STRIP_BAR_MAX_HEIGHT));
}

int Queue[SAMPLES_QUEUE_SIZE] = {-1};
int queueIndex = -1;

void averageSamples(int sample) {
    queueIndex = (queueIndex + 1) % SAMPLES_QUEUE_SIZE;
    Queue[queueIndex] = sample;

    int sum = 0, count = 0;
    for (int val: Queue) {
        if (val > -1) {
            sum += val;
            count++;
        }
    }
    GlobalCurrentSample = sum / count;
}

//unsigned long lastChecked = millis();
void loop() {
    unsigned long now = millis();

    dfmp3.loop();

    onPlayerBusy(now);

    int sample = adc1_get_raw(ADC_CHANNEL);
    averageSamples(sample);

//    if (now - lastChecked > 100) {
//        ESP_LOGD(MAIN_TAG, "Sample %d => %d", sample, GlobalCurrentSample);
//        lastChecked = now;
//    }

    calcBarHeight(GlobalCurrentSample);
}
