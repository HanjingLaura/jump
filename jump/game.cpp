#include "game.h"
#include "config.h"
#include "emg.h"
#include "sprites.h"

#include <Wire.h>
#include <Preferences.h>
#include <string.h>

#if ENABLE_AUDIO
#include "driver/i2s_std.h"
#endif

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
static Preferences prefs;

enum class State : uint8_t { Calibrate, Ready, Playing, GameOver };

struct Obstacle {
  bool alive;
  float x;
  SpriteId spr;
  uint8_t scored;
};

static State state = State::Calibrate;
static float playerY = 0;
static float velY = 0;
static bool onGround = true;
static uint8_t runFrame = 0;
static uint8_t animTick = 0;
static uint16_t score = 0;
static uint16_t best = 0;
static float speed = SPEED_START;
static Obstacle obstacles[MAX_OBSTACLES];
static float spawnIn = 40;
static uint32_t lastFrameMs = 0;
#if ENABLE_VIBRATION
static uint32_t vibUntil = 0;
#endif
static uint8_t oledAddr = OLED_ADDR_PRIMARY;

#if ENABLE_AUDIO
static i2s_chan_handle_t i2sTx = nullptr;
static bool i2sOk = false;

static void audioBegin() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  if (i2s_new_channel(&chan_cfg, &i2sTx, nullptr) != ESP_OK) return;
  i2s_std_config_t std_cfg;
  memset(&std_cfg, 0, sizeof(std_cfg));
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
  std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO);
  std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  std_cfg.gpio_cfg.ws = (gpio_num_t)PIN_I2S_LRC;
  std_cfg.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DIN;
  std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(i2sTx, &std_cfg) != ESP_OK) return;
  if (i2s_channel_enable(i2sTx) != ESP_OK) return;
  i2sOk = true;
}

static void audioBeep(uint16_t freqHz, uint16_t ms) {
  if (!i2sOk) return;
  const int sampleRate = 16000;
  const int n = (sampleRate * ms) / 1000;
  int16_t stereo[128];
  int period = sampleRate / (freqHz ? freqHz : 1);
  if (period < 2) period = 2;
  int remaining = n;
  int phase = 0;
  while (remaining > 0) {
    int chunk = remaining > 64 ? 64 : remaining;
    for (int i = 0; i < chunk; i++) {
      int16_t s = (phase < period / 2) ? 6000 : -6000;
      stereo[i * 2] = s;
      stereo[i * 2 + 1] = s;
      phase++;
      if (phase >= period) phase = 0;
    }
    size_t written = 0;
    i2s_channel_write(i2sTx, stereo, chunk * 4, &written, 50);
    remaining -= chunk;
  }
}
#endif

static void haptic(uint16_t ms) {
#if ENABLE_VIBRATION
  digitalWrite(PIN_VIBRATION, HIGH);
  vibUntil = millis() + ms;
#else
  (void)ms;
#endif
}

static void hapticService() {
#if ENABLE_VIBRATION
  if (vibUntil && (int32_t)(millis() - vibUntil) >= 0) {
    digitalWrite(PIN_VIBRATION, LOW);
    vibUntil = 0;
  }
#endif
}

static void playJumpSound() {
#if ENABLE_AUDIO
  audioBeep(880, 60);
#endif
}

static void playHitSound() {
#if ENABLE_AUDIO
  audioBeep(196, 140);
#endif
}

bool displayBegin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_PRIMARY)) {
    oledAddr = OLED_ADDR_PRIMARY;
    return true;
  }
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_FALLBACK)) {
    oledAddr = OLED_ADDR_FALLBACK;
    return true;
  }
  Serial.println("OLED not found at 0x3C or 0x3D");
  return false;
}

static void resetRun() {
  playerY = (float)(GROUND_Y - PLAYER_H);
  velY = 0;
  onGround = true;
  runFrame = 0;
  animTick = 0;
  score = 0;
  speed = SPEED_START;
  spawnIn = 48;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].alive = false;
    obstacles[i].scored = 0;
  }
}

