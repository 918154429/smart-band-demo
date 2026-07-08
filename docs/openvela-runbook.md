# openvela 复现说明

本文档说明如何把本仓库的智能手环应用接入 openvela 平台。应用选择原生 **C + LVGL** 路线，工程口径对齐 openvela `packages/demos` 原生应用示例。主页面包含中文表盘、心率、步数和应用中心四页；传感器可用时读取 Sensor/uORB 数据，缺失时自动回退模拟数据。

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
- `LV_FONT_SIMSUN_16_CJK`，用于显示主界面的中文文案。
- NuttX/openvela shell，用于启动 `smart_band`。

## 2. 拷贝应用

假设 openvela 根目录为 `$OPENVELA_ROOT`。当前 goldfish 配置实际编译入口
是 `$OPENVELA_ROOT/apps/packages/demos/smart_band_basic`，同时保留
`$OPENVELA_ROOT/packages/demos/smart_band_basic` 作为 packages 镜像；两处需要保持同步：

```sh
mkdir -p "$OPENVELA_ROOT/packages/demos/smart_band_basic"
cp -r openvela_app/smart_band/* "$OPENVELA_ROOT/packages/demos/smart_band_basic/"
mkdir -p "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic"
cp -r openvela_app/smart_band/* "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"
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
- `LV_FONT_SIMSUN_16_CJK`
- 当前模拟器或开发板对应的 framebuffer/display/input 配置

也可以直接在目标 defconfig 中加入：

```text
CONFIG_LV_FONT_SIMSUN_16_CJK=y
CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC=y
```

## 4. 编译与运行

模拟器示例：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap -skin xiaomi_smart_band_8_pro -skindir prebuilts/emulator/skins/
```

如果当前工作区的顶层 Make 链在最终链接时报 `nsh_main` 或
`g_builtins` 缺失，可使用已经验证过的 CMake 目标构建同一份模拟器
`nuttx` 产物：

```sh
export PATH="$OPENVELA_ROOT/prebuilts/build-tools/linux-x86_64/bin:$OPENVELA_ROOT/prebuilts/gcc/linux-x86_64/aarch64-none-elf/bin:$PATH"
cmake --build cmake_out/vela_goldfish-arm64-v8a-ap --target apps/packages/demos/smart_band_basic/libapps_smart_band.a -j2
cmake --build cmake_out/vela_goldfish-arm64-v8a-ap --target apps/graphics/lvgl/liblvgl.a -j2
cmake --build cmake_out/vela_goldfish-arm64-v8a-ap --target nuttx -j2
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap -skin xiaomi_smart_band_8_pro -skindir prebuilts/emulator/skins/
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

- 初始表盘显示当前时间、日期、电量、睡眠、心率、压力和温度。
- 向左滑动进入心率页面，再向左进入步数页面，再向左进入应用中心。
- 向右滑动返回上一页，底部圆点同步显示当前页。
- 心率、步数、电量、温度每秒刷新一次。
- 模拟器传感器可用时，详情页来源显示 `传感器`；读不到设备时显示 `模拟` 并继续运行。

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

当前 C 版主 UI 使用短中文文案，需要在 openvela/LVGL 配置中启用 `CONFIG_LV_FONT_SIMSUN_16_CJK=y`。若仍显示方块，请先清理构建缓存后重新编译：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap distclean -j2
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```
