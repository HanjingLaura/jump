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
// Gameplay
// ---------------------------------------------------------------------------
#define GAME_FPS           30
#define GROUND_Y           54
#define PLAYER_X           10
#define PLAYER_W           16
#define PLAYER_H           16
#define JUMP_VELOCITY      -4.6f
#define GRAVITY            0.38f
#define MAX_FALL_SPEED     5.0f
#define HITBOX_INSET       2            // slightly forgiving AABB

#define SPEED_START        2.2f
#define SPEED_PER_SCORE    0.07f
#define SPEED_MAX          5.4f
#define GAP_MIN_START      78
#define GAP_MAX_START      128
#define GAP_SHRINK_PER     0.4f
#define GAP_MIN_FLOOR      52
#define MAX_OBSTACLES      3

#define PREFS_NAMESPACE    "jump"
#define PREFS_KEY_BEST     "best"

#define SERIAL_BAUD        115200
