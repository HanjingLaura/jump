#include "emg.h"
#include "config.h"

static uint16_t rawValue = 0;
static float dcLevel = 2048.0f;
static float envelope = 0.0f;
static float baseline = 0.0f;
static float noise = EMG_NOISE_FLOOR;
static float threshold = 80.0f;
static float releaseTh = 40.0f;

static bool armed = true;
static bool above = false;
static uint32_t lastJumpMs = 0;
static uint32_t lastSampleUs = 0;
static uint32_t lastDebugMs = 0;

static bool btnLast = true;  // pull-up idle HIGH
static uint32_t btnChangeMs = 0;

static uint32_t calibStartMs = 0;
static double calibSum = 0;
static double calibSumSq = 0;
static uint32_t calibCount = 0;
static bool calibrating = false;

void emgBegin() {
  analogReadResolution(EMG_ADC_BITS);
  analogSetPinAttenuation(PIN_EMG, ADC_11db);
  pinMode(PIN_EMG, INPUT);
  pinMode(PIN_JUMP_BTN, INPUT_PULLUP);
  rawValue = analogRead(PIN_EMG);
  dcLevel = (float)rawValue;
  envelope = 0;
  lastSampleUs = micros();
}

void emgCalibrateStart() {
  calibrating = true;
  calibStartMs = millis();
  calibSum = 0;
  calibSumSq = 0;
  calibCount = 0;
  envelope = 0;
}

float emgCalibrateProgress() {
  if (!calibrating) return 1.0f;
  float p = (float)(millis() - calibStartMs) / (float)EMG_CALIB_MS;
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  return p;
}

bool emgCalibrateTick() {
  emgService();
  if (!calibrating) return true;

  uint32_t elapsed = millis() - calibStartMs;
  if (elapsed < EMG_CALIB_SKIP_MS) {
    return false;
  }
  if (elapsed < EMG_CALIB_MS) {
    calibSum += envelope;
    calibSumSq += (double)envelope * (double)envelope;
    calibCount++;
    return false;
  }

  if (calibCount < 10) {
    baseline = envelope;
    noise = EMG_NOISE_FLOOR;
  } else {
    baseline = (float)(calibSum / (double)calibCount);
    double meanSq = calibSumSq / (double)calibCount;
    double var = meanSq - (double)baseline * (double)baseline;
    if (var < 0) var = 0;
    noise = (float)sqrt(var);
  }
  if (noise < EMG_NOISE_FLOOR) noise = EMG_NOISE_FLOOR;
  threshold = baseline + EMG_K * noise;
  releaseTh = baseline + EMG_K * EMG_HYST_RATIO * noise;
  calibrating = false;
  armed = true;
  above = false;
  lastJumpMs = millis();
  Serial.printf("CALIB done  baseline=%.1f  noise=%.1f  th=%.1f  rel=%.1f\n",
                baseline, noise, threshold, releaseTh);
  return true;
}

static void sampleOnce() {
  rawValue = analogRead(PIN_EMG);
  dcLevel = dcLevel + EMG_DC_ALPHA * ((float)rawValue - dcLevel);
  float rect = fabsf((float)rawValue - dcLevel);
  envelope = envelope + EMG_LP_ALPHA * (rect - envelope);
}

void emgService() {
  const uint32_t periodUs = 1000000UL / EMG_SAMPLE_HZ;
  uint32_t now = micros();
  uint8_t n = 0;
  while ((uint32_t)(now - lastSampleUs) >= periodUs && n < 8) {
    lastSampleUs += periodUs;
    sampleOnce();
    n++;
    now = micros();
  }
}

static bool buttonFallingEdge() {
  bool level = digitalRead(PIN_JUMP_BTN);  // HIGH idle, LOW pressed
  uint32_t now = millis();
  if (level == btnLast) return false;
  if (now - btnChangeMs < 25) return false;
  btnChangeMs = now;
  btnLast = level;
  return !level;
}

bool emgEffortNow() {
  return envelope >= threshold;
}

bool emgPollJump(bool accept) {
  emgService();
  uint32_t now = millis();

  bool btn = buttonFallingEdge();

  if (envelope >= threshold) {
    above = true;
  } else if (envelope < releaseTh) {
    above = false;
    armed = true;
  }

  bool emgEdge = false;
  if (above && armed) {
    emgEdge = true;
    armed = false;
  }

  bool want = emgEdge || btn;
  if (!want) return false;
  if (!accept) return false;
  if ((now - lastJumpMs) < EMG_COOLDOWN_MS) return false;
  lastJumpMs = now;
  return true;
}

uint16_t emgRaw() { return rawValue; }
float emgEnvelope() { return envelope; }
float emgThreshold() { return threshold; }
float emgReleaseThreshold() { return releaseTh; }
float emgBaseline() { return baseline; }
float emgNoise() { return noise; }

float emgIntensity01() {
  float span = threshold * 2.0f;
  if (span < 1.0f) span = 1.0f;
  float v = envelope / span;
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  return v;
}

void emgPrintDebug() {
  uint32_t now = millis();
  if (now - lastDebugMs < (1000 / EMG_DEBUG_HZ)) return;
  lastDebugMs = now;
  Serial.printf("raw=%u  env=%.1f  th=%.1f  trig=%d  btn=%d\n",
                (unsigned)rawValue, envelope, threshold,
                (int)emgEffortNow(),
                (int)(digitalRead(PIN_JUMP_BTN) == LOW));
}
