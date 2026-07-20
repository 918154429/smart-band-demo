# openvela Smart Band Demo

这是一个运行在 openvela/NuttX 上的智能手环原生应用示例，主程序位于
`openvela_app/smart_band`。应用使用 C + LVGL 实现，面向手环/小屏设备展示
表盘、健康数据、传感器接入、页面切换和轻量小应用。

本仓库只提交智能手环应用本身，不包含 openvela SDK、NuttX、LVGL、模拟器、
交叉编译工具链等上游系统代码。评审或复现时，请把本目录复制到已有 openvela
工程的 `packages/demos/smart_band_basic` 位置后编译运行。

## 功能概览

- 表盘与时间显示：显示当前时间、日期、电池百分比、电池图形进度、充电闪电标识、
  温度、睡眠、心率、压力、天气摘要。
- 健康数据展示：心率优先读取 `/dev/uorb/sensor_hrate0`；步数优先读取
  `/dev/uorb/sensor_step_counter0`，不可用时从加速度变化推导；读不到传感器时
  保留模型兜底值，保证页面仍可展示。
- 传感器对接：已接入心率、加速度、计步、环境温度、相对湿度、电池容量和充电状态。
- 基础交互：支持左右滑动切换表盘、心率、计步和应用中心页面；应用中心点击图标
  进入不同小应用。
- 步数目标：计步页面支持调整目标步数，范围为 1000 到 50000。
- 应用中心：包含天气、计算器、倒计时、2048、秒表、扫雷、俄罗斯方块、电子木鱼
  8 个可运行小应用。
- 异常处理：LVGL 初始化失败会返回错误；传感器打开失败不会崩溃，会使用兜底数据；
  温度设备名不一致时自动尝试备用路径；电池和传感器读数做基本范围保护。

## 目录结构

```text
openvela_app/smart_band/        openvela 原生应用源码，可复制到 packages/demos
openvela_app/smart_band/apps/   每个小应用的 context、view 与 lifecycle 实现
openvela_app/smart_band/logic/  Calculator、2048、Mines 等无 LVGL 生产状态机
openvela_app/smart_band/ui/lvgl 通用 LVGL 组件与主页面 view
openvela_app/smart_band/include 公共头文件、模型和图标声明
openvela_app/smart_band/assets  项目图标与演示图片资源
demo/                           浏览器静态演示页面，便于无模拟器时录屏
docs/                           openvela 复现说明与演示说明
scripts/                        模拟器传感器滚动验证脚本
tests/                          本机基础逻辑测试
LICENSE                         本仓库原创代码和资源的开源许可证
NOTICE                          项目声明、资产声明和上游边界说明
THIRD_PARTY_NOTICES.md          第三方依赖声明
```

## 环境要求

已验证的目标环境：

- Ubuntu 20.04/22.04 或同类 Linux 开发环境。
- 已拉取并可正常编译的 openvela 工程。
- openvela goldfish arm64 模拟器配置：
  `vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap`。
- openvela 工程中已安装对应交叉编译工具链、Python、make、CMake 等基础构建工具。

基础 UI 运行需要启用的 openvela/NuttX 能力：

- `GRAPHICS_LVGL`
- `LV_USE_NUTTX`
- `LV_USE_NUTTX_LIBUV`
- `LVX_USE_DEMO_SMART_BAND_BASIC`
- 目标板或模拟器对应的 framebuffer/display/input 配置

真实传感器模式还需要启用 `SENSORS`、`UORB` 和
`LVX_DEMO_SMART_BAND_USE_SENSORS`。若目标只需要演示数据，可关闭 provider；此时
`sensor_bridge.c` 使用无设备依赖的空 provider，模型明确运行在 simulation 模式。

## 接入 openvela 工程

假设 openvela 根目录为 `$OPENVELA_ROOT`，本仓库目录为
`$OPENVELA_ROOT/workspaces/smart-band-demo`。

### 一键复现 Skill

本仓库提供了一个复现 skill：

```text
skills/openvela-smart-band-reproduce/SKILL.md
```

推荐复现流程：

1. 由仓库脚本准备官方 skill。脚本会读取
   `skills/openvela-smart-band-reproduce/versions.env`，把 `.claude` 固定到仓库默认
   commit，并校验 openvela 使用清单中固定的 `tags/trunk-5.4.xml` release manifest 及
   manifest 仓库 commit。不要用未固定版本的 `git clone` 或 `git pull` 替代：

```sh
bash scripts/reproduce_openvela_demo.sh \
  --openvela-root "$OPENVELA_ROOT" --dry-run --no-browser
```

为避免清单外漂移，脚本会拒绝存在未审阅 `.repo/local_manifests/*.xml` 的 checkout。

需要复现经过审阅的其他版本时，可显式传入完整 40 位 commit：

