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

#define STRIP_STAGE_SIZE 34
#define COLOR_WIPE_WAIT 2

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
uint32_t stripStageColor;

void colorWipe(Adafruit_NeoPixel& strip, uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < stripStage.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void task1(void* params) {
  while(true) {
    // ESP_LOGI(MAIN_TAG, "COLOR %d, %d, %d", (stripStageColor >> 16) & 0xff, (stripStageColor >> 8) & 0xff, stripStageColor & 0xff);
    colorWipe(stripStage, stripStageColor, COLOR_WIPE_WAIT);
  }
  vTaskDelete(NULL);
}

void setup() {

  ESP_LOGI(MAIN_TAG, "Setup!");

  xTaskCreate(
    task1,
    "task1",
    10000,
    NULL,
    1,
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

  if (diff > 20) {
    ESP_LOGD(MAIN_TAG, "Base %4d, Sample %4d (%4d)", base, sample, diff);
  }

  uint32_t color = (diff > 20) ? stripStage.Color(map(diff, 0, 127, 0, 255), 0, 0) : stripStage.Color(0, 0, 0);

  uint8_t r = (color >> 16) & 0xff;
  uint8_t g = (color >> 8) & 0xff;
  uint8_t b = color & 0xff;

  if (now - lastStrip1Changed > STRIP_STAGE_SIZE * COLOR_WIPE_WAIT) {
    stripStageColor = stripStage.Color(maxR, maxG, maxB);
    if (maxR + maxG + maxB != 0) {
      ESP_LOGI(MAIN_TAG, "COLOR %d, %d, %d", maxR, maxG, maxB);
    }
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
}
