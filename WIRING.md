# 接线说明

板子：**ESP32-S3-N16R8**。OLED、振动模块用板上 **3V3**，MAX98357 用 **5V**，所有模块 **GND 共地**。不要使用 GPIO **0 / 3 / 45 / 46**。

## OLED 0.96" 128×64 SSD1306（I2C）

| OLED | ESP32-S3 | 说明 |
|---|---|---|
| VDD / VCC | 3V3 | 不要接 5V |
| GND | GND | |
| SDA | GPIO8 | 已在固件里写死 |
| SCK / SCL | GPIO9 | 已在固件里写死 |

部分模块还有 `RES`：可接 3V3 或悬空（软件 `OLED_RESET = -1`）。地址固件先试 `0x3C`，失败再试 `0x3D`。

## 跳跃按键

| 按键 | ESP32-S3 |
|---|---|
| 一端 | GPIO4（`INPUT_PULLUP`） |
| 另一端 | GND |

不需要外接电阻。按下为 LOW，固件去抖 25 ms，一次按下跳一次。标题画面按一下开始，游戏结束后按一下重来。

## 振动模块（自带驱动）

| 振动模块 | ESP32-S3 |
|---|---|
| S / IN | GPIO5 |
| VCC | 3V3 |
| GND | GND |

高电平震动。撞到障碍震 150 ms；每次起跳轻震 30 ms（`config.h` 里 `VIBRATE_ON_JUMP` 设 `0` 可关）。如果是裸马达（没有驱动板），必须加三极管/MOS 管和续流二极管，不要直接接 GPIO。

## MAX98357 I2S 功放

| MAX98357 | ESP32-S3 |
|---|---|
| Vin | 5V |
| GND | GND |
| BCLK | GPIO15 |
| LRC / WS | GPIO16 |
| DIN | GPIO7 |
| SD / GAIN | 悬空即可（默认使能、9 dB 增益） |

喇叭（4–8 Ω）接模块螺丝端子的 `+` / `−`。音量在 `config.h` 的 `VOLUME`（0–100，默认 35）。

## 引脚总表

| GPIO | 功能 |
|---|---|
| 4 | 跳跃按键（另一端 GND） |
| 5 | 振动模块 S/IN |
| 7 | MAX98357 DIN |
| 8 | OLED SDA |
| 9 | OLED SCK / SCL |
| 15 | MAX98357 BCLK |
| 16 | MAX98357 LRC / WS |
| 3V3 | OLED VDD、振动模块 VCC |
| 5V | MAX98357 Vin |
| GND | 全部模块共地 |