```sh
SMART_BAND_CLAUDE_REVISION="$CLAUDE_COMMIT" \
SMART_BAND_OPENVELA_MANIFEST_REVISION="$OPENVELA_MANIFEST_COMMIT" \
SMART_BAND_OPENVELA_MANIFEST_FILE="$OPENVELA_MANIFEST_FILE" \
  bash scripts/reproduce_openvela_demo.sh --openvela-root "$OPENVELA_ROOT"
```

2. 如果 openvela 开发环境还没有安装完成，使用官方环境搭建提示：

```text
帮我搭建 openvela 开发环境
```

这会交给 open-vela 官方 `.claude/skills/openvela-quickstart` 流程处理依赖安装、
仓库初始化、同步、编译和首次模拟器启动。

3. 官方环境搭建完成后，在本仓库执行：

```sh
bash scripts/reproduce_openvela_demo.sh --openvela-root "$OPENVELA_ROOT"
```

这个脚本会同步 `smart_band` 到 openvela、启用配置、构建 goldfish arm64，并打开
浏览器演示页：

```text
http://127.0.0.1:8765/demo/index.html
```

脚本还会把本仓库的复现 skill 安装到：

```text
$OPENVELA_ROOT/.claude/skills/openvela-smart-band-reproduce
```

因此后续在 openvela 根目录中，也可以直接使用该 skill 继续复现或排查。

脚本默认拒绝覆盖 openvela 中已经变脏的目标配置或应用目录。应用与 skill 使用
overlay 同步，不删除目标目录中的额外文件；goldfish defconfig 会先备份，任一步骤
失败或脚本被中断时自动恢复。正式执行前可先做无写入检查：

```sh
bash scripts/reproduce_openvela_demo.sh \
  --openvela-root "$OPENVELA_ROOT" --dry-run --no-browser
```

如果已经审阅并确认目标目录中的本地改动可以被覆盖，必须显式增加
`--allow-dirty`。任一同步、配置或构建步骤失败时脚本都会以非零状态退出。

1. 复制应用到 openvela packages demos 目录：

```sh
mkdir -p "$OPENVELA_ROOT/packages/demos/smart_band_basic"
rsync -a \
  "$OPENVELA_ROOT/workspaces/smart-band-demo/openvela_app/smart_band/" \
  "$OPENVELA_ROOT/packages/demos/smart_band_basic/"
```

2. 如果你的 openvela 工程同时维护 `apps/packages` 镜像目录，也同步一份：

```sh
mkdir -p "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic"
rsync -a \
  "$OPENVELA_ROOT/workspaces/smart-band-demo/openvela_app/smart_band/" \
  "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"
```

3. 打开配置并启用应用：

```sh
cd "$OPENVELA_ROOT"
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap menuconfig
```

在配置中打开：

```text
LVX_USE_DEMO_SMART_BAND_BASIC=y
LVX_DEMO_SMART_BAND_USE_SENSORS=y
LVX_DEMO_SMART_BAND_BASIC_PRIORITY=100
LVX_DEMO_SMART_BAND_BASIC_STACKSIZE=32768
```

如果当前 openvela 版本不会自动扫描 `packages/demos` 子目录，需要在上层 Kconfig
和 Make/CMake 入口中手工包含 `packages/demos/smart_band_basic`。本应用目录内
已经提供 `Kconfig`、`Make.defs`、`Makefile` 和 `CMakeLists.txt`。

## 编译

在 openvela 根目录执行：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

如果需要清理后完整编译：

```sh
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap distclean -j2
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap -j2
```

本地开发时，如果模拟器启动目录使用 `cmake_out/vela_goldfish-arm64-v8a-ap`，可在
构建后复制产物：

```sh
mkdir -p cmake_out/vela_goldfish-arm64-v8a-ap
cp nuttx/nuttx cmake_out/vela_goldfish-arm64-v8a-ap/nuttx
for f in vela_system.bin vela_data.bin vela_ap.bin nuttx.bin nuttx.hex; do
  [ -f "nuttx/$f" ] && cp "nuttx/$f" "cmake_out/vela_goldfish-arm64-v8a-ap/$f"
done
```

## 运行

启动 goldfish 模拟器：

```sh
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap \
  -skin xiaomi_smart_screen_10 \
  -skindir "$OPENVELA_ROOT/prebuilts/emulator/skins/"
```

进入 NuttShell 后运行：

```sh
smart_band
```

如果使用开发板，完成烧录后在串口 shell 中执行同样的命令：

```sh
smart_band
```

## 传感器来源

应用启动后会尝试打开以下设备：

```text
/dev/uorb/sensor_hrate0          心率
/dev/uorb/sensor_step_counter0   计步
/dev/uorb/sensor_accel0          加速度，计步不可用时用于推导步数
/dev/uorb/sensor_ambient_temp0   环境温度
/dev/uorb/sensor_temp0           温度备用路径
/dev/uorb/sensor_humi0           相对湿度
/dev/charge/goldfish_battery     电池容量和充电状态
```

传感器读取策略：

