#pragma once

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
#define PIN_PLAYER_VOLUME 27

#define PIN_SAMPLE_THRESHOLD 34
#define PIN_STRIP_STAGE 21
#define PIN_STRIP_BAR1 22
#define PIN_STRIP_BAR2 23
#define PIN_BRIGHT 26

// ============================================================
// RX/TX for dfplayer
// ------------------------------------------------------------
// Default : GPIO16, 17
// ============================================================
#define UART_PLAYER 2

// ============================================================
// ADC : Mic Sensor
// ------------------------------------------------------------
// Default : GPIO35
// ============================================================
#define ADC_CHANNEL ADC1_CHANNEL_7
