#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern Adafruit_SSD1306 display;

bool displayBegin();
void gameBegin();
void gameLoop();  // call from loop(); internally paces EMG + ~30 fps frames
