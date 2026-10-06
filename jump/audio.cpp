#include "audio.h"
#include "config.h"

#if ENABLE_AUDIO

#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

struct Note {
  uint16_t f0;     // start frequency (Hz), 0 = rest
  uint16_t f1;     // end frequency (Hz) for a linear sweep
  uint16_t ms;
  uint8_t noise;   // 1 = mix in noise (crash)
};

const Note kStart[] = {
  {523, 523, 70, 0}, {659, 659, 70, 0}, {784, 784, 70, 0}, {1047, 1047, 140, 0},
};
const Note kJump[] = {
  {620, 1240, 60, 0},
};
const Note kScore[] = {
  {1568, 1568, 30, 0}, {2093, 2093, 40, 0},
};
const Note kCrash[] = {
  {330, 110, 140, 1}, {110, 55, 200, 1},
};

struct Effect {
  const Note *notes;
  uint8_t count;
};

const Effect kEffects[] = {
  {kStart, sizeof(kStart) / sizeof(kStart[0])},
  {kJump, sizeof(kJump) / sizeof(kJump[0])},
  {kScore, sizeof(kScore) / sizeof(kScore[0])},
  {kCrash, sizeof(kCrash) / sizeof(kCrash[0])},
};

constexpr int kChunk = 128;                       // 8 ms at 16 kHz
constexpr int32_t kAmp = (int32_t)16000 * VOLUME / 100;
constexpr int kFadeSamples = AUDIO_SAMPLE_RATE / 500;   // 2 ms click guard

I2SClass i2s;
QueueHandle_t queue = nullptr;
int16_t buf[kChunk];
uint32_t noiseState = 0x12345678u;

inline int16_t noiseSample() {
  noiseState ^= noiseState << 13;
  noiseState ^= noiseState >> 17;
  noiseState ^= noiseState << 5;
  return (int16_t)(noiseState & 0xFFFF);
}

// Returns false if a crash arrived and the current effect should stop.
bool playNote(const Note &n, bool isCrash) {
  const int total = (int)((uint32_t)AUDIO_SAMPLE_RATE * n.ms / 1000);
  uint32_t phase = 0;
  int done = 0;
  while (done < total) {
    if (!isCrash && uxQueueMessagesWaiting(queue) > 0) {
      Sfx next;
      if (xQueuePeek(queue, &next, 0) == pdTRUE && next == SFX_CRASH) return false;
    }
    int len = total - done;
    if (len > kChunk) len = kChunk;
    for (int i = 0; i < len; i++) {
      int s = done + i;
      int32_t v = 0;
      if (n.f0) {
        uint32_t f = n.f0 + (int32_t)((int32_t)n.f1 - (int32_t)n.f0) * s / total;
        phase += (uint32_t)(((uint64_t)f << 32) / AUDIO_SAMPLE_RATE);
        v = (phase & 0x80000000u) ? kAmp : -kAmp;
        if (n.noise) v = (v + (int32_t)noiseSample() * kAmp / 32768) / 2;
        int edge = s < total - s ? s : total - 1 - s;
        if (edge < kFadeSamples) v = v * edge / kFadeSamples;
      }
      buf[i] = (int16_t)v;
    }
    i2s.write((const uint8_t *)buf, len * sizeof(int16_t));
    done += len;
  }
  return true;
}

void writeSilence(int ms) {
  memset(buf, 0, sizeof(buf));
  int total = AUDIO_SAMPLE_RATE * ms / 1000;
  while (total > 0) {
    int len = total > kChunk ? kChunk : total;
    i2s.write((const uint8_t *)buf, len * sizeof(int16_t));
    total -= len;
  }
}

void audioTask(void *) {
  // Prime the DMA ring with zeros so the amp starts quiet.
  writeSilence(100);
  for (;;) {
    Sfx sfx;
    if (xQueueReceive(queue, &sfx, portMAX_DELAY) != pdTRUE) continue;
    if (sfx > SFX_CRASH) continue;
    const Effect &e = kEffects[sfx];
    for (uint8_t i = 0; i < e.count; i++) {
      if (!playNote(e.notes[i], sfx == SFX_CRASH)) break;
    }
    // Flush the 6x240-frame DMA ring (~90 ms) with silence so nothing loops.
    if (uxQueueMessagesWaiting(queue) == 0) writeSilence(100);
  }
}

}  // namespace

bool audioBegin() {
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
  // Mono samples, duplicated into both slots so the MAX98357 (L+R)/2 mix
  // gets full level.
  if (!i2s.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_MONO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S audio init failed");
    return false;
  }
  queue = xQueueCreate(4, sizeof(Sfx));
  if (!queue) return false;
  if (xTaskCreatePinnedToCore(audioTask, "sfx", 3072, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("audio task create failed");
    return false;
  }
  Serial.printf("I2S audio ok  BCLK=%d LRC=%d DIN=%d  volume=%d\n",
                PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN, VOLUME);
  return true;
}

void audioPlay(Sfx sfx) {
  if (!queue) return;
  if (sfx == SFX_CRASH) xQueueReset(queue);
  xQueueSend(queue, &sfx, 0);
}

#else  // ENABLE_AUDIO == 0

bool audioBegin() { return false; }
void audioPlay(Sfx) {}

#endif
