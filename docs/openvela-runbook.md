# openvela 复现说明

本文档说明如何把本仓库的智能手环应用接入 openvela 平台。应用选择原生 **C + LVGL** 路线，工程口径对齐 openvela `packages` 顶层仓库中的原生应用示例方向 `packages_demos`。传感器数据当前使用模拟值，后续可替换为 Sensor/uORB 提供的真实心率或计步数据。

openvela `packages` 顶层仓库说明了两个示例方向：

- `packages_demos`：openvela native 应用代码示例，例如音乐播放器、智能手环、自行车码表。
- `packages_fe_examples`：openvela quickapp 代码示例，例如日历、播放器、任务清单、图表和设置 UI。

本项目当前交付原生应用版本；快应用可作为后续扩展路线。

## 1. 环境准备

准备 openvela SDK、交叉编译工具链、目标板或模拟器。不同 openvela 版本的命令可能略有差异，请以当前 SDK 的 `README`、`build.sh`、`menuconfig` 入口为准。

需要启用的能力：

- LVGL 图形库。官方 Bandx 手环示例还会启用 `LV_USE_FRAGMENT`。
- 目标屏幕或模拟器显示驱动。
- 触摸输入或鼠标输入，用于 LVGL 手势事件。
- NuttX/openvela shell，用于启动 `smart_band`。

## 2. 拷贝应用

假设 openvela 根目录为 `$OPENVELA_ROOT`：

```sh
mkdir -p "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic"
cp -r openvela_app/smart_band/* "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"
```

如果当前 SDK 的 `apps/packages/demos/Kconfig` 没有自动扫描子目录，需要加入：

```text
source "packages/demos/smart_band_basic/Kconfig"
```

如果当前 `apps/packages/demos/Make.defs` 没有自动包含子目录 `Make.defs`，需要加入：

```make
include $(wildcard $(APPDIR)/packages/demos/smart_band_basic/Make.defs)
```

## 3. 配置

进入 openvela SDK 后打开配置：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap menuconfig
```

在配置中启用：

- `LV_USE_FRAGMENT`
- `LVX_USE_DEMO_SMART_BAND_BASIC`
- 当前模拟器或开发板对应的 framebuffer/display/input 配置

如果你的板级初始化没有提前执行 `lv_init()`，同时启用：

```text
LVX_DEMO_SMART_BAND_BASIC_STANDALONE_INIT
```

多数已经集成 LVGL 的图形系统不需要打开这个选项。

## 4. 编译与运行

模拟器示例：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap distclean -j8
./build.sh vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap -j8
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

## 6. 常见问题

### 编译提示找不到 LVGL

确认 SDK 已启用 LVGL 相关配置。官方 Bandx 示例至少会打开 `LV_USE_FRAGMENT` 和 `LVX_USE_DEMO_BANDX`；本项目对应打开 `LVX_USE_DEMO_SMART_BAND_BASIC`。

### 运行后黑屏

确认显示驱动、LVGL tick、输入设备和 framebuffer 初始化已经完成。若当前系统没有全局 LVGL 初始化，打开 `LVX_DEMO_SMART_BAND_BASIC_STANDALONE_INIT` 再试。

### 手势没有反应

确认触摸或鼠标输入设备已接入 LVGL indev。应用使用 `LV_EVENT_GESTURE` 和 `lv_indev_get_gesture_dir()` 处理左右滑动。

### 字体或中文显示异常

当前 C 版 UI 使用少量中文文本。如果目标固件没有包含中文字体，可以把 `app_lvgl.c` 中的中文标签替换为英文，或在 openvela/LVGL 配置中加入中文字体资源。
