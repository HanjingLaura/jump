# EMG Jump — 肌电控制的 OLED 跑酷

在 **ESP32-S3-N16R8** 上运行的横版跳跃小游戏：角色在左侧原地跑，障碍从右往左来，用前臂肌电（或备用按键）跳过得分。画面输出到 0.96 寸 128×64 I2C SSD1306。

角色是原创圆头小人（2 帧跑 + 1 帧跳），不是 Chrome 恐龙或其它现成形象。

## 需要安装的库

Arduino 核心：**esp32 by Espressif Systems 3.x**

库（Arduino Library Manager / `arduino-cli lib install`）：

| 库 | 用途 |
|---|---|
| Adafruit SSD1306 | OLED 驱动 |
| Adafruit GFX Library | 画点阵和图元 |
| Adafruit BusIO | SSD1306 依赖 |

ESP32 自带 `Preferences`、`Wire`。振动 / I2S 音效默认关闭，不额外装库。

## 编译与上传（arduino-cli）

板子：`esp32:esp32:esp32s3`，Flash **16MB**，PSRAM **OPI**（对应 N16R8）。

```bash
arduino-cli config init --overwrite
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library" "Adafruit BusIO"

FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CDCOnBoot=cdc"

arduino-cli compile --fqbn "$FQBN" jump
arduino-cli upload --fqbn "$FQBN" -p /dev/ttyACM0 jump
```

串口监视器：`115200`。S3 常见设备节点是 `/dev/ttyACM0`（macOS 上多为 `/dev/cu.usbmodem*`）。

若编译选项名与本机核心版本略有差异，可先列出：

```bash
arduino-cli board details -b esp32:esp32:esp32s3
```

## 接线

完整说明见 [WIRING.md](WIRING.md)。摘要：

| 模块 | ESP32-S3 |
|---|---|
| OLED SDA | GPIO8 |
| OLED SCL | GPIO9 |
| OLED VCC / GND | 3V3 / GND |
| 肌电模拟输出 | GPIO1 |
| 肌电供电 | 3V3 / GND |
| 跳跃按键（另一端 GND） | GPIO4（内部上拉） |
| 振动马达（可选，默认关） | GPIO5 |
| MAX98357 BCLK / LRC / DIN（可选，默认关） | GPIO15 / 16 / 7 |

OLED 地址先试 `0x3C`，失败自动试 `0x3D`。不要占用 GPIO 0 / 3 / 45 / 46。

## 玩法

1. 上电后屏幕显示「放松手臂」，约 3 秒自动校准静息基线和噪声。
2. 校准结束进入待命：用力一次或按 GPIO4 开始，同时完成第一次跳跃。
3. 角色自动向前跑（实际是障碍左移）。跳过一个障碍 +1 分，速度随分数加快。
4. 撞到障碍 Game Over，显示本局分数和最高分（存在 NVS `Preferences`，断电保留）。
5. 再用力一次或按键重新开局。
6. 右上角小条是实时肌电强度，竖线是触发阈值位置。

## 肌电检测与调阈值

采样：`analogRead(GPIO1)`，约 500 Hz。对信号做慢速直流跟踪后整流，再一阶低通得到包络。

```
阈值 = 基线 + k × 噪声
```

`k` 默认 `6.0`，在 `jump/config.h` 的 `EMG_K`。迟滞用 `EMG_HYST_RATIO`（降到释放线才允许下一次），冷却 `EMG_COOLDOWN_MS`（默认 300 ms），避免一次发力连跳。

串口（115200）约 20 Hz 打印：

```
raw=2040  env=18.2  th=95.4  trig=0  btn=0
```

| 现象 | 怎么调 |
|---|---|
| 放松也会乱跳 | 增大 `EMG_K`（例如 8～10），或确认校准时手臂确实放松 |
| 用力跳不起来 | 减小 `EMG_K`（例如 3.5～5），电极贴紧肌腹 |
| 一次发力跳两下 | 增大 `EMG_COOLDOWN_MS` 或 `EMG_HYST_RATIO` |
| `raw` 完全不动 | 查 GPIO1 接线、传感器 3V3 供电、共地 |
| 条在动但从不 `trig=1` | 看 `env` 相对 `th`；仍不够就降 `EMG_K` |

改完 `config.h` 后重新编译上传。不要在校准时捏电极或说话时绷前臂。

## 可选外设

在 `config.h`（或编译参数）打开：

```c
#define ENABLE_VIBRATION  1   // GPIO5，撞到震 150 ms
#define ENABLE_AUDIO      1   // MAX98357，跳跃/撞击短音
```

## 固件结构

```
jump/
  jump.ino      入口
  config.h      引脚与可调参数
  emg.h/.cpp    采样、校准、阈值、按键
  game.h/.cpp   状态机、物理、碰撞、OLED、可选震动/音效
  sprites.h/.cpp 原创角色/障碍点阵与中文标题
```
