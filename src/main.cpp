#include <Arduino.h>

// #include <Adafruit_NeoPixel.h>
// #ifdef __AVR__
//   #include <avr/power.h>
// #endif

#include "soc/soc.h"             // disable brownout problems
#include "soc/rtc_cntl_reg.h"    // disable brownout problems

#include "esp_log.h"
#include "esp_adc_cal.h"

// calibration values for the adc
#define DEFAULT_VREF 1100
esp_adc_cal_characteristics_t *adc_chars;

#define USE_SOUND

#define MAIN_TAG "Main"

#ifdef USE_SOUND
  #include "DFMiniMp3.h"
  #include "Mp3Notify.h"
  HardwareSerial mySerial(2); // 16, 17
  DfMp3 dfmp3(mySerial);
  int volume = 10; // 0~30
#endif

// These are all GPIO pins on the ESP32
// Recommended pins include 2,4,12-19,21-23,25-27,32-33
// for the ESP32-S2 the GPIO pins are 1-21,26,33-42

// 13 outputs PWM signal at boot
// 14 outputs PWM signal at boot

void setupAudioInput() {
  //Range 0-4096
  adc1_config_width(ADC_WIDTH_BIT_12);
  // full voltage range
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

  // check to see what calibration is available
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
    ESP_LOGI(MAIN_TAG, "Using voltage ref stored in eFuse");
  }
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
    ESP_LOGI(MAIN_TAG, "Using two point values from eFuse");
  }
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_DEFAULT_VREF) == ESP_OK) {
    ESP_LOGI(MAIN_TAG, "Using default VREF");
  }
  //Characterize ADC
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);  
}

void dfmp3Delay(DfMp3& dfmp3, int ms) {
  int cnt = ms / 10;
  for(int i = 0; i < cnt; i ++) {
    dfmp3.loop();
    delay(10);
  }
}

void setup() {

#ifdef USE_SOUND
  // delay(2000);

  dfmp3.begin(9600, 1000);
  ESP_LOGI(MAIN_TAG, "dfplayer begin");
  dfmp3Delay(dfmp3, 100);

  dfmp3.setVolume(volume);
  ESP_LOGI(MAIN_TAG, "Set volume %d", volume);
  dfmp3Delay(dfmp3, 100);
#endif

  setupAudioInput();

#ifdef USE_SOUND
  dfmp3.setRepeatPlayCurrentTrack(true);
  dfmp3Delay(dfmp3, 100);

  dfmp3.playMp3FolderTrack(1);
  dfmp3Delay(dfmp3, 100);

  // dfmp3.setEq(DfMp3_Eq_Bass);
  // dfmp3.enableDac();
#endif
}

unsigned long lastTime = 0;

#define SAMPLE_COUNT 100
int sampleIndex = 0;
int currentMax = 0;
int currentMin = 4096;

void loop() {

#ifdef USE_SOUND
  dfmp3.loop();
#endif

  int sample = adc1_get_raw(ADC1_CHANNEL_7);
  if (sampleIndex == 0) {
    currentMin = sample;
    currentMax = sample;
  } else {
    currentMin = min(currentMin, sample);
    currentMax = max(currentMax, sample);
  }
  if (sampleIndex == SAMPLE_COUNT - 1) {
    ESP_LOGD(MAIN_TAG, "Min=%4d, Max=%4d (%3d)", currentMin, currentMax, currentMax - currentMin);
  }

  sampleIndex = (sampleIndex + 1) % SAMPLE_COUNT;

  // unsigned long now = millis();
  // if (now - lastTime > 100) {
  //   lastTime = now;
  // }

  delay(10);
}
