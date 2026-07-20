# 实现过程说明

本文档记录 `smart_band` 从前端原型到 openvela 原生应用的处理过程，便于开源提交、
答辩说明和后续维护。

## 1. 技术路线选择

项目最初有一个简单前端手环页面。为了在 openvela 模拟器中运行并接入传感器，最终
选择 openvela 原生应用路线：

```text
C + LVGL + NuttX application command
```

选择原因：

- 可以直接使用 openvela/NuttX 的设备节点、uORB 传感器和电池 ioctl。
- 可以通过 `smart_band` shell 命令在模拟器或开发板中启动。
- 与 `packages/demos` 原生 demo 的组织方式一致，方便提交到 openvela packages。
- 小屏 UI 的性能和资源占用更可控。

浏览器 `demo` 保留为静态演示，不作为最终系统运行入口。

## 2. 工程组织

应用被组织为独立目录：

```text
openvela_app/smart_band
```

该目录可复制到：

```text
packages/demos/smart_band_basic
```

核心文件职责：

- `smart_band_main.c`：应用入口，初始化 LVGL、NuttX LVGL 适配和 libuv UI 循环。
- `app_lvgl.c`：主页面、表盘、心率页、计步页、应用中心和公共 UI helper。
- `watch_model.c`：基础模型、时间、模拟数据、步数目标等逻辑。
- `sensor_bridge.c`：openvela/NuttX 传感器和电池设备接入。
- `smart_band_apps.c`：应用中心目录和小应用分发。
- `apps/*.c`：每个小应用单独文件实现，避免所有功能堆在一个文件中。
- `icon_assets.c`：项目图标转换后的 LVGL 图片资源。

构建入口：

- `Kconfig`
- `CMakeLists.txt`
- `Makefile`
- `Make.defs`

## 3. 页面与交互处理

主界面分为 4 个页面：

1. 表盘页
2. 心率页
3. 计步页
4. 应用中心

交互处理：

- 使用 LVGL 手势事件处理左右滑动。
- 页面切换时只切换可见状态，减少重复创建对象。
- 应用中心点击图标进入详情页。
- 详情页使用左上角 `<` 返回应用中心。
- 每秒 tick 更新模型、传感器数据和当前应用状态。

UI 处理重点：

- 字号和布局按小屏设备约束调整。
- 电池同时显示百分比和电池形状进度。
- 充电状态使用闪电标识表达。
- 图标从字母徽章改为更直观的图片图标。
- 避免中文字体缺失导致乱码，openvela C 版主要使用 ASCII 文案。

## 4. 传感器处理

传感器接入集中在 `sensor_bridge.c`。

已处理的设备：

```text
/dev/uorb/sensor_hrate0
/dev/uorb/sensor_step_counter0
/dev/uorb/sensor_accel0
/dev/uorb/sensor_ambient_temp0
/dev/uorb/sensor_temp0
/dev/uorb/sensor_humi0
/dev/charge/goldfish_battery
```

处理策略：

- 设备使用 `O_RDONLY | O_NONBLOCK` 打开，避免 UI 主循环被阻塞。
- uORB 传感器设置采样间隔为 1 秒，若设备不支持该 ioctl，只打印日志并继续运行。
- `read_latest()` 会读完队列中的最新样本，避免 UI 使用旧数据。
- 心率读数有效时更新状态；无数据时保留模型兜底值。
- step counter 可用时直接使用；不可用时从加速度变化推导步数。
- 温度优先读 `sensor_ambient_temp0`，失败后尝试 `sensor_temp0`。
- 湿度读取 `sensor_humi0`，更新天气应用中的 Humidity 数值。
- 电池通过 `BATIOC_CAPACITY` 和 `BATIOC_STATE` 读取容量和充电状态。

## 5. 异常与边界保护

当前已实现的基础异常处理：

- `smart_band_main.c` 检查 LVGL 是否重复初始化。
- 显示设备初始化失败时输出错误并返回。
- UI 创建失败时输出错误并释放 LVGL/NuttX 资源。
- 传感器 bridge 对空指针直接返回。
- 每个传感器 fd 初始化为 `-1`，读取前检查 fd 是否有效。
- 传感器不存在时不会终止应用。
- 电池 ioctl 失败时不更新电池状态，不影响其他页面。
- 温度、心率、电池、步数等显示值做范围保护。
- 倒计时、步数目标、小游戏状态均做边界限制，避免按钮连续点击导致异常状态。

## 6. 小应用实现

应用中心当前包含 8 个应用：

| 应用 | 文件 | 说明 |
| --- | --- | --- |
| Weather | `apps/weather_app.c` | 显示温度、范围、风力、湿度等天气信息 |
| Calculator | `apps/calculator_app.c` | 可点击计算器，支持基础四则运算 |
| Timer | `apps/timer_app.c` | 可调倒计时，支持 `-1m`、`+1m`、开始/暂停、重置 |
| 2048 | `apps/music_app.c` | 原音乐入口已改为 2048 游戏，核心为滑动合并逻辑 |
| Stopwatch | `apps/stopwatch_app.c` | 秒表计时、开始/暂停、重置 |
| Mines | `apps/mines_app.c` | 扫雷游戏，支持难度调整 |
| Tetris | `apps/tetris_app.c` | 小屏俄罗斯方块 |
| Wooden Fish | `apps/wooden_fish_app.c` | 电子木鱼，包含功德统计、速度提示和归零提示 |

每个应用独立文件实现，公共能力通过 `smart_band_app_host_t` 注入，例如创建按钮、
创建文本、尺寸缩放、格式化温度等。

## 7. 资源与图标处理

早期 UI 使用英文字母作为功能图标，识别度较弱。后续改为图片图标：

```text
openvela_app/smart_band/assets/generated_icons
```

图标再转换为 LVGL 图片描述结构，汇总到：

```text
icon_assets.c
include/icon_assets.h
```

这样运行时不依赖文件系统加载 PNG，图标直接随应用编译进镜像，更适合嵌入式 demo。

## 8. 验证过程

开发过程中使用了以下验证方式：

- `python3 tests/test_watch_model.py` 使用 host C 编译器直接编译并执行生产
  `watch_model.c`，验证模型规则。
- openvela 目标配置下执行 `./build.sh ... -j2` 验证编译。
- 启动 goldfish 模拟器，在 NSH 中运行 `smart_band`。
- 使用模拟器传感器滚动脚本验证心率、温度、湿度和电池状态进入 UI。
- 手动点击和滑动验证页面切换、应用进入、返回和按钮操作。

## 9. 后续维护建议

- 若要改中文 UI，需要先接入中文字体，再重新检查小屏排版。
- 若要上真实硬件，需要确认传感器设备节点名称，必要时在 `sensor_bridge.c` 增加路径表。
- 若要新增小应用，建议在 `apps/` 下新增单独文件，并在 `smart_band_apps.c` 中注册。
- 若提交平台要求不同开源协议，应同步更新 `LICENSE`、`NOTICE` 和
  `THIRD_PARTY_NOTICES.md`。
