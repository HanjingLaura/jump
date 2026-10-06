#pragma once

#include <Arduino.h>

void emgBegin();

void emgCalibrateStart();
// Call often during calibration. Returns true when the 3 s window is finished.
bool emgCalibrateTick();
float emgCalibrateProgress();  // 0..1

void emgService();             // ~500 Hz sampling + envelope
bool emgPollJump();            // rising-edge jump (EMG or button), respects cooldown
bool emgEffortNow();           // envelope currently above threshold

uint16_t emgRaw();
float emgEnvelope();
float emgThreshold();
float emgReleaseThreshold();
float emgBaseline();
float emgNoise();
float emgIntensity01();        // 0..1 bar fill, 0.5 ~= threshold

void emgPrintDebug();
