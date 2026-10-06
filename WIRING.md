# 接线说明

板子：**ESP32-S3-N16R8**。电源统一用板上 **3V3** 和 **GND**。不要使用 GPIO **0 / 3 / 45 / 46**。

## OLED 0.96" 128×64 SSD1306（I2C）

| OLED | ESP32-S3 | 说明 |
|---|---|---|
| VCC | 3V3 | 不要接 5V |
| GND | GND | |
| SDA | GPIO8 | 已在固件里写死 |
| SCL | GPIO9 | 已在固件里写死 |

部分模块还有 `RES`：可接 3V3 或悬空（软件 `OLED_RESET = -1`）。地址固件先扫 `0x3C`，失败再试 `0x3D`。

## 干电极肌电板（模拟输出）

| 传感器 | ESP32-S3 | 说明 |
|---|---|---|
| VCC / 3V3 | 3V3 | 与 MCU 同电源 |
| GND | GND | 必须共地 |
| SIG / AO / Analog | GPIO1 | ADC1，`analogRead` |

电极贴在要发力的前臂肌腹，参考电极按板子说明接。信号是模拟包络/原始肌电均可：固件会整流 + 低通。

## 备用跳跃按键

| 按键 | ESP32-S3 |
|---|---|
| 一端 | GPIO4（`INPUT_PULLUP`） |
| 另一端 | GND |

按下为 LOW，与肌电一样：一次边沿跳一次。

## 振动马达（可选，默认关闭）

在 `jump/config.h` 把 `ENABLE_VIBRATION` 设为 `1`。

| 马达电路 | ESP32-S3 |
|---|---|
| 驱动输入（经晶体管/MOS，不要直接灌大电流） | GPIO5 |
| 电源 | 3V3 / GND（按驱动模块） |

撞到障碍时拉高约 150 ms。

## MAX98357 I2S 功放（可选，默认关闭）

在 `jump/config.h` 把 `ENABLE_AUDIO` 设为 `1`。

| MAX98357 | ESP32-S3 |
|---|---|
| BCLK | GPIO15 |
| LRC / WS | GPIO16 |
| DIN | GPIO7 |
| VIN | 3V3 |
| GND | GND |
| SD / GAIN | 按模块手册（常接 3V3 使能） |

喇叭接模块输出。固件在跳跃和撞击时各发一段短方波。

## 引脚总表

| GPIO | 功能 |
|---|---|
| 1 | 肌电 ADC |
| 4 | 跳跃按键 |
| 5 | 振动（可选） |
| 7 | I2S DIN（可选） |
| 8 | OLED SDA |
| 9 | OLED SCL |
| 15 | I2S BCLK（可选） |
| 16 | I2S LRC（可选） |
