# openvela 复现说明

本文档说明如何把 `smart_band` 应用接入已有 openvela 工程，并在 goldfish
模拟器或开发板上复现。本文只覆盖本仓库提交的智能手环应用部分，上游 openvela
SDK、NuttX、LVGL、模拟器和工具链请按各自官方文档准备。

## 1. 工程边界

本项目提交内容：

```text
workspaces/smart-band-demo/openvela_app/smart_band
```

目标接入位置：

```text
packages/demos/smart_band_basic
```

可选镜像位置：

```text
apps/packages/demos/smart_band_basic
```

`smart_band` 是 openvela 原生应用，入口为 `smart_band_main.c`，通过
`nuttx_add_application()` 或 NuttX `Application.mk` 注册命令：

```text
smart_band
```

## 2. 环境准备

请先确认 openvela 根目录能完成一次基础构建。例如在本项目验证环境中使用：

```sh
cd /home/dhy/openvela
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

需要的基础能力：

- Linux shell 环境。
- openvela 工程和目标板配置。
- 交叉编译工具链。
- LVGL + NuttX + libuv UI 循环。
- framebuffer/display/input 支持。
- `SENSORS` 和 `UORB` 支持。
- goldfish 模拟器或实际开发板。

## 3. 复制源码

从仓库工作区复制到 openvela packages：

```sh
OPENVELA_ROOT=/home/dhy/openvela
DEMO_ROOT="$OPENVELA_ROOT/workspaces/smart-band-demo"

mkdir -p "$OPENVELA_ROOT/packages/demos/smart_band_basic"
rsync -a --delete \
  "$DEMO_ROOT/openvela_app/smart_band/" \
  "$OPENVELA_ROOT/packages/demos/smart_band_basic/"
```

如果你的工程同时使用 `apps/packages` 镜像目录，也同步：

```sh
mkdir -p "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic"
rsync -a --delete \
  "$DEMO_ROOT/openvela_app/smart_band/" \
  "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"
```

## 4. 配置项

打开 menuconfig：

```sh
cd "$OPENVELA_ROOT"
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap menuconfig
```

确认以下选项可用并启用：

```text
CONFIG_GRAPHICS_LVGL=y
CONFIG_LV_USE_NUTTX=y
CONFIG_LV_USE_NUTTX_LIBUV=y
CONFIG_SENSORS=y
CONFIG_UORB=y
CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC=y
CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY=100
CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE=32768
```

如果 menuconfig 找不到 `LVX_USE_DEMO_SMART_BAND_BASIC`，说明当前 openvela
版本没有自动扫描该 demo 目录。此时检查：

- `packages/demos/smart_band_basic/Kconfig` 是否存在。
- `packages/demos/smart_band_basic/Make.defs` 是否存在。
- 上层 `packages/demos/Kconfig` 是否包含 demos 子目录。
- 上层 `packages/demos/CMakeLists.txt` 是否扫描或添加了该目录。
- 上层 `packages/demos/Make.defs` 是否包含 demos 子目录 `Make.defs`。

## 5. 编译

增量编译：

```sh
cd "$OPENVELA_ROOT"
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

干净编译：

```sh
cd "$OPENVELA_ROOT"
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap distclean -j2
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

构建过程中应能看到类似文件被编译：

```text
CC:  app_lvgl.c
CC:  sensor_bridge.c
CC:  apps/timer_app.c
CC:  smart_band_main.c
LD:  nuttx
Generating: vela_system.bin
Generating: vela_data.bin
Generating: vela_ap.bin
```

如果模拟器启动目录不是构建脚本自动维护的目录，可复制产物：

```sh
mkdir -p cmake_out/vela_goldfish-arm64-v8a-ap
cp nuttx/nuttx cmake_out/vela_goldfish-arm64-v8a-ap/nuttx
for f in vela_system.bin vela_data.bin vela_ap.bin nuttx.bin nuttx.hex; do
  [ -f "nuttx/$f" ] && cp "nuttx/$f" "cmake_out/vela_goldfish-arm64-v8a-ap/$f"
done
```

## 6. 启动模拟器

本项目演示时使用智能屏皮肤，方便投屏展示：

```sh
cd "$OPENVELA_ROOT"
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap \
  -skin xiaomi_smart_screen_10 \
  -skindir "$OPENVELA_ROOT/prebuilts/emulator/skins/"
