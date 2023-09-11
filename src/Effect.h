#pragma once

#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
#include <avr/power.h>
#endif

#define DEFAULT_BRIGHT 30

int currentBright = DEFAULT_BRIGHT;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(Adafruit_NeoPixel &strip, byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow(Adafruit_NeoPixel &strip, uint8_t wait) {
    uint16_t i, j;

    for (j = 0; j < 256; j++) {
        for (i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(strip, (i + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbowCycle(Adafruit_NeoPixel &strip, uint8_t wait) {
    uint16_t i, j;

    for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
        for (i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(strip, ((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

void colorWipe(Adafruit_NeoPixel &strip, uint32_t c, uint8_t wait) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, c);
        strip.show();
        delay(wait);
    }
}

void theaterChaseRainbow(Adafruit_NeoPixel &strip, uint8_t wait) {
    for (int j = 0; j < 256; j++) {     // cycle all 256 colors in the wheel
        for (int q = 0; q < 3; q++) {
            for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
                strip.setPixelColor(i + q, Wheel(strip, (i + j) % 255));    //turn every third pixel on
            }

            strip.setBrightness(currentBright);
            strip.show();

            delay(wait);

            for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
                strip.setPixelColor(i + q, 0);        //turn every third pixel off
            }
        }
    }
}

void colorHeight(Adafruit_NeoPixel &strip1, Adafruit_NeoPixel &strip2, uint32_t c, int height) {
    uint16_t pixels1 = strip1.numPixels() - 1;
    for (uint16_t i = 0; i < pixels1; i++) {
        strip1.setPixelColor(pixels1 - i, (i < height) ? c : 0);
    }
    uint16_t pixels2 = strip1.numPixels() - 1;
    for (uint16_t i = 0; i < pixels2; i++) {
        strip2.setPixelColor(pixels1 - i, (i < height) ? c : 0);
    }
    strip1.show();
    strip2.show();
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
