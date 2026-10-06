#include "game.h"
#include "config.h"
#include "audio.h"
#include "sprites.h"

#include <Wire.h>
#include <Preferences.h>

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
static Preferences prefs;

enum class State : uint8_t { Title, Playing, HitStun, GameOver };

struct Obstacle {
  float x;
  uint8_t scored;
};

static State state = State::Title;
static float playerY = 0;
static bool jumping = false;
static uint32_t jumpStartMs = 0;
static uint8_t runFrame = 0;
static uint32_t runAnimMs = 0;
static uint16_t score = 0;
static uint16_t best = 0;
static float speed = SPEED_START;
static Obstacle obstacle;
static uint32_t lastFrameMs = 0;
static uint32_t hitAtMs = 0;
static uint8_t oledAddr = OLED_ADDR_PRIMARY;
static bool oledOk = false;

// ---------------------------------------------------------------------------
// Jump button: GPIO4, INPUT_PULLUP, pressed = LOW. Debounced; one press
// (released -> pressed edge) produces exactly one event.
// ---------------------------------------------------------------------------
static bool btnRaw = HIGH;
static bool btnStable = HIGH;
static uint32_t btnChangedMs = 0;
static bool btnEvent = false;

static void buttonBegin() {
  pinMode(PIN_JUMP_BTN, INPUT_PULLUP);
  btnRaw = btnStable = digitalRead(PIN_JUMP_BTN);
  btnChangedMs = millis();
}

static void buttonService(uint32_t now) {
  bool r = digitalRead(PIN_JUMP_BTN);
  if (r != btnRaw) {
    btnRaw = r;
    btnChangedMs = now;
  }
  if (btnRaw != btnStable && now - btnChangedMs >= BTN_DEBOUNCE_MS) {
    btnStable = btnRaw;
    if (btnStable == LOW) btnEvent = true;
  }
}

static bool buttonTakePress() {
  bool e = btnEvent;
  btnEvent = false;
  return e;
}

// ---------------------------------------------------------------------------
// Vibration: GPIO5 HIGH = motor on. Non-blocking pulse.
// ---------------------------------------------------------------------------
#if ENABLE_VIBRATION
static uint32_t vibUntil = 0;
static bool vibOn = false;
#endif

static void hapticBegin() {
#if ENABLE_VIBRATION
  pinMode(PIN_VIBRATION, OUTPUT);
  digitalWrite(PIN_VIBRATION, LOW);
#endif
}

static void haptic(uint16_t ms) {
#if ENABLE_VIBRATION
  uint32_t until = millis() + ms;
  if (!vibOn || (int32_t)(until - vibUntil) > 0) vibUntil = until;
  vibOn = true;
  digitalWrite(PIN_VIBRATION, HIGH);
#else
  (void)ms;
#endif
}

