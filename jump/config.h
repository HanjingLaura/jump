#pragma once

// ---------------------------------------------------------------------------
// Hardware pins (fixed — do not change)
// ---------------------------------------------------------------------------
#define PIN_JUMP_BTN       4     // INPUT_PULLUP, other side to GND, pressed = LOW
#define PIN_VIBRATION      5     // vibration module S/IN, HIGH = motor on
#define PIN_I2S_DIN        7     // MAX98357 DIN
#define PIN_OLED_SDA       8
#define PIN_OLED_SCL       9
#define PIN_I2S_BCLK       15    // MAX98357 BCLK
#define PIN_I2S_LRC        16    // MAX98357 LRC / WS

// Do not use GPIO 0 / 3 / 45 / 46 on this board.

#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_ADDR_PRIMARY  0x3C
#define OLED_ADDR_FALLBACK 0x3D
#define OLED_RESET         -1

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------
#define BTN_DEBOUNCE_MS    25           // level must be stable this long

// ---------------------------------------------------------------------------
// Sound (MAX98357 I2S) — on by default
// ---------------------------------------------------------------------------
#ifndef ENABLE_AUDIO
#define ENABLE_AUDIO       1
#endif
#define AUDIO_SAMPLE_RATE  16000
// 0..100. Square-wave effects get loud fast on a MAX98357 (default 9 dB gain),
// so the default is moderate. Raise/lower and re-flash.
#ifndef VOLUME
#define VOLUME             35
#endif

// ---------------------------------------------------------------------------
// Vibration (motor module with its own driver) — on by default
// ---------------------------------------------------------------------------
#ifndef ENABLE_VIBRATION
#define ENABLE_VIBRATION   1
#endif
#ifndef VIBRATE_ON_JUMP
#define VIBRATE_ON_JUMP    1            // short tick on every jump
#endif
#define VIBRATION_MS       150          // crash pulse
#define VIBRATION_JUMP_MS  30           // jump tick

// ---------------------------------------------------------------------------
// Gameplay — rhythm copied from the HanjingLaura/zhi-dao-le waiting runner
// mini-game (above the generate button):
//   run cycle 166 ms / 2 frames, jump 680 ms with hang at 38–64%,
//   one obstacle loops every 2.35 s, hit freezes 520 ms.
// That web game has no score and auto-resumes; this firmware keeps
// +1 per successful hop, a slow speed-up, and a Game Over / best screen.
// ---------------------------------------------------------------------------
#define GAME_FPS           30
#define GROUND_Y           54
#define PLAYER_X           14           // web: left 18px
#define PLAYER_W           16
#define PLAYER_H           16
#define JUMP_MS            680
#define JUMP_PEAK_PX       18           // web: 20px in a 72px strip
#define JUMP_HANG_START    0.38f        // CSS keyframe percents
#define JUMP_HANG_END      0.64f
#define JUMP_BEZIER_X1     0.30f        // cubic-bezier(0.3, 0.02, 0.35, 1)
#define JUMP_BEZIER_Y1     0.02f
#define JUMP_BEZIER_X2     0.35f
#define JUMP_BEZIER_Y2     1.00f
#define RUN_CYCLE_MS       166          // two frames, step-end
#define HITBOX_INSET_X     2            // web: 5px of the 44px runner
#define HITBOX_INSET_Y     1            // web: 3px of the 47px runner

#define OBSTACLE_PERIOD_MS 2350
#define SPEED_START        ((float)(OLED_WIDTH + 8) * 1000.0f / (float)OBSTACLE_PERIOD_MS / (float)GAME_FPS)
#define SPEED_PER_SCORE    0.04f
#define SPEED_MAX          4.2f
#define HIT_STUN_MS        520

#define PREFS_NAMESPACE    "jump"
#define PREFS_KEY_BEST     "best"

#define SERIAL_BAUD        115200
