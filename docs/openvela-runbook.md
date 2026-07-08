# openvela 复现说明

本文档说明如何把本仓库的智能手环应用接入 openvela 平台。应用选择原生 **C + LVGL** 路线，工程口径对齐 openvela `packages/demos` 原生应用示例。传感器数据当前使用模拟值，后续可替换为 Sensor/uORB 提供的真实心率或计步数据。

openvela `packages` 顶层仓库说明了两个示例方向：

- `packages_demos`：openvela native 应用代码示例，例如音乐播放器、智能手环、自行车码表。
- `packages_fe_examples`：openvela quickapp 代码示例，例如日历、播放器、任务清单、图表和设置 UI。

本项目当前交付原生应用版本；快应用可作为后续扩展路线。

## 1. 环境准备

准备 openvela SDK、交叉编译工具链、目标板或模拟器。不同 openvela 版本的命令可能略有差异，请以当前 SDK 的 `README`、`build.sh`、`menuconfig` 入口为准。

当前验证目标优先使用 openvela goldfish 模拟器，例如：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

需要启用的能力：

- LVGL 图形库、`LV_USE_NUTTX` 和 `LV_USE_NUTTX_LIBUV`。
- 目标屏幕或模拟器显示驱动。
- 触摸输入或鼠标输入，用于 LVGL 手势事件。
- uORB/Sensors，用于读取 `sensor_hrate0`、`sensor_accel0`，可用时读取 `sensor_step_counter0`。
- goldfish 电池驱动，用于读取 `/dev/charge/goldfish_battery`。
- NuttX/openvela shell，用于启动 `smart_band`。

## 2. 拷贝应用

假设 openvela 根目录为 `$OPENVELA_ROOT`：

```sh
mkdir -p "$OPENVELA_ROOT/packages/demos/smart_band_basic"
cp -r openvela_app/smart_band/* "$OPENVELA_ROOT/packages/demos/smart_band_basic/"
```

当前 openvela 仓库的 `packages/demos/CMakeLists.txt` 会通过 `nuttx_add_subdirectory()` 自动扫描子目录，`packages/demos/Make.defs` 也会自动包含子目录 `Make.defs`。如果你使用的是其他 SDK 版本且没有自动扫描，需要手工加入：

```text
source "packages/demos/smart_band_basic/Kconfig"
```

以及：

```make
include $(wildcard $(APPDIR)/packages/demos/smart_band_basic/Make.defs)
```

## 3. 配置

进入 openvela SDK 后打开配置：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap menuconfig
```

在配置中启用：

- `LV_USE_NUTTX`
- `LV_USE_NUTTX_LIBUV`
- `SENSORS`
- `UORB`
- `LVX_USE_DEMO_SMART_BAND_BASIC`
- 当前模拟器或开发板对应的 framebuffer/display/input 配置

## 4. 编译与运行

模拟器示例：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap distclean -j2
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
./emulator.sh vela
```

进入 shell 后执行：

```sh
smart_band
```

开发板示例：

```sh
./build.sh <board>:<config>
```

烧录后在串口 shell 中运行：

```sh
smart_band
```

## 5. 交互与验收

- 初始页面显示当前时间和日期。
- 向左滑动进入心率页面。
- 再向左滑动进入计步页面。
- 向右滑动返回上一页。
- 心率、步数、电量每秒刷新一次。
- 模拟器传感器可用时，心率页面显示 `Sensor HR`，电池显示 `sensor` 后缀，计步页面显示 `Sensor` 前缀。

## 6. 常见问题

### 编译提示找不到 LVGL

确认 SDK 已启用 LVGL 相关配置。本项目依赖 `GRAPHICS_LVGL`、`LV_USE_NUTTX`、`LV_USE_NUTTX_LIBUV` 和 `LVX_USE_DEMO_SMART_BAND_BASIC`。

### 运行后黑屏

确认显示驱动、LVGL tick、输入设备和 framebuffer 初始化已经完成。当前 C 入口会按 openvela demo 风格自行执行 `lv_init()`、`lv_nuttx_init()` 和 libuv UI 循环。

### 手势没有反应

确认触摸或鼠标输入设备已接入 LVGL indev。应用使用 `LV_EVENT_GESTURE` 和 `lv_indev_get_gesture_dir()` 处理左右滑动。

### 传感器没有数据显示

应用会尝试打开 `/dev/uorb/sensor_hrate0`、`/dev/uorb/sensor_accel0`、`/dev/uorb/sensor_step_counter0` 和 `/dev/charge/goldfish_battery`。goldfish 当前常见配置有心率、加速度和电池；如果 step counter 没有注册，应用会用加速度变化派生步数，并在读不到任何传感器时继续使用模拟值。

### 字体或中文显示异常

当前 C 版 UI 已改为英文 ASCII 文案，默认 LVGL 字体即可显示。若后续需要中文界面，需要在 openvela/LVGL 配置中加入中文字体资源。
