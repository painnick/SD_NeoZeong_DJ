#include <Arduino.h>

#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems

#include "esp_log.h"

#include "common.h"
#include "DFMiniMp3.h"
#include "Mp3Notify.h"
#include "AdcUtil.h"

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
#define STRIP_BAR_SIZE 8

#define COLOR_WIPE_WAIT 2

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar1 = Adafruit_NeoPixel(STRIP_BAR_SIZE, PIN_STRIP_BAR1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar2 = Adafruit_NeoPixel(STRIP_BAR_SIZE, PIN_STRIP_BAR2, NEO_GRB + NEO_KHZ800);

uint32_t stageColor;
int barHeight;

void colorWipe(Adafruit_NeoPixel& strip, uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void colorHeight(Adafruit_NeoPixel& strip1, Adafruit_NeoPixel& strip2, uint32_t c, int height) {
  for (uint16_t i = 0; i < strip1.numPixels(); i++) {
    strip1.setPixelColor(i, (i < height) ? c : 0);
  }
  for (uint16_t i = 0; i < strip2.numPixels(); i++) {
    strip2.setPixelColor(i, (i < height) ? c : 0);
  }
  strip1.show();
  strip2.show();
}

void taskStage(void* params) {
  while(true) {
    colorWipe(stripStage, stageColor, COLOR_WIPE_WAIT);
  }
  vTaskDelete(NULL);
}

unsigned long lastBarChecked = 0;
void taskBar(void* params) {
  int lastHeight = 0;

  while(true) {
    if (lastHeight != 0) {
      ESP_LOGI(MAIN_TAG, "Bar %d", lastHeight);
    }
    if (barHeight > lastHeight) {
      lastHeight = barHeight;
    } else {
      lastHeight --;
      lastHeight = max(lastHeight, 0);
    }

    uint32_t color = stripBar1.Color(0, 0, 0);
    int height = min(lastHeight, STRIP_BAR_SIZE);
    if (height < 3) {
      color = stripBar1.Color(0, 0, 64);
    } else if (height < 5) {
      color = stripBar1.Color(0, 64, 0);
    } else  {
      color = stripBar1.Color(64, 0, 0);
    }
    colorHeight(stripBar1, stripBar2, color, lastHeight);

    while (true)
    {
      unsigned long now = millis();
      if (now - lastBarChecked < 100) {
        delay(5);
      } else {
        lastBarChecked = now;
        break;
      }
    }
  }
  vTaskDelete(NULL);
}

void setup() {

  ESP_LOGI(MAIN_TAG, "Setup!");

  xTaskCreate(
    taskStage,
    "taskStage",
    10000,
    NULL,
    1,
    NULL);

  xTaskCreate(
    taskBar,
    "taskBar",
    10000,
    NULL,
    2,
    NULL);

  pinMode(PIN_PLAYER_BUSY, INPUT);

  dfmp3.begin(9600, 1000);
  ESP_LOGI(MAIN_TAG, "dfplayer begin");
  dfmp3.delayForResponse(100);

  dfmp3.setVolume(DEFAULT_VOLUME);
  ESP_LOGI(MAIN_TAG, "Set volume %d", DEFAULT_VOLUME);
  dfmp3.delayForResponse(100);

  setupAudioInput();

  dfmp3.playMp3FolderTrack(1);
  ESP_LOGI(MAIN_TAG, "Play #%d", 1);
  dfmp3.delayForResponse(100);
}

unsigned long lastMp3Busy = 0;
unsigned long lastStrip1Changed = 0;
unsigned long lastBarChanged = 0;
uint8_t maxR = 0;
uint8_t maxG = 0;
uint8_t maxB = 0;

void loop() {

  unsigned long now = millis();

  dfmp3.loop();

  int mp3busy = digitalRead(PIN_PLAYER_BUSY);
  if (mp3busy == HIGH) {
    if (lastMp3Busy == 0) {
      lastMp3Busy = now;
    } else {
      if (now - lastMp3Busy > 2000) {
        dfmp3.setVolume(DEFAULT_VOLUME);
        dfmp3.delayForResponse(100);

        dfmp3.nextTrack();
        dfmp3.delayForResponse(100);

        lastMp3Busy = 0;
      }
    }
  } else {
    lastMp3Busy = 0;
  }

  int base = analogRead(PIN_SAMPLE_THRESHOLD);
  int sample = adc1_get_raw(ADC_CHANNEL);
  int diff = sample - base;

  // if (diff > 20) {
  //   ESP_LOGD(MAIN_TAG, "Base %4d, Sample %4d (%4d)", base, sample, diff);
  // }

  uint16_t value = map(diff, 0, 127, 0, 255);
  uint32_t color = (diff > 20) ? stripStage.Color(value, 0, 0) : stripStage.Color(0, 0, 0);

  uint8_t r = (color >> 16) & 0xff;
  uint8_t g = (color >> 8) & 0xff;
  uint8_t b = color & 0xff;

  if (now - lastStrip1Changed > STRIP_STAGE_SIZE * COLOR_WIPE_WAIT) {
    stageColor = stripStage.Color(maxR, maxG, maxB);
    // if (maxR + maxG + maxB != 0) {
    //   ESP_LOGI(MAIN_TAG, "COLOR %d, %d, %d", maxR, maxG, maxB);
    // }
    // Reset
    maxR = r;
    maxG = g;
    maxB = g;

    lastStrip1Changed = now;
  } else {
    maxR = max(maxR, r);
    maxG = max(maxG, g);
    maxB = max(maxB, b);

  }

  if (now - lastBarChanged > 100) {
    barHeight = 0;

    lastBarChanged = now;
  } else {
    int barValue = map(diff, 0, 512, 0, STRIP_BAR_SIZE);
    barHeight = max(barHeight, barValue);
  }
}