static int16_t gapRange() {
  int16_t gmin = (int16_t)(GAP_MIN_START - score * GAP_SHRINK_PER);
  int16_t gmax = (int16_t)(GAP_MAX_START - score * GAP_SHRINK_PER * 1.2f);
  if (gmin < GAP_MIN_FLOOR) gmin = GAP_MIN_FLOOR;
  if (gmax < gmin + 18) gmax = gmin + 18;
  return gmax;
}

static void spawnObstacle() {
  int slot = -1;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].alive) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return;

  uint8_t r = random(0, 3);
  SpriteId sid = SPR_OBS_ROCK;
  if (r == 1) sid = SPR_OBS_STALK;
  if (r == 2) sid = SPR_OBS_WIDE;
  obstacles[slot].alive = true;
  obstacles[slot].x = (float)OLED_WIDTH;
  obstacles[slot].spr = sid;
  obstacles[slot].scored = 0;

  int16_t gmin = (int16_t)(GAP_MIN_START - score * GAP_SHRINK_PER);
  if (gmin < GAP_MIN_FLOOR) gmin = GAP_MIN_FLOOR;
  spawnIn = (float)random(gmin, gapRange());
}

static void doJump() {
  if (!onGround) return;
  velY = JUMP_VELOCITY;
  onGround = false;
  playJumpSound();
}

static bool collide() {
  int16_t px = PLAYER_X + HITBOX_INSET;
  int16_t py = (int16_t)playerY + HITBOX_INSET;
  int16_t pw = PLAYER_W - HITBOX_INSET * 2;
  int16_t ph = PLAYER_H - HITBOX_INSET * 2;
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].alive) continue;
    const Sprite &s = spriteGet(obstacles[i].spr);
    int16_t ox = (int16_t)obstacles[i].x + 1;
    int16_t oy = GROUND_Y - s.h;
    int16_t ow = s.w - 2;
    int16_t oh = s.h;
    if (ow < 4) ow = s.w;
    bool sep = px + pw <= ox || ox + ow <= px || py + ph <= oy || oy + oh <= py;
    if (!sep) return true;
  }
  return false;
}

static void saveBest() {
  if (score > best) {
    best = score;
    prefs.putUShort(PREFS_KEY_BEST, best);
  }
}

static void enterGameOver() {
  state = State::GameOver;
  saveBest();
  haptic(VIBRATION_MS);
  playHitSound();
}

static void drawGround() {
  display.drawFastHLine(0, GROUND_Y, OLED_WIDTH, SSD1306_WHITE);
  for (int x = 0; x < OLED_WIDTH; x += 8) {
    display.drawPixel(x, GROUND_Y + 2, SSD1306_WHITE);
    display.drawPixel(x + 3, GROUND_Y + 3, SSD1306_WHITE);
  }
}

static void drawEmgBar() {
  const int16_t x = 90;
  const int16_t y = 1;
  const int16_t w = 36;
  const int16_t h = 6;
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  int fill = (int)(emgIntensity01() * (w - 2) + 0.5f);
  if (fill > w - 2) fill = w - 2;
  if (fill > 0) display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
  // Threshold marker at mid-bar (intensity maps threshold to 0.5).
  int16_t tx = x + w / 2;
  display.drawFastVLine(tx, y - 1, h + 2, SSD1306_WHITE);
}

static void drawHudPlaying() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(score);
  drawEmgBar();
}

static void drawWorld() {
  drawGround();
  SpriteId ps = SPR_JUMP;
  if (onGround) ps = (runFrame & 1) ? SPR_RUN1 : SPR_RUN2;
  drawSprite(display, PLAYER_X, (int16_t)playerY, ps);
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].alive) continue;
    const Sprite &s = spriteGet(obstacles[i].spr);
    drawSprite(display, (int16_t)obstacles[i].x, GROUND_Y - s.h, obstacles[i].spr);
  }
}

static void drawCalibrate() {
  display.clearDisplay();
  drawBitmapProgmem(display, 40, 6, bmp_title, bmp_title_W, bmp_title_H);
  drawBitmapProgmem(display, 40, 24, bmp_relax, bmp_relax_W, bmp_relax_H);
  drawBitmapProgmem(display, 44, 38, bmp_calibrating, bmp_calibrating_W, bmp_calibrating_H);
  int16_t w = (int16_t)(emgCalibrateProgress() * 100);
  display.drawRect(14, 54, 100, 6, SSD1306_WHITE);
  if (w > 0) display.fillRect(14, 54, w, 6, SSD1306_WHITE);
  drawEmgBar();
  display.display();
}

