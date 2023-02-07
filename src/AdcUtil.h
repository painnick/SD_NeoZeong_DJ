// ------------------------------------------------------------
// Calibration values for the adc
// ------------------------------------------------------------

#pragma once

#include "esp_adc_cal.h"
#include "esp_log.h"

#include "common.h"

#define ADC_TAG "AdcUtil"

#define DEFAULT_VREF 1100
esp_adc_cal_characteristics_t *adc_chars;

extern void setupAudioInput() {
  //Range 0-4096
  adc1_config_width(ADC_WIDTH_BIT_12);
  // full voltage range
  adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);

  // check to see what calibration is available
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
    ESP_LOGI(ADC_TAG, "Using voltage ref stored in eFuse");
  }
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
    ESP_LOGI(ADC_TAG, "Using two point values from eFuse");
  }
  if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_DEFAULT_VREF) == ESP_OK) {
    ESP_LOGI(ADC_TAG, "Using default VREF");
  }
  //Characterize ADC
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);  
}

