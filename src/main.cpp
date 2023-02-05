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
#define PIN_THRESHOLD 33
#define PIN_DFPLAYER_RX 16
#define PIN_DFPLAYER_TX 17

// ============================================================
// Dfplayer
// ------------------------------------------------------------
// Volume : 0~30
// ============================================================
#define DEFAULT_VOLUME (10)
HardwareSerial mySerial(2); // 16, 17
DfMp3 dfmp3(mySerial);

// ============================================================
// Neopixel
// ============================================================
// #include <Adafruit_NeoPixel.h>
// #ifdef __AVR__
//   #include <avr/power.h>
// #endif


void setup() {

  pinMode(PIN_THRESHOLD, INPUT);

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

unsigned long lastBusy = 0;

void loop() {

  unsigned long now = millis();

  dfmp3.loop();

  int mp3busy = digitalRead(PIN_THRESHOLD);
  if (mp3busy == HIGH) {
    if (lastBusy == 0) {
      lastBusy = now;
    } else {
      if (now - lastBusy > 2000) {
        dfmp3.nextTrack();
        dfmp3.delayForResponse(100);

        lastBusy = 0;
      }
    }
  } else {
        lastBusy = 0;
  }

  int base = analogRead(34);
  int sample = adc1_get_raw(ADC1_CHANNEL_7);
  int diff = sample - base;

  if (diff > 20) {
    ESP_LOGD(MAIN_TAG, "Base %4d, Sample %4d (%4d)", base, sample, diff);
  }

  // unsigned long now = millis();
  // if (now - lastTime > 100) {
  //   lastTime = now;
  // }

  // delay(100);
}
