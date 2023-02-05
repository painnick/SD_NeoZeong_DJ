#include <Arduino.h>

#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems

#include "esp_log.h"

#include "DFMiniMp3.h"
#include "Mp3Notify.h"
#include "AdcUtil.h"

#define MAIN_TAG "Main"

// ============================================================
// Pin Map
// ------------------------------------------------------------
// These are all GPIO pins on the ESP32
// Recommended pins include 2,4,12-19,21-23,25-27,32-33
// for the ESP32-S2 the GPIO pins are 1-21,26,33-42
//
// 13 outputs PWM signal at boot
// 14 outputs PWM signal at boot
// ============================================================
#define PIN_PLAYER_BUSY 33
#define PIN_DFPLAYER_RX 16
#define PIN_DFPLAYER_TX 17
#define PIN_MIC_SENSOR 35
#define PIN_SAMPLE_THRESHOLD 34
#define PIN_STRIP1 21

// ============================================================
// Dfplayer
// ------------------------------------------------------------
// Volume : 0~30
// ============================================================
#define DEFAULT_VOLUME 10
HardwareSerial mySerial(2); // 16, 17
DfMp3 dfmp3(mySerial);

// ============================================================
// Neopixel
// ============================================================
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define STRIP1_SIZE 34
#define COLOR_WIPE_WAIT 2

Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(STRIP1_SIZE, PIN_STRIP1, NEO_GRB + NEO_KHZ800);
uint32_t strip1Color;

// Fill the dots one after the other with a color
void colorWipe(Adafruit_NeoPixel& strip, uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip1.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void task1(void* params) {
  while(true) {
    // ESP_LOGI(MAIN_TAG, "COLOR %d, %d, %d", (strip1Color >> 16) & 0xff, (strip1Color >> 8) & 0xff, strip1Color & 0xff);
    colorWipe(strip1, strip1Color, COLOR_WIPE_WAIT);
  }
  vTaskDelete(NULL);
}

void setup() {

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
        dfmp3.nextTrack();
        dfmp3.delayForResponse(100);

        lastMp3Busy = 0;
      }
    }
  } else {
        lastMp3Busy = 0;
  }

  int base = analogRead(PIN_SAMPLE_THRESHOLD);
  int sample = adc1_get_raw(ADC1_CHANNEL_7);
  int diff = sample - base;

  if (diff > 20) {
    ESP_LOGD(MAIN_TAG, "Base %4d, Sample %4d (%4d)", base, sample, diff);
  }

  uint32_t color = (diff > 20) ? strip1.Color(map(diff, 0, 127, 0, 255), 0, 0) : strip1.Color(0, 0, 0);

  uint8_t r = (color >> 16) & 0xff;
  uint8_t g = (color >> 8) & 0xff;
  uint8_t b = color & 0xff;

  if (now - lastStrip1Changed > STRIP1_SIZE * COLOR_WIPE_WAIT) {
    strip1Color = strip1.Color(maxR, maxG, maxB);
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
