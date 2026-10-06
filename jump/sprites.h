#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>

enum SpriteId : uint8_t {
  SPR_RUN1 = 0,
  SPR_RUN2,
  SPR_JUMP,
  SPR_OBS_ROCK,
  SPR_OBS_STALK,
  SPR_OBS_WIDE,
};

struct Sprite {
  const uint8_t *data;
  uint8_t w;
  uint8_t h;
};

const Sprite &spriteGet(SpriteId id);

void drawSprite(Adafruit_GFX &g, int16_t x, int16_t y, SpriteId id);

extern const uint8_t bmp_title[];
extern const uint8_t bmp_title_W, bmp_title_H;
extern const uint8_t bmp_relax[];
extern const uint8_t bmp_relax_W, bmp_relax_H;
extern const uint8_t bmp_calibrating[];
extern const uint8_t bmp_calibrating_W, bmp_calibrating_H;
extern const uint8_t bmp_ready[];
extern const uint8_t bmp_ready_W, bmp_ready_H;
extern const uint8_t bmp_gameover[];
extern const uint8_t bmp_gameover_W, bmp_gameover_H;
extern const uint8_t bmp_score[];
extern const uint8_t bmp_score_W, bmp_score_H;
extern const uint8_t bmp_best[];
extern const uint8_t bmp_best_W, bmp_best_H;
extern const uint8_t bmp_retry[];
extern const uint8_t bmp_retry_W, bmp_retry_H;

void drawBitmapProgmem(Adafruit_GFX &g, int16_t x, int16_t y,
                       const uint8_t *bmp, uint8_t w, uint8_t h);