static void hapticService(uint32_t now) {
#if ENABLE_VIBRATION
  if (vibOn && (int32_t)(now - vibUntil) >= 0) {
    digitalWrite(PIN_VIBRATION, LOW);
    vibOn = false;
  }
#else
  (void)now;
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

static float groundY() { return (float)(GROUND_Y - PLAYER_H); }

static float cubicBez(float t, float a, float b) {
  float u = 1.0f - t;
  return 3.0f * u * u * t * a + 3.0f * u * t * t * b + t * t * t;
}

// CSS animation-timing-function cubic-bezier → keyframe progress.
static float jumpEaseProgress(float x) {
  float t = x;
  for (int i = 0; i < 6; i++) {
    float u = 1.0f - t;
    float xt = cubicBez(t, JUMP_BEZIER_X1, JUMP_BEZIER_X2);
    float dx = 3.0f * u * u * JUMP_BEZIER_X1 + 6.0f * u * t * JUMP_BEZIER_X2 + 3.0f * t * t;
    if (dx < 1e-5f) break;
    t -= (xt - x) / dx;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
  }
  return cubicBez(t, JUMP_BEZIER_Y1, JUMP_BEZIER_Y2);
}

static float jumpKeyLift(float p) {
  if (p <= 0.0f) return 0;
  if (p >= 1.0f) return 0;
  if (p <= JUMP_HANG_START) return p / JUMP_HANG_START;
  if (p <= JUMP_HANG_END) return 1.0f;
  return (1.0f - p) / (1.0f - JUMP_HANG_END);
}

static float jumpHeightAt(float t01) {
  return JUMP_PEAK_PX * jumpKeyLift(jumpEaseProgress(t01));
}

static void tickJump(uint32_t now) {
  if (!jumping) {
    playerY = groundY();
    return;
  }
  float t = (float)(now - jumpStartMs) / (float)JUMP_MS;
  if (t >= 1.0f) {
    jumping = false;
    playerY = groundY();
    return;
  }
  playerY = groundY() - jumpHeightAt(t);
}

static void tickRunAnim(uint32_t dtMs) {
  if (jumping) return;
  runAnimMs += dtMs;
  while (runAnimMs >= RUN_CYCLE_MS / 2) {
    runAnimMs -= RUN_CYCLE_MS / 2;
    runFrame ^= 1;
  }
}

static void resetRun() {
  jumping = false;
  jumpStartMs = 0;
  playerY = groundY();
  runFrame = 0;
  runAnimMs = 0;
  score = 0;
  speed = SPEED_START;
  obstacle.x = (float)(OLED_WIDTH + 4);
  obstacle.scored = 0;
}

static void doJump() {
  if (jumping) return;
  jumping = true;
  jumpStartMs = millis();
  audioPlay(SFX_JUMP);
#if VIBRATE_ON_JUMP
  haptic(VIBRATION_JUMP_MS);
#endif
}

static bool collide() {
  int16_t px = PLAYER_X + HITBOX_INSET_X;
  int16_t py = (int16_t)playerY + HITBOX_INSET_Y;
  int16_t pw = PLAYER_W - HITBOX_INSET_X * 2;
  int16_t ph = PLAYER_H - HITBOX_INSET_Y * 2;
  const Sprite &s = spriteGet(SPR_OBS_POST);
  int16_t ox = (int16_t)obstacle.x;       // hitbox == drawn 4x5 rock
  int16_t oy = GROUND_Y - s.h;
  int16_t ow = s.w;
  int16_t oh = s.h;
  bool sep = px + pw <= ox || ox + ow <= px || py + ph <= oy || oy + oh <= py;
  return !sep;
}

static void saveBest() {
  if (score > best) {
    best = score;
    prefs.putUShort(PREFS_KEY_BEST, best);
  }
}

static void enterHit() {
  jumping = false;
  playerY = groundY();
  state = State::HitStun;
  hitAtMs = millis();
  haptic(VIBRATION_MS);
  audioPlay(SFX_CRASH);
  saveBest();
  Serial.printf("crash  score=%u best=%u\n", score, best);
}

static void startRun() {
  resetRun();
  buttonTakePress();
  audioPlay(SFX_START);
  state = State::Playing;
  lastFrameMs = millis();
}

static void drawGround() {
  display.drawFastHLine(0, GROUND_Y, OLED_WIDTH, SSD1306_WHITE);
  for (int x = 0; x < OLED_WIDTH; x += 8) {
    display.drawPixel(x, GROUND_Y + 2, SSD1306_WHITE);
    display.drawPixel(x + 3, GROUND_Y + 3, SSD1306_WHITE);
  }
}

static void drawHudPlaying() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(score);
  // best score, right-aligned
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "HI %u", best);
  display.setCursor(OLED_WIDTH - n * 6, 0);
  display.print(buf);
}

static void drawWorld(bool showObstacle) {
  drawGround();
  SpriteId ps = SPR_JUMP;
  if (state == State::HitStun || state == State::GameOver) {
    ps = SPR_JUMP;
  } else if (!jumping) {
    ps = runFrame ? SPR_RUN2 : SPR_RUN1;
  }
  int16_t px = PLAYER_X;
  int16_t py = (int16_t)playerY;
  if (state == State::HitStun || state == State::GameOver) {
    px += 1;
    py += 1;
  }
  drawSprite(display, px, py, ps);
  if (showObstacle) {
    const Sprite &s = spriteGet(SPR_OBS_POST);
    drawSprite(display, (int16_t)obstacle.x, GROUND_Y - s.h, SPR_OBS_POST);
  }
}