static void drawReady() {
  display.clearDisplay();
  drawWorld();
  drawBitmapProgmem(display, 44, 18, bmp_ready, bmp_ready_W, bmp_ready_H);
  drawHudPlaying();
  display.setCursor(0, 10);
  display.print("HI ");
  display.print(best);
  display.display();
}

static void drawPlaying() {
  display.clearDisplay();
  drawWorld();
  drawHudPlaying();
  display.display();
}

static void drawGameOver() {
  display.clearDisplay();
  drawWorld();
  display.fillRect(16, 8, 96, 42, SSD1306_BLACK);
  display.drawRect(16, 8, 96, 42, SSD1306_WHITE);
  drawBitmapProgmem(display, 40, 10, bmp_gameover, bmp_gameover_W, bmp_gameover_H);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(24, 24);
  display.print("S ");
  display.print(score);
  display.print("  HI ");
  display.print(best);
  drawBitmapProgmem(display, 40, 36, bmp_retry, bmp_retry_W, bmp_retry_H);
  drawEmgBar();
  display.display();
}

static void updatePlaying() {
  if (emgPollJump()) doJump();

  velY += GRAVITY;
  if (velY > MAX_FALL_SPEED) velY = MAX_FALL_SPEED;
  playerY += velY;
  float ground = (float)(GROUND_Y - PLAYER_H);
  if (playerY >= ground) {
    playerY = ground;
    velY = 0;
    onGround = true;
  } else {
    onGround = false;
  }

  animTick++;
  if (animTick >= 4) {
    animTick = 0;
    runFrame++;
  }

  speed = SPEED_START + score * SPEED_PER_SCORE;
  if (speed > SPEED_MAX) speed = SPEED_MAX;

  spawnIn -= speed;
  if (spawnIn <= 0) spawnObstacle();

  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].alive) continue;
    obstacles[i].x -= speed;
    const Sprite &s = spriteGet(obstacles[i].spr);
    if (!obstacles[i].scored && obstacles[i].x + s.w < PLAYER_X) {
      obstacles[i].scored = 1;
      score++;
    }
    if (obstacles[i].x + s.w < -2) {
      obstacles[i].alive = false;
    }
  }

  if (collide()) enterGameOver();
}

void gameBegin() {
  Serial.begin(SERIAL_BAUD);
  delay(50);
  Serial.println();
  Serial.println("EMG Jump  ESP32-S3  128x64");

#if ENABLE_VIBRATION
  pinMode(PIN_VIBRATION, OUTPUT);
  digitalWrite(PIN_VIBRATION, LOW);
#endif
#if ENABLE_AUDIO
  audioBegin();
#endif

  emgBegin();
  if (!displayBegin()) {
    Serial.println("continuing without a confirmed OLED");
  }
  display.clearDisplay();
  display.display();

  prefs.begin(PREFS_NAMESPACE, false);
  best = prefs.getUShort(PREFS_KEY_BEST, 0);

  randomSeed((uint32_t)esp_random());
  resetRun();
  emgCalibrateStart();
  state = State::Calibrate;
  lastFrameMs = millis();
  Serial.printf("OLED addr 0x%02X  best=%u\n", oledAddr, best);
}

void gameLoop() {
  emgService();
  hapticService();
  emgPrintDebug();

  const uint32_t frameMs = 1000 / GAME_FPS;
  uint32_t now = millis();

  switch (state) {
    case State::Calibrate:
      if (emgCalibrateTick()) {
        resetRun();
        state = State::Ready;
      }
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        drawCalibrate();
      }
      break;
    case State::Ready:
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        animTick++;
        if (animTick >= 4) {
          animTick = 0;
          runFrame++;
        }
        drawReady();
      }
      if (emgPollJump()) {
        resetRun();
        doJump();
        state = State::Playing;
      }
      break;
    case State::Playing:
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        updatePlaying();
        if (state == State::Playing) drawPlaying();
        else drawGameOver();
      }
      break;
    case State::GameOver:
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        drawGameOver();
      }
      if (emgPollJump()) {
        resetRun();
        state = State::Playing;
      }
      break;
  }
}
