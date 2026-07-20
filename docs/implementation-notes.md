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
- `app_lvgl.c`：根对象生命周期、页面导航和 central runtime 的 LVGL controller 适配。
- `services/runtime.c`：统一持有 model、sensor provider、app registry、event queue、clock
  与 capability snapshot，并固定 model -> sensor -> app tick 顺序。
- `services/event_queue.c`：无堆分配的定长优先级队列、metrics/tick 合并和满载淘汰。
- `services/event_inbox.c`：外部 callback 使用的带锁定长 inbox，UI tick 串行化到主队列。
- `services/clock.c` / `services/capabilities.c`：可注入墙钟/单调钟及平台能力基线。
- `platform/platform_noop.c`：storage、power、haptic、sync 的显式 unavailable 后端。
- `platform/loopback/sync_loopback.c`：固定 8 x 64 bytes 内存 sync transport。
- `ui/lvgl/components.c`：缩放、字体、标签、卡片、图标和格式化等通用 helper。
- `ui/lvgl/watch_pages.c`：表盘、心率和计步页面的 view 创建与按需渲染。
- `watch_model.c`：基础模型、时间、模拟数据、步数目标等逻辑。
- `sensor_bridge.c`：openvela/NuttX 传感器和电池设备接入。
- `smart_band_apps.c`：单一 descriptor registry、静态 context runtime 和 owned container 生命周期。
- `apps/*.c`：每个小应用使用显式 context，分离持久 state、LVGL view、event binding
  和 `init/mount/unmount/tick/render` lifecycle。
- `logic/*.c`：Calculator、2048 和 Mines 的无 LVGL 生产状态机，随机逻辑使用显式
  seed，可由 host test 直接编译验证。
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
- tick 更新模型、传感器数据和当前应用状态；dirty flags 只触发受影响页面或应用渲染。

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
- 每项指标保存 source、freshness、last-update 和 TTL；真实样本短暂中断时先
  保留 stale 值，超过 5 秒后自动模式才使用独立模拟缓存。
- 系统墙钟回拨时旧样本立即过期，避免 TTL 因时间校正而无限延长。
- 加速度推导步数使用独立累计基线，不再与模型模拟步数相加。

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
| 2048 | `apps/game_2048_app.c` | 滑动合并游戏，文件、枚举和 lifecycle 均使用 2048 语义 |
| Stopwatch | `apps/stopwatch_app.c` | 秒表计时、开始/暂停、重置 |
| Mines | `apps/mines_app.c` | 扫雷游戏，支持难度调整 |
| Tetris | `apps/tetris_app.c` | 小屏俄罗斯方块 |
| Wooden Fish | `apps/wooden_fish_app.c` | 电子木鱼，包含功德统计、速度提示和归零提示 |

每个应用独立文件实现，公共能力通过 `smart_band_app_host_t` 注入，例如创建按钮、
创建文本、尺寸缩放、格式化温度等。registry 同时保存标题、颜色、图标、tick 策略
和 ops，不再维护多组按 ID 分派的 switch。runtime 为每次 mount 创建自己的 owned
container，并在失败、返回和销毁时统一回收。

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
  `watch_model.c` 和无传感器分支的 `sensor_bridge.c`，验证模型与 provider 规则。
- `python3 tests/test_time_apps.py` 直接组合生产 runtime、Timer 和 Stopwatch，验证
  双实例隔离、后台计时、卸载重挂载和 tick 回绕。
- `python3 tests/test_app_runtime.py` 直接编译生产 registry/runtime，验证 8 个应用
  反复 mount/unmount、owned container、失败回滚和 tick policy。
- `python3 tests/test_runtime_core.py` 直接编译 central runtime、event queue/inbox、clock、
  capability、model、provider 和 platform adapters，验证满载优先级、外部 callback
  串行化、tick 回绕、RTC 恢复、墙钟回拨、dirty flags、no-op 与 sync loopback。
- `python3 tests/test_app_logic.py` 直接编译 Calculator、2048、Mines 的生产 model，
  验证 reducer、合并规则、确定性随机数、邻居计数、连锁展开和非法输入不变性。
- openvela 目标配置下执行 `./build.sh ... -j2` 验证编译。
- 启动 goldfish 模拟器，在 NSH 中运行 `smart_band`。
- 使用模拟器传感器滚动脚本验证心率、温度、湿度和电池状态进入 UI。
- 手动点击和滑动验证页面切换、应用进入、返回和按钮操作。

Q1-C 的最终自动化验收还覆盖 compact/framed 主 UI 和 8 个 lazy app 的每一个对象创建
失败点，均要求清理后可重试；1000 次完整 UI 生命周期 object/event/timer 零净增长。
Linux GCC/gcov 总行覆盖率为 `90.9% (1388/1527)`，7 个 Q1-C 新生产源文件各自不低于
90%。固定版本 openvela fresh/incremental 构建和 native Heart Rate 旅程均通过，结果见
`docs/q1c-runtime-platform-20260720.md`。

## 9. 后续维护建议

- 若要改中文 UI，需要先接入中文字体，再重新检查小屏排版。
- 若要上真实硬件，需要确认传感器设备节点名称，必要时在 `sensor_bridge.c` 增加路径表。
- 若要新增小应用，建议在 `apps/` 下新增单独文件，并在 `smart_band_apps.c` 中注册。
- 若提交平台要求不同开源协议，应同步更新 `LICENSE`、`NOTICE` 和
  `THIRD_PARTY_NOTICES.md`。
