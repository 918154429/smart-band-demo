# openvela Smart Band Demo

这是一个面向初赛交付的智能手环应用样例，技术路线选择 **openvela 原生应用 C + LVGL**。实现口径对齐 openvela `packages` 顶层仓库中提到的两个示例方向：原生应用示例仓库 `packages_demos` 和快应用示例仓库 `packages_fe_examples`。本项目当前优先实现原生 `packages/demos` 风格应用，同时提供静态桌面演示，方便在没有开发板或模拟器时录制页面切换、表盘、心率和计步效果。

## 已完成功能

- 表盘页面：显示当前时间、日期、电量、温度与页面指示点。
- 健康页面：优先读取模拟器心率传感器，缺失时回退到模型数据。
- 计步页面：优先读取模拟器计步/加速度传感器，缺失时回退到模型数据。
- 应用中心：已模仿 openvela 原生 demo 与快应用 examples 的组织方式，在一个
  Apps 页面内提供可点击的小应用图标。当前包含天气、计算器、计时器、音乐控制、
  设置、秒表、手电筒、扫雷、俄罗斯方块和电子木鱼 10 个可运行应用；实现为当前
  原生 LVGL 应用内部逻辑，没有直接调用示例仓库的应用入口。
- 基础交互：支持左右滑动切换页面；桌面演示也支持键盘方向键和按钮。
- 异常处理：时间获取失败时显示兜底文本；模拟数据做范围限制；UI 初始化失败会直接返回错误码。

## 目录结构

```text
openvela_app/smart_band/    可复制到 packages/demos/smart_band_basic 的 C + LVGL 应用
demo/                       无依赖浏览器演示，用于截图和录屏
docs/                       复现说明、录屏脚本和截图建议
tests/                      可在本机运行的基础逻辑测试
```

## 快速演示

直接用浏览器打开：

```text
demo/index.html
```

页面打开后：

- 左右滑动切换表盘、心率、计步和应用中心页面。
- 在应用中心点击图标进入小应用，返回后仍停留在应用中心。
- 按键盘 `ArrowLeft` / `ArrowRight` 也可以切换。
- 心率、步数、电量会自动刷新。

## openvela 运行说明

详细步骤见 [docs/openvela-runbook.md](docs/openvela-runbook.md)。核心流程是把 `openvela_app/smart_band` 拷贝到 openvela `packages/demos/smart_band_basic`，在配置中启用 LVGL、显示驱动和 `LVX_USE_DEMO_SMART_BAND_BASIC`，然后在模拟器或开发板 shell 中运行：

```sh
smart_band
```

应用在 openvela/NuttX 下会订阅 `/dev/uorb/sensor_hrate0`、
`/dev/uorb/sensor_accel0`、`/dev/uorb/sensor_ambient_temp0` 和
`/dev/charge/goldfish_battery`。若温度设备名不同，会回退尝试
`/dev/uorb/sensor_temp0`。时间仍来自模拟器系统时钟，应用在 NuttX 下
会按 UTC+8 格式化显示，避免模拟器 shell 的默认 UTC 时区影响手环页面。

可以用滚动脚本验证模拟器传感器值是否进入应用：

```sh
SMART_BAND_ROLL_LOOPS=4 SMART_BAND_ROLL_DELAY=2 scripts/roll_emulator_sensors.sh
```

## 本机基础测试

当前环境没有要求必须安装交叉编译器。若有 Python 3，可运行：

```sh
python tests/test_watch_model.py
```

该测试会验证页面切换、心率范围、步数累计和日期时间格式的核心规则。
