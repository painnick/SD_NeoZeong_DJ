#pragma once

#include <Arduino.h>

#include <Adafruit_NeoPixel.h>

#define DEFAULT_BRIGHT 30

int CurrentBright = DEFAULT_BRIGHT;

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return Adafruit_NeoPixel::Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return Adafruit_NeoPixel::Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return Adafruit_NeoPixel::Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow(Adafruit_NeoPixel &strip, uint8_t wait) {
    uint16_t i, j;

    for (j = 0; j < 256; j++) {
        for (i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel((i + j) & 255));
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
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
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

uint16_t lastHue = 0;
void randomColorWipe(Adafruit_NeoPixel &strip, uint8_t wait) {
    lastHue += 256;
    uint32_t c = Adafruit_NeoPixel::ColorHSV(lastHue, 255,255);
    colorWipe(strip, c, wait);
}

void theaterChaseRainbow(Adafruit_NeoPixel &strip, uint8_t wait, int colorStep, int moveStep) {
    for (int j = 0; j < 256; j += colorStep) {     // cycle all 256 colors in the wheel
        for (int q = 0; q < moveStep; q++) {
            for (uint16_t i = 0; i < strip.numPixels(); i += moveStep) {
                strip.setPixelColor(i + q, Wheel((i + j) % 255));    //turn every third pixel on
            }

            strip.show();

            delay(wait);

            for (uint16_t i = 0; i < strip.numPixels(); i += moveStep) {
                strip.setPixelColor(i + q, 0);        //turn every third pixel off
            }
        }
    }
}

void colorHeight(Adafruit_NeoPixel &strip1, Adafruit_NeoPixel &strip2, uint32_t c, int height) {
    uint16_t pixels1 = strip1.numPixels();
    for (uint16_t i = 1; i < pixels1 + 1; i++) {
        strip1.setPixelColor(pixels1 - i, (i <= height) ? c : 0);
    }
    uint16_t pixels2 = strip1.numPixels();
    for (uint16_t i = 1; i < pixels2 + 1; i++) {
        strip2.setPixelColor(pixels2 - i, (i <= height) ? c : 0);
    }
    strip1.show();
    strip2.show();
}

uint32_t calcColor(int height) {
    uint32_t color;
    if (height < 3) {
        color = Adafruit_NeoPixel::Color(0, 0, CurrentBright);
    } else if (height < 5) {
        color = Adafruit_NeoPixel::Color(0, CurrentBright, 0);
    } else {
        color = Adafruit_NeoPixel::Color(CurrentBright, 0, 0);
    }
    return color;
}
