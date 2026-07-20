# openvela 复现说明

本文档说明如何把 `smart_band` 应用接入已有 openvela 工程，并在 goldfish
模拟器或开发板上复现。本文只覆盖本仓库提交的智能手环应用部分，上游 openvela
SDK、NuttX、LVGL、模拟器和工具链请按各自官方文档准备。

## 固定版本的 Nightly 构建

仓库的 `.github/workflows/openvela-nightly.yml` 每天定时运行，也支持从 GitHub
Actions 页面手动触发。它不是只检查脚本语法，而是同步固定的 openvela release、
执行 goldfish arm64 真实构建、验证最终 NuttX ELF 中包含 `smart_band`，随后启动
固定版本的 headless emulator 并运行 native 应用 smoke。

openvela manifest 提交、manifest 文件、官方 `.claude` URL 和提交的唯一版本清单是
`skills/openvela-smart-band-reproduce/versions.env`。Nightly 和本地复现脚本都读取该
文件，不在 workflow 中各维护一份版本号。Google `repo` launcher/tool 当前固定为
`2.54`，launcher 还会校验 workflow 中记录的固定 SHA-256。

workflow 会生成 `repo manifest -r`，并拒绝任何未解析成 40 位 Git SHA 的项目。
因此定时构建不会悄悄追踪 `dev`、`trunk` 或其他浮动 HEAD。需要升级 openvela 时，
应在同一个 PR 中更新上述固定值，先手动运行 workflow，确认 resolved manifest、
构建日志和 NuttX SHA-256 artifact 后再合并。

GitHub hosted runner 是一次性的。workflow 有意不缓存 `.repo`、源码树或构建目录，
避免恢复与固定 manifest 不一致的 mutable checkout；它会清理 runner 中与本项目
无关的 Android/.NET/GHC SDK 来获得同步空间。repo sync 和构建分别有硬超时，
整个 job 也有总超时。无论成功或失败，以下证据都会上传并保留 14 天：

- resolved SHA manifest 与 manifest/.claude 实际提交；
- repo sync、openvela build 完整日志；
- emulator/tools 固定提交、二进制 SHA-256、PTY/console runtime transcript；
- runner 磁盘状态、repo dirty status、最终 `.config`；
- 成功时的 NuttX ELF、文件类型与 SHA-256。

超时、sync 失败、构建失败、配置未启用、找不到 NuttX ELF，或无法在 ELF 中确认
`smart_band`，以及无法启动 emulator、到达 NSH、创建 UI 或保持应用进程存活，
都会让 job 失败，不会用占位步骤报告成功。

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
- goldfish 模拟器或实际开发板。

真实 provider 额外需要 `SENSORS`、`UORB` 和
`LVX_DEMO_SMART_BAND_USE_SENSORS`；关闭 provider 时可仅运行模拟数据。

## 2.1 Skill 一键复现流程

本仓库内置复现 skill：

```text
skills/openvela-smart-band-reproduce/SKILL.md
```

完整自动复现流程如下：

1. 进入 openvela 根目录，准备 open-vela 官方 skill：

```sh
git clone https://github.com/open-vela/.claude.git .claude
```

如果 `.claude` 已存在，可以执行：

```sh
git -C .claude pull --ff-only
```

2. 如果 openvela 开发环境还没有搭建好，使用官方环境搭建提示：

```text
帮我搭建 openvela 开发环境
```

这一步应由 open-vela 官方 `.claude/skills/openvela-quickstart` 接管。该官方 skill
负责检测环境、安装依赖、初始化仓库、同步代码、编译和首次运行模拟器。

3. 官方 openvela 环境完成后，回到本仓库执行：

```sh
bash scripts/reproduce_openvela_demo.sh --openvela-root /path/to/openvela
```

首次写入前建议先运行无写入预检；脚本也会拒绝覆盖目标路径中的未提交改动：

```sh
bash scripts/reproduce_openvela_demo.sh \
  --openvela-root /path/to/openvela --dry-run --no-browser
```

只有在确认目标改动可以被覆盖后才使用 `--allow-dirty`。

该脚本会自动：

- 确认 `/path/to/openvela/.claude`，缺失时首次克隆。
- 同步 `openvela_app/smart_band` 到 `packages/demos/smart_band_basic`。
- 同步到 `apps/packages/demos/smart_band_basic` 镜像目录，如果该目录存在。
- 在 goldfish arm64 `defconfig` 中启用智能手环 demo 配置。
- 构建 openvela，优先尝试 CMake 构建，失败后尝试 legacy 构建。
- 启动本地 HTTP 服务并打开浏览器演示页。
- 将本仓库的复现 skill 安装到
  `/path/to/openvela/.claude/skills/openvela-smart-band-reproduce`，方便后续
  在 openvela 根目录继续使用。

最终浏览器演示页通常是：

```text
http://127.0.0.1:8765/demo/index.html
```

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
CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS=y
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
cp nuttx/.config cmake_out/vela_goldfish-arm64-v8a-ap/.config
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

启动成功后控制台会先输出：

```text
smart_band: UI ready
```

传感器就绪后还可能输出：

```text
smart_band: temperature sensor 29C
```

这是温度传感器读数日志，不是错误。

CI 或无图形 Linux 环境可自动执行同一条 native 路径：

```sh
python3 "$DEMO_ROOT/scripts/smoke_openvela_emulator.py" \
  --openvela-root "$OPENVELA_ROOT" \
  --evidence-dir /tmp/smart-band-emulator-smoke
```

脚本使用 PTY 操作真实 NSH，而不是只检查 ELF 字符串。它要求 emulator console
`ping` 成功、出现 `smart_band: UI ready`，并在两个时间点通过 `pidof` 确认应用仍
在运行。固定 goldfish 配置没有启用 ADB shell，因此不能用 `adb shell smart_band`
替代该检查。

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
/dev/uorb/sensor_humi0
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
- 天气应用湿度值跟随相对湿度传感器变化。
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
