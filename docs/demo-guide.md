# 演示录制建议

本文档用于答辩、开源提交页面或录屏材料准备。优先建议使用 openvela 模拟器录屏；
如果当前机器没有模拟器或开发板，也可以用 `demo/index.html` 做静态演示补充。

## 1. openvela 模拟器演示路线

启动模拟器并运行：

```sh
cd /home/dhy/openvela
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap \
  -skin xiaomi_smart_screen_10 \
  -skindir /home/dhy/openvela/prebuilts/emulator/skins/
```

进入 NuttShell 后：

```sh
smart_band
```

建议录制顺序：

1. 表盘页面：展示当前时间、日期、电池图形、充电标识、温度和健康摘要。
2. 心率页面：左滑进入，说明心率优先来自 `/dev/uorb/sensor_hrate0`。
3. 计步页面：继续左滑，展示步数、进度和可调目标步数。
4. 应用中心：继续左滑，展示 8 个应用图标。
5. 倒计时：点击进入，演示 `-1m`、`+1m`、`Start/Pause`、`Reset`。
6. 2048 或扫雷：展示小应用不是静态页面，确实可以交互。
7. 返回表盘：证明页面切换和返回流程完整。

## 2. 传感器验证展示

如果需要证明数据来自模拟器传感器，可以在录屏前或录屏中运行：

```sh
cd /home/dhy/openvela/workspaces/smart-band-demo
SMART_BAND_ROLL_LOOPS=4 SMART_BAND_ROLL_DELAY=2 \
  scripts/roll_emulator_sensors.sh
```

观察点：

- 温度值随环境温度传感器变化。
- 心率随心率传感器变化。
- 电池容量变化时，百分比和电池图形进度同步变化。
- 充电状态变化时出现或隐藏闪电标识。

## 3. 浏览器静态演示

当无法启动 openvela 时，可以直接打开：

```text
demo/index.html
```

操作方式：

- 左右滑动切换页面。
- 键盘 `ArrowLeft` / `ArrowRight` 切换页面。
- 应用中心点击图标进入应用。

注意：浏览器 demo 只用于视觉和交互录屏，不代表 openvela 传感器接入验证。
传感器验证请以 openvela 模拟器或开发板为准。

## 4. 建议截图

建议准备以下图片或录屏片段：

```text
docs/screenshot-watch-face.png
docs/screenshot-heart-rate.png
docs/screenshot-steps.png
docs/screenshot-apps.png
docs/screenshot-timer.png
docs/demo-recording.mp4
```

这些文件不是源码运行所必需的，如提交平台不要求，可以不上传。

## 5. 答辩说明口径

可以按以下口径介绍项目：

- 这是 openvela 原生 C + LVGL 智能手环应用。
- 页面包括表盘、心率、计步和应用中心。
- 心率、步数、温度、电池来自模拟器传感器，失败时有模型兜底。
- 基础交互包括左右滑动、点击应用、返回、按钮控制。
- 小应用分文件实现，便于后续扩展。
- 文档提供了环境、编译、运行、传感器验证和开源合规说明。