static void drawTitle() {
  display.clearDisplay();
  drawWorld(false);
  drawBitmapProgmem(display, (OLED_WIDTH - bmp_title_W) / 2, 4,
                    bmp_title, bmp_title_W, bmp_title_H);
  // blink the "press to start" prompt
  if ((millis() / 500) & 1) {
    drawBitmapProgmem(display, (OLED_WIDTH - bmp_start_W) / 2, 20,
                      bmp_start, bmp_start_W, bmp_start_H);
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  char buf[12];
  int n = snprintf(buf, sizeof(buf), "HI %u", best);
  display.setCursor(OLED_WIDTH - n * 6 - 4, 40);
  display.print(buf);
  display.display();
}

static void drawPlaying() {
  display.clearDisplay();
  drawWorld(true);
  drawHudPlaying();
  display.display();
}

static void drawGameOver() {
  display.clearDisplay();
  drawWorld(true);
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
  display.display();
}

static void updatePlaying(uint32_t now, uint32_t dtMs) {
  tickJump(now);
  tickRunAnim(dtMs);

  speed = SPEED_START + score * SPEED_PER_SCORE;
  if (speed > SPEED_MAX) speed = SPEED_MAX;

  obstacle.x -= speed;
  const Sprite &s = spriteGet(SPR_OBS_POST);
  if (!obstacle.scored && obstacle.x + s.w < PLAYER_X) {
    obstacle.scored = 1;
    score++;
    audioPlay(SFX_SCORE);
  }
  if (obstacle.x + s.w < -2) {
    obstacle.x = (float)(OLED_WIDTH + 4);
    obstacle.scored = 0;
  }

  if (collide()) enterHit();
}

void gameBegin() {
  Serial.begin(SERIAL_BAUD);
  // Give a USB-CDC host a moment to attach so the boot lines are visible.
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  Serial.println();
  Serial.println("Jump Runner  ESP32-S3  128x64  (button GPIO4)");

  buttonBegin();
  hapticBegin();
  audioBegin();

  oledOk = displayBegin();
  if (oledOk) {
    Serial.printf("OLED ok at 0x%02X  SDA=%d SCL=%d\n", oledAddr, PIN_OLED_SDA, PIN_OLED_SCL);
  } else {
    Serial.println("continuing without a confirmed OLED");
  }
  display.clearDisplay();
  display.display();

  prefs.begin(PREFS_NAMESPACE, false);
  best = prefs.getUShort(PREFS_KEY_BEST, 0);

  resetRun();
  state = State::Title;
  lastFrameMs = millis();
  Serial.printf("audio=%d vibration=%d  best=%u  -> title, press to start\n",
                ENABLE_AUDIO, ENABLE_VIBRATION, best);
}

void gameLoop() {
  const uint32_t frameMs = 1000 / GAME_FPS;
  uint32_t now = millis();
  buttonService(now);
  hapticService(now);

  switch (state) {
    case State::Title:
      if (buttonTakePress()) {
        startRun();
        break;
      }
      if (now - lastFrameMs >= frameMs) {
        uint32_t dt = now - lastFrameMs;
        lastFrameMs = now;
        tickRunAnim(dt);
        drawTitle();
      }
      break;
    case State::Playing:
      if (buttonTakePress()) doJump();   // ignored while airborne: one press = one jump
      if (now - lastFrameMs >= frameMs) {
        uint32_t dt = now - lastFrameMs;
        lastFrameMs = now;
        updatePlaying(now, dt);
        drawPlaying();
      }
      break;
    case State::HitStun:
      buttonTakePress();                 // swallow presses during the freeze
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        drawPlaying();
      }
      if (now - hitAtMs >= HIT_STUN_MS) {
        state = State::GameOver;
      }
      break;
    case State::GameOver:
      if (buttonTakePress()) {
        startRun();
        break;
      }
      if (now - lastFrameMs >= frameMs) {
        lastFrameMs = now;
        drawGameOver();
      }
      break;
  }
  delay(1);  // yield; keeps the button sampled at ~1 kHz
}
