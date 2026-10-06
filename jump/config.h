#pragma once

// ---------------------------------------------------------------------------
// Hardware pins (fixed — do not change)
// ---------------------------------------------------------------------------
#define PIN_EMG            1     // ADC1 analog, dry-electrode myoelectric board
#define PIN_JUMP_BTN       4     // INPUT_PULLUP, pressed = LOW
#define PIN_VIBRATION      5     // optional motor
#define PIN_I2S_DIN        7
#define PIN_OLED_SDA       8
#define PIN_OLED_SCL       9
#define PIN_I2S_BCLK       15
#define PIN_I2S_LRC        16

// Do not use GPIO 0 / 3 / 45 / 46 on this board.

#define OLED_WIDTH         128
#define OLED_HEIGHT        64
#define OLED_ADDR_PRIMARY  0x3C
#define OLED_ADDR_FALLBACK 0x3D
#define OLED_RESET         -1

// ---------------------------------------------------------------------------
// Optional peripherals (off by default)
// ---------------------------------------------------------------------------
#ifndef ENABLE_VIBRATION
#define ENABLE_VIBRATION   0
#endif
#ifndef ENABLE_AUDIO
#define ENABLE_AUDIO       0
#endif

#define VIBRATION_MS       150

// ---------------------------------------------------------------------------
// EMG detection
// ---------------------------------------------------------------------------
#define EMG_SAMPLE_HZ      500
#define EMG_ADC_BITS       12
#define EMG_CALIB_MS       3000
#define EMG_CALIB_SKIP_MS  250          // ignore the first samples while settling
#define EMG_LP_ALPHA       0.12f        // envelope low-pass at ~500 Hz
#define EMG_DC_ALPHA       0.005f       // slow DC tracker for rectification
#define EMG_K              6.0f         // threshold = baseline + k * noise
#define EMG_HYST_RATIO     0.55f        // release when env < baseline + hyst*k*noise
#define EMG_COOLDOWN_MS    300          // one effort = one jump
#define EMG_NOISE_FLOOR    12.0f        // minimum noise so a dead-quiet ADC still arms
#define EMG_DEBUG_HZ       20

// ---------------------------------------------------------------------------
// Gameplay — rhythm copied from HanjingLaura/zhi-dao-le DinoRunner
// (waiting mini-game above the generate button):
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
#define JUMP_HANG_START    0.38f
#define JUMP_HANG_END      0.64f
#define RUN_CYCLE_MS       166          // two frames, step-end
#define HITBOX_INSET_X     2            // web: 5px of 44px dino
#define HITBOX_INSET_Y     1            // web: 3px of 47px dino

#define OBSTACLE_PERIOD_MS 2350
#define SPEED_START        ((float)(OLED_WIDTH + 8) * 1000.0f / (float)OBSTACLE_PERIOD_MS / (float)GAME_FPS)
#define SPEED_PER_SCORE    0.04f
#define SPEED_MAX          4.2f
#define HIT_STUN_MS        520

#define PREFS_NAMESPACE    "jump"
#define PREFS_KEY_BEST     "best"

#define SERIAL_BAUD        115200