```

模拟器启动后等待 NuttShell：

```text
NuttShell (NSH)
goldfish-armv8a-ap>
```

启动应用：

```sh
smart_band
```

启动成功后控制台可能输出：

```text
smart_band: temperature sensor 29C
```

这是温度传感器读数日志，不是错误。

## 7. 开发板运行

开发板流程取决于板级配置，通用步骤为：

```sh
cd "$OPENVELA_ROOT"
./build.sh <board-config> -j2
```

烧录后进入串口 shell：

```sh
smart_band
```

如果开发板没有 goldfish 电池设备，电池相关 UI 会保留默认或模型值，不影响其他页面。

## 8. 验收步骤

建议按以下顺序验收：

1. 启动 `smart_band`，确认进入表盘页面。
2. 表盘页面检查当前时间、日期、电量、温度和健康摘要。
3. 左滑进入心率页，确认 bpm 正常刷新。
4. 左滑进入计步页，使用 `-` 和 `+` 调整目标步数。
5. 左滑进入应用中心，点击各应用图标进入详情页。
6. 进入倒计时，使用 `-1m`、`+1m`、`Start/Pause`、`Reset` 验证可调时间。
7. 进入 2048、扫雷、俄罗斯方块，确认触摸和按钮可用。
8. 返回表盘，确认页面切换仍流畅。

## 9. 传感器验证

应用会尝试读取以下设备：

```text
/dev/uorb/sensor_hrate0
/dev/uorb/sensor_step_counter0
/dev/uorb/sensor_accel0
/dev/uorb/sensor_ambient_temp0
/dev/uorb/sensor_temp0
/dev/charge/goldfish_battery
```

可以运行脚本滚动模拟器传感器：

```sh
cd "$DEMO_ROOT"
SMART_BAND_ROLL_LOOPS=4 SMART_BAND_ROLL_DELAY=2 \
  scripts/roll_emulator_sensors.sh
```

预期现象：

- 心率值跟随模拟器心率传感器变化。
- 温度显示传感器温度整数摄氏度。
- 电池百分比和图形进度随电池容量变化。
- 充电状态为 charging 时，电池旁出现闪电标识。

## 10. 常见问题

### 找不到 `smart_band` 命令

检查 `CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC=y` 是否写入 `.config`，并确认
`packages/demos/smart_band_basic` 已被构建系统包含。

### 编译提示找不到 LVGL 或 `lv_nuttx_*`

确认启用了 `GRAPHICS_LVGL`、`LV_USE_NUTTX`、`LV_USE_NUTTX_LIBUV`，并且当前
openvela 版本包含 LVGL NuttX 适配。

### 启动后显示初始化失败

如果控制台出现：

```text
smart_band: LVGL display initialization failed
```

请检查 framebuffer/display/input 设备和模拟器皮肤是否正确。应用会在失败时返回，
不会继续访问空显示对象。

### 运行后黑屏或没有触摸

检查目标配置中的显示驱动、输入设备、LVGL tick 和 libuv UI 循环。模拟器展示时
建议使用本文中的 `xiaomi_smart_screen_10` 命令。

### 传感器没有数值

应用会自动容错，页面仍能展示模型值。请检查对应设备节点是否存在，并确认
`SENSORS`、`UORB` 和模拟器硬件传感器已启用。

### 温度一直是 0C

刚启动时模拟器传感器可能还没有推送数据。可以用
`scripts/roll_emulator_sensors.sh` 写入温度值，再观察 UI 是否刷新。

### 中文显示异常

当前 openvela C 版 UI 主要使用英文 ASCII 文案，避免默认字体缺字。若要改成中文，
需要在 LVGL 配置中加入中文字体并重新设计布局宽度。

### 模拟器卡住或端口被占用

关闭旧的 openvela 模拟器进程后重新启动：

```sh
pkill -f "$OPENVELA_ROOT/prebuilts/emulator" || true
pkill -f "./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap" || true
```

然后重新执行模拟器启动命令。

## 11. 提交前检查清单

- `README.md` 存在并包含环境、编译、运行说明。
- `LICENSE` 存在。
- `NOTICE` 存在。
- `THIRD_PARTY_NOTICES.md` 存在。
- `openvela_app/smart_band/Kconfig` 存在。
- `openvela_app/smart_band/CMakeLists.txt` 存在。
- `openvela_app/smart_band/Makefile` 和 `Make.defs` 存在。
- `python3 tests/test_watch_model.py` 通过。
- openvela 目标配置下 `./build.sh ... -j2` 通过。
