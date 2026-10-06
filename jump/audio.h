#pragma once

#include <Arduino.h>

enum Sfx : uint8_t {
  SFX_START = 0,   // rising jingle
  SFX_JUMP,        // short upward blip
  SFX_SCORE,       // tick when an obstacle is cleared
  SFX_CRASH,       // falling buzz
};

// Starts I2S on the MAX98357 pins and a small FreeRTOS task that renders
// effects. Returns false if I2S could not start (game keeps running silently).
bool audioBegin();

// Non-blocking: queues an effect for the audio task. SFX_CRASH pre-empts
// anything queued or playing.
void audioPlay(Sfx sfx);
