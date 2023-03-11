#include <Arduino.h>

#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems

#include "esp_log.h"

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
#define STRIP_BAR_SIZE 8

#define COLOR_WIPE_WAIT 2

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar1 = Adafruit_NeoPixel(STRIP_BAR_SIZE, PIN_STRIP_BAR1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar2 = Adafruit_NeoPixel(STRIP_BAR_SIZE, PIN_STRIP_BAR2, NEO_GRB + NEO_KHZ800);

uint32_t stageColor;
int barHeight;
int currentVolume = DEFAULT_VOLUME;
int currentSampleThreshold = 0;

void taskStage(void* params) {
  while(true) {
    theaterChaseRainbow(stripStage, 100);
  }
  vTaskDelete(NULL);
}

unsigned long lastBarChecked = 0;
void taskBar(void* params) {
  int lastHeight = 0;

  while(true) {
    if (lastHeight != 0) {
      ESP_LOGD(MAIN_TAG, "Bar %d", lastHeight);
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
      color = stripBar1.Color(0, 0, currentBright);
    } else if (height < 5) {
      color = stripBar1.Color(0, currentBright, 0);
    } else  {
      color = stripBar1.Color(currentBright, 0, 0);
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
  pinMode(PIN_PLAYER_VOLUME, INPUT_PULLDOWN);
  pinMode(PIN_BRIGHT, INPUT_PULLDOWN);

  dfmp3.begin(9600, 1000);
  ESP_LOGI(MAIN_TAG, "dfplayer begin");
  dfmp3.delayForResponse(100);

  dfmp3.stop();
  dfmp3.delayForResponse(100);

  setupAudioInput();

}

unsigned long lastBarChanged = 0;

unsigned long lastPlayerBusy = 0;
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
      // if (lastPlayerBusy == 0) {
      //   ESP_LOGI(MAIN_TAG, "Player B.U.S.Y!");

      //   dfmp3.stop();
      //   dfmp3.delayForResponse(100);

      //   lastPlayerBusy = now;
      // } else {
      //   if (now - lastPlayerBusy > 1000 * 30) {
      //     ESP_LOGI(MAIN_TAG, "Play Next! BR %d VOL %d TH %d", currentBright, currentVolume, currentSampleThreshold);
      //     dfmp3.setVolume(DEFAULT_VOLUME);
      //     dfmp3.delayForResponse(100);

      //     dfmp3.nextTrack();
      //     dfmp3.delayForResponse(100);

      //     lastPlayerBusy = 0;
      //   }
      // }
    }
  } else {
    lastPlayerBusy = 0;
  }
}

unsigned long lastVolumeChecked = 0;
void checkPlayerVolume(unsigned long now) {
  if (now - lastVolumeChecked > 500) {
    int volume = analogRead(PIN_PLAYER_VOLUME);
    volume = map(volume, 0, 4095, 0, 20);
    
    if (currentVolume != volume) {
      currentVolume = volume;

      dfmp3.setVolume(currentVolume);
      ESP_LOGI(MAIN_TAG, "Set volume %d", currentVolume);
      dfmp3.delayForResponse(100);
    }

    lastVolumeChecked = now;
  }
}

unsigned long lastBrightChecked = 0;
void checkBirght(unsigned long now) {
  if (now - lastBrightChecked > 500) {
    int bright = analogRead(PIN_BRIGHT);
    bright = map(bright, 0, 4095, 0, 256);
    if (currentBright != bright) {
      currentBright = bright;
      ESP_LOGI(MAIN_TAG, "Set Bright %d", currentBright);
    }

    lastBrightChecked = now;
  }
}

#define MAX_SAMPLE_THRESHOLD 100
int thresholds[MAX_SAMPLE_THRESHOLD] = {-1};
int thresholdIndex = -1;
int avgSampleThreshold(int threshold) {
  thresholdIndex = (thresholdIndex + 1) % MAX_SAMPLE_THRESHOLD;
  thresholds[thresholdIndex] = threshold;

  int thresholdSum = 0;
  for (int i = 0; i < MAX_SAMPLE_THRESHOLD; i ++) {
    if (thresholds[i] == -1) {
      return threshold;
    }
    thresholdSum += threshold;
  }

  return thresholdSum / MAX_SAMPLE_THRESHOLD;
}

unsigned long lastSampleThresholdChecked = 0;
void checkSampleThreshold(unsigned long now) {
  if (now - lastSampleThresholdChecked > 1000) {
    int threshold = analogRead(PIN_SAMPLE_THRESHOLD);

    int avgThreshold = avgSampleThreshold(threshold);

    if (currentSampleThreshold != avgThreshold) {
      currentSampleThreshold = avgThreshold;
      // ESP_LOGI(MAIN_TAG, "Set Sample Threshold %d", currentSampleThreshold);
    }

    lastSampleThresholdChecked = now;
  }
}

void checkBarInfo(unsigned long now, int diff) {
  if (now - lastBarChanged > 100) {
    barHeight = 0;

    lastBarChanged = now;
  } else {
    int barValue = map(diff, 0, 512, 0, STRIP_BAR_SIZE);
    barHeight = max(barHeight, barValue);
  }
}

void loop() {

  unsigned long now = millis();

  dfmp3.loop();

  onPlayerBusy(now);

  checkPlayerVolume(now);
  checkBirght(now);
  checkSampleThreshold(now);



  int sample = adc1_get_raw(ADC_CHANNEL);
  int diff = sample - currentSampleThreshold;

  // if (diff > 20) {
  //   ESP_LOGD(MAIN_TAG, "Sample %4d (%4d)", sample, diff);
  // }

  checkBarInfo(now, diff);

  delay(100);
}
