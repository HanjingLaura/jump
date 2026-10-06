# Jump — 按键控制的 OLED 跑酷

在 **ESP32-S3-N16R8** 上运行的横版跳跃小游戏：角色在左侧原地跑，障碍从右往左来，按一下按键跳一次，跳过得分。画面输出到 0.96 寸 128×64 I2C SSD1306，MAX98357 功放出音效，振动模块给触感反馈。

角色是原创的像素小恐龙（朝右，圆脑袋 + 小眼睛，背上一排小骨板，短尾巴、小短手、圆滚滚的身体和小短腿，2 帧跑 + 1 帧跳），障碍是一根只有 8 像素高的矮横纹标记柱，轻松一跳就能过。都是为本游戏重新设计的，不模仿任何现成的游戏形象。节奏对齐 [zhi-dao-le](https://github.com/HanjingLaura/zhi-dao-le) 生成按钮上方的等待跑酷小游戏。

## 玩法来源（zhi-dao-le）

网页版等待跑酷小游戏没有计分，也没有 Game Over：空格或点空白处跳，滞空不可再跳，一根障碍 2.35 秒循环一圈，撞到定格 520 ms 后自动重来。

本固件沿用同一套节奏，并加上 OLED 游玩需要的计分：

| 网页跑酷小游戏 | 本固件 |
|---|---|
| 跑动 166 ms 两帧 | `RUN_CYCLE_MS 166` |
| 跳跃 680 ms，38%–64% 停在最高点（约 20px），`cubic-bezier(0.3, 0.02, 0.35, 1)` | 同一套关键帧 + 贝塞尔 |
| 滞空不可连跳 | `doJump()` 在空中直接 return，空中按键直接丢弃 |
| 单障碍 2.35 s 从右循环到左 | `OBSTACLE_PERIOD_MS 2350` |
| 碰撞内缩约 5px / 3px | `HITBOX_INSET_X/Y` |
| 撞到定格 520 ms 后自动再开 | 定格 520 ms 后进入 Game Over，按键才重开 |
| 无分数 | 跳过一根 +1，速度随分数略加快，最高分存 NVS |

## 需要安装的库

Arduino 核心：**esp32 by Espressif Systems 3.x**（已在 3.3.11 上验证）

库（Arduino Library Manager / `arduino-cli lib install`）：

| 库 | 用途 |
|---|---|
| Adafruit SSD1306 | OLED 驱动 |
| Adafruit GFX Library | 画点阵和图元 |
| Adafruit BusIO | SSD1306 依赖（随上面自动装） |

ESP32 核心自带 `ESP_I2S`（I2S 音效）、`Preferences`、`Wire`，不需要额外装。

## 编译与上传（arduino-cli）

板子：`esp32:esp32:esp32s3`，Flash **16MB**，PSRAM **OPI**（对应 N16R8）。

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"

FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi"

arduino-cli compile --fqbn "$FQBN" jump
arduino-cli upload  --fqbn "$FQBN" -p /dev/ttyACM0 jump
```

Windows（PowerShell）示例，端口按 `arduino-cli board list` 实际显示填写：

```powershell
$env:TEMP='D:\gtmp'; $env:TMP='D:\gtmp'   # 默认 TEMP 路径可能让 cc1plus 出错
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi --build-path D:\gbuild\jump jump
arduino-cli upload  -p COM7 --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi --input-dir D:\gbuild\jump jump
```

如果用板子上的原生 USB 口（USB-Serial-JTAG）看串口，FQBN 末尾再加 `,CDCOnBoot=cdc`。串口监视器：`115200`。

若编译选项名与本机核心版本略有差异，可先列出：

```bash
arduino-cli board details -b esp32:esp32:esp32s3
```

## 接线

完整说明见 [WIRING.md](WIRING.md)。摘要：

| 模块 | ESP32-S3 |
|---|---|
| OLED VDD / GND | 3V3 / GND |
| OLED SDA | GPIO8 |
| OLED SCK / SCL | GPIO9 |
| 跳跃按键（另一端 GND） | GPIO4（内部上拉） |
| 振动模块 S/IN · VCC · GND | GPIO5 · 3V3 · GND |
| MAX98357 Vin · GND | 5V · GND |
| MAX98357 BCLK / LRC / DIN | GPIO15 / 16 / 7 |
| 喇叭 | MAX98357 螺丝端子 + / − |

OLED 地址先试 `0x3C`，失败自动试 `0x3D`。不要占用 GPIO 0 / 3 / 45 / 46。

## 玩法

1. 上电进入标题画面「跳跃」，「按键开始」闪烁，右侧显示最高分。
2. 按一下按键开始，同时响起开场音。
3. 角色自动向前跑（实际是障碍左移，约 2.35 秒一圈）。按一下跳一下（带去抖，一次按下只跳一次，空中按键无效）。
4. 跳过一根障碍 +1 分并「嘀」一声，速度随分数略加快。左上角是本局分数，右上角是最高分。
5. 撞到障碍：撞击音 + 振动 150 ms，定格约半秒后显示「游戏结束」、本局分数和最高分（存在 NVS `Preferences`，断电保留）。
6. 「按键重来」：再按一下重新开局。

## 音效与振动

默认都已打开，在 `jump/config.h` 调整：

```c
#define ENABLE_AUDIO       1    // MAX98357 I2S 音效
#define VOLUME             35   // 0..100，默认中等音量
#define ENABLE_VIBRATION   1    // GPIO5 振动模块，高电平震
#define VIBRATE_ON_JUMP    1    // 每次起跳轻震 30 ms（0 = 只在撞到时震）
#define VIBRATION_MS       150  // 撞到时的振动时长
```

| 事件 | 声音 | 振动 |
|---|---|---|
| 开局 | 上行四音 | — |
| 起跳 | 短促上滑音 | 30 ms（`VIBRATE_ON_JUMP`） |
| 跳过障碍 | 两声「嘀」 | — |
| 撞到 | 下滑噪声 | 150 ms |

音效由单独的 FreeRTOS 任务（核心 0）通过队列合成并写 I2S（16 kHz / 16 bit / 单声道复制到左右声道），振动用 `millis()` 计时，都不会阻塞游戏循环。太吵就把 `VOLUME` 调小；也可以按 MAX98357 手册改 GAIN 引脚。

## 固件结构

```
jump/
  jump.ino       入口
  config.h       引脚与可调参数（音量、振动、节奏）
  audio.h/.cpp   MAX98357 I2S 音效任务（ESP_I2S）
  game.h/.cpp    按键去抖、状态机、物理、碰撞、OLED、振动
  sprites.h/.cpp 原创角色/障碍点阵与中文标签
```