- 心率：读取到有效 bpm 后更新 UI，显示范围保护为 40 到 500 bpm。
- 步数：优先使用 step counter；如果没有 step counter，则根据加速度能量变化增加步数。
- 温度：直接使用温度传感器整数摄氏度值，不再人为压缩到固定天气范围。
- 湿度：读取相对湿度传感器并限制在 0% 到 100%；无数据时保留模型兜底值。
- 电池：读取容量百分比和充电状态；充电时在电池图形旁显示闪电。
- 时间：使用系统本地时间；在模拟器中按 Asia/Shanghai/UTC+8 口径显示。

每项健康指标都会记录数据来源和新鲜度。真实样本短暂中断时，在 5 秒 TTL 内
保留最近值并标记为 stale；超过 TTL 后，自动模式才回退到独立模拟值。加速度
推导步数拥有独立累计基线，不再叠加模型模拟步数。若系统墙钟发生回拨，旧样本
会立即视为过期，避免 stale 数据无限保留。

可用脚本滚动模拟器传感器，验证 UI 是否随传感器变化：

```sh
SMART_BAND_ROLL_LOOPS=4 SMART_BAND_ROLL_DELAY=2 \
  scripts/roll_emulator_sensors.sh
```

## 操作说明

- 左右滑动：切换表盘、心率、计步、应用中心。
- 应用中心：点击图标进入小应用。
- 返回按钮：应用详情页左上角 `<` 返回应用中心。
- 计步页面：使用 `-` 和 `+` 调整目标步数。
- 倒计时：使用 `-1m`、`+1m` 修改倒计时时间，`Start/Pause` 控制运行，
  `Reset` 恢复到 05:00。

## 本机基础测试

本仓库包含五组不依赖 openvela 的 host C 门禁。Python 入口会寻找
GCC、Clang 或 MSVC，直接编译生产模型、无传感器 provider、应用 runtime、Timer、
Stopwatch 以及完整 LVGL/UI 源集，再执行测试或编译链接 smoke：

```sh
python3 tests/test_watch_model.py
python3 tests/test_time_apps.py
python3 tests/test_app_runtime.py
python3 tests/test_app_logic.py
python3 tests/test_ui_compile.py
bash scripts/test_reproduce_failure.sh
npm ci
npx playwright install chromium
npm run test:browser
```

测试覆盖时间格式、页面切换、数据来源/TTL/墙钟回拨、无传感器构建、模拟数据范围、
步数目标，以及应用 runtime 的 owned container、失败回滚、双实例隔离、1000 次
mount/unmount，Timer/Stopwatch 的后台计时、卸载重挂载和 tick 回绕。测试不再维护
与生产代码分离的 Python 模型副本，并直接验证 Calculator、2048、Mines 的生产
reducer、显式 seed 与边界条件。浏览器门禁覆盖 320x568、667x375、焦点保留、
ARIA live、对比度、触控尺寸和 reduced-motion。Linux CI 还使用
`tests/test_core_coverage.py` 对完整 host-testable production C core 强制至少 85% 行
覆盖率。也可以通过 `CC` 环境变量指定编译器。

## 基本异常处理说明

应用对常见失败路径做了保护：

- `smart_band_main.c` 检查 LVGL 是否重复初始化、显示设备是否创建成功、UI 是否创建成功。
- `sensor_bridge.c` 对空指针、传感器打开失败、读取不到数据、电池 ioctl 失败进行容错。
- 温度传感器先尝试 `sensor_ambient_temp0`，失败后尝试 `sensor_temp0`。
- 湿度传感器不可用时保留模型兜底值，不影响天气应用启动。
- 计步传感器不可用时自动回退到加速度推导；全部传感器不可用时保留模型模拟值。
- UI 创建函数遇到对象创建失败会返回错误码，避免继续访问空对象。
- 小应用内部对边界值做限制，例如倒计时最大 99 分钟、步数目标最大 50000、
  2048/扫雷/俄罗斯方块均有状态保护。

## 开源协议与合规

本仓库已补充以下合规文件：

- `LICENSE`：本项目原创代码和项目资源使用 MIT License。
- `NOTICE`：说明项目归属、提交边界、图标资源和未包含的上游系统部分。
- `THIRD_PARTY_NOTICES.md`：列出运行或构建所依赖的第三方项目及其许可证口径。

重要边界：

- 本仓库不重新分发 openvela、NuttX、LVGL、libuv、Android Emulator/QEMU 等上游代码。
- 本仓库中的图标资源位于 `openvela_app/smart_band/assets/generated_icons`，
  为项目自有资源；没有复制第三方商标、品牌图标或示例仓库的图片。
- 如果提交平台要求跟随上游工程统一许可证，请以提交平台规则为准；本目录自身
  以 `LICENSE` 和 `NOTICE` 为准。

## 相关文档

- [openvela 复现说明](docs/openvela-runbook.md)
- [实现过程说明](docs/implementation-notes.md)
- [第三方依赖声明](THIRD_PARTY_NOTICES.md)
