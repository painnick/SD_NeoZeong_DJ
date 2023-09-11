#include <Arduino.h>

#include "common.h"
#include "DFMiniMp3.h"
#include "Mp3Notify.h"

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

int SAMPLE_GAIN_MIN = 0; // 800
int SAMPLE_GAIN_MAX = 511; // 2048

Adafruit_NeoPixel stripStage = Adafruit_NeoPixel(STRIP_STAGE_SIZE, PIN_STRIP_STAGE, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar1 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel stripBar2 = Adafruit_NeoPixel(STRIP_BAR_MAX_HEIGHT, PIN_STRIP_BAR2, NEO_GRB + NEO_KHZ800);

// ============================================================
// Bluetooth
// ============================================================
//#define USE_BLUETOOTH 1

#ifdef USE_BLUETOOTH
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;
#endif

// ============================================================
// EEPROM
// ============================================================
#include <EEPROM.h>

#define EEPROM_SIZE (1 + 4 + 4)
#define EEPROM_ADDR_VOLUME 0
#define EEPROM_ADDR_GAIN_MIN 1
#define EEPROM_ADDR_GAIN_MAX (1 + 4)

// ============================================================
// I2S
// ============================================================
#include "driver/i2s.h"
#include "I2SMEMSSampler.h"

I2SSampler *i2sSampler = NULL;

// i2s config for reading from left channel of I2S
i2s_config_t i2sMemsConfigLeftChannel = {
        .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

// i2s pins
i2s_pin_config_t i2sPins = {
        .bck_io_num = GPIO_NUM_17,
        .ws_io_num = GPIO_NUM_18,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = GPIO_NUM_39};

// how many samples to read at once
const int SAMPLE_SIZE = 16000 / 100;

// Task to write samples to our server
void i2sMemsWriterTask(void *param) {
    auto *sampler = (I2SSampler *) param;
    auto *samples = (int16_t *) malloc(sizeof(uint16_t) * SAMPLE_SIZE);
    if (!samples) {
        Serial.println("Failed to allocate memory for samples");
        return;
    }
    while (true) {
        int samples_read = sampler->read(samples, SAMPLE_SIZE);
        if (samples_read > 2) {
            uint16_t maxV = 0;
            for (int i = 0; i < samples_read; i++) {
                uint16_t val = samples[i];
                maxV = val > maxV ? val : maxV;
            }
            ESP_LOGD(MAIN_TAG, "I2S Read (%d) Max %10u [%10u %10u %10u %10u %10u %10u %10u %10u %10u %10u]", samples_read, maxV, samples[0], samples[1],
                     samples[2], samples[3], samples[4], samples[5], samples[6], samples[7], samples[8], samples[9]);
        }
    }
}

// ============================================================

int GlobalBarHeight = 0;
int GlobalCurrentSample = 0;

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
            ESP_LOGD(MAIN_TAG, "Bar == UP == Height %d (Last %d) (%4d)", capturedGlobalBarHeight, lastHeight,
                     GlobalCurrentSample);
            lastHeight = capturedGlobalBarHeight;
            uint32_t color = calcColor(min(lastHeight, STRIP_BAR_MAX_HEIGHT));
            colorHeight(stripBar1, stripBar2, color, lastHeight);
            lastBarChecked = now;
            off = false;
        } else {
            if (now - lastBarChecked > 200) {
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
        vTaskDelay(1);
    }
}

#ifdef USE_BLUETOOTH
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
    } else if (cmd.equals("gain")) {
        SerialBT.printf("Gain is %d ~ %d\n", SAMPLE_GAIN_MIN, SAMPLE_GAIN_MAX);
    } else if (cmd.startsWith("gain ")) {
        int minGain = 0, maxGain = 0;
        sscanf(cmd.c_str(), "gain %d %d", &minGain, &maxGain);
        SAMPLE_GAIN_MIN = minGain;
        SAMPLE_GAIN_MAX = maxGain;
        EEPROM.writeInt(EEPROM_ADDR_GAIN_MIN, SAMPLE_GAIN_MIN);
        EEPROM.writeInt(EEPROM_ADDR_GAIN_MAX, SAMPLE_GAIN_MAX);
        EEPROM.commit();
    } else if (cmd.equals("next")) {
        dfmp3.nextTrack();
    } else {
        SerialBT.printf("Unknown command\n");
    }
}
#endif

void setup() {

    ESP_LOGI(MAIN_TAG, "Setup!");

    EEPROM.begin(EEPROM_SIZE);
    MP3_VOLUME = EEPROM.read(EEPROM_ADDR_VOLUME);
    SAMPLE_GAIN_MIN = EEPROM.readInt(EEPROM_ADDR_GAIN_MIN);
    SAMPLE_GAIN_MAX = EEPROM.readInt(EEPROM_ADDR_GAIN_MAX);

    i2sSampler = new I2SMEMSSampler(I2S_NUM_0, i2sPins, i2sMemsConfigLeftChannel, false);
    i2sSampler->start();

    TaskHandle_t i2sMemsWriterTaskHandle;
    xTaskCreatePinnedToCore(i2sMemsWriterTask, "I2S Writer Task", 4096, i2sSampler, 1, &i2sMemsWriterTaskHandle, 1);

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

#ifdef USE_BLUETOOTH
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
#endif
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

void calcBarHeight(int diff) {
    int barValue = map(diff, SAMPLE_GAIN_MIN, SAMPLE_GAIN_MAX, 0, STRIP_BAR_MAX_HEIGHT);
    GlobalBarHeight = max(0, min(barValue, STRIP_BAR_MAX_HEIGHT));
}

//unsigned long lastChecked = millis();
void loop() {
    unsigned long now = millis();

    dfmp3.loop();

    onPlayerBusy(now);

//    if (now - lastChecked > 100) {
//        ESP_LOGD(MAIN_TAG, "Sample %d => %d", sample, GlobalCurrentSample);
//        lastChecked = now;
//    }

    calcBarHeight(GlobalCurrentSample);

#ifdef USE_BLUETOOTH
    if (SerialBT.available()) {
        String btCmd = SerialBT.readString();
        processCommand(btCmd);
    }
#endif
}