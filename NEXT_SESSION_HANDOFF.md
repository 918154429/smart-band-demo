# smart-band-demo 下一会话交接

更新时间：2026-07-20（Asia/Shanghai）

> 复赛方向已确认：后续开发以根目录 `FINALS_TOP_TIER_ROADMAP.md` 为工程主路线，
> 只推进产品功能 A–G、native 验证、稳定性和开发板适配。视频与比赛平台提交事务不在
> 工程路线内；GitHub 工程提交属于持续开发流程。Q0 与 Q1-V 已完成；Q1-C 第一半代码与本地回归已完成，但 Linux coverage
> 和真实 openvela 构建尚未在本轮确认。下一对话先关闭这两个 Gate，再推进 Q1-C 后半。

## 1. 仓库与权限

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub 主仓库：<https://github.com/918154429/smart-band-demo>
- `origin`：`git@github.com:918154429/smart-band-demo.git`
- `upstream`：`https://www.gitlink.org.cn/dhy000123/smart-band-demo.git`
- 默认分支：`master`
- 当前功能基线：`1ddc533cb3096e541fe5cadaed810df39029ebc1`
- 工作约定：用户确认其对 GitHub 主仓库拥有完整管理权并对仓库负责，长期授权 Codex
  按持续开发需要自主修改、提交、推送以及创建、更新和合并 PR，无需逐次申请；强制
  推送、改写已发布历史、删除远端分支/标签和正式 release 仍需当次明确指示。允许使用
  子智能体处理独立工程子任务。
- 当前工作树含本轮未提交的路线图/交接、Q0/Q1-V 脚本与测试、CI/README、证据报告和
  compact evidence；这些均为已知改动，不得在下一会话清除或覆盖。

开始下一会话时仍应先读取用户最新指令；若与本文件冲突，以最新指令为准。

## 2. 原始路线图与完成状态

首轮审计的目标是把项目从“功能初步可演示”提升到“可验证、可维护、可复现”。
原始四个 phase 已全部完成，并在排查真实 fixed-release 构建时追加了三个闭环 PR。

| PR | 状态 | 内容 | Merge commit |
| --- | --- | --- | --- |
| [PR1](https://github.com/918154429/smart-band-demo/pull/1) | 已合并 | 生命周期、真实生产 C 测试、脚本 fail-fast/executable bit、复现安全门禁 | `91821bbe686aa152f4122c8c7428f87f76c75289` |
| [PR2](https://github.com/918154429/smart-band-demo/pull/2) | 已合并 | 传感器来源/新鲜度模型、真实与模拟数据隔离、Timer/Stopwatch 单调时间 | `cd62eef78800a8eb72eeb81f460e2d0cb686ed7d` |
| [PR3](https://github.com/918154429/smart-band-demo/pull/3) | 已合并 | 应用 context、mount/unmount、descriptor registry、LVGL owned container 与逻辑拆分 | `64bebf485fd85debfe3da210f2b4dec5e3b5b97c` |
| [PR4](https://github.com/918154429/smart-band-demo/pull/4) | 已合并 | 响应式/可访问 UI、焦点稳定更新、覆盖率/编译矩阵、固定复现与 nightly | `0d1dd28dd9517b14f638a88c95f83c2dd1206ca5` |
| [PR5](https://github.com/918154429/smart-band-demo/pull/5) | 已合并 | 兼容 `repo` manifest include wrapper | `093fa199f3c5d8ab775a04d4bd5d5d151e2df91e` |
| [PR6](https://github.com/918154429/smart-band-demo/pull/6) | 已合并 | 修复 openvela `-O3 -Werror` 下 Stopwatch 格式截断警告 | `30821658c976d8341acfa1aa50d703ee0579045b` |
| [PR7](https://github.com/918154429/smart-band-demo/pull/7) | 已合并 | 固定 emulator、真实 headless boot、PTY/NSH native runtime smoke、证据与清理 | `a41d076c3822165a69db1e48ad2b32c86a3fb09e` |
| [PR8](https://github.com/918154429/smart-band-demo/pull/8) | 已合并 | 独立远端 simulator 复跑、运行镜像 artifact、OpenGL 依赖预检与试运行记录 | `1ddc533cb3096e541fe5cadaed810df39029ebc1` |

当前没有开放 PR。

## 3. 当前验证结果

### Host 与浏览器门禁

- GCC、Clang、MSVC 均以严格警告运行生产 C 测试。
- 生产 C core 行覆盖率：`86.4%`，`909/1052`，门槛为 `85%`。
- Chromium 浏览器测试：`6/6`。
- 已覆盖 `320×568`、`667×375`、焦点/DOM identity、独立 ARIA live、44×44 触控目标、4.5:1 对比度和 reduced-motion。
- 当前 `master@1ddc533` 主门禁：
  - Host：<https://github.com/918154429/smart-band-demo/actions/runs/29730739442>
  - Browser：<https://github.com/918154429/smart-band-demo/actions/runs/29730739529>
- 2026-07-20 最终审计又在本地重跑 MSVC `/W4 /WX` 五组生产 C 门禁、Browser `6/6`、
  Shell syntax/rollback；Q0 receipt 已绑定这些结果和上述 `master` Actions runs。

### Q1-C 第一半：当前未提交实现

- 新增 `smart_band_runtime_t`，统一持有 model、sensor bridge、app registry、event queue、
  clock 与 capability snapshot；`app_lvgl.c` 只提供 LVGL clock/controller 适配，周期顺序
  固定为 model -> sensor -> apps。
- 定长队列容量为 16，无堆分配；首批事件类型与路线图一致。优先级由事件类型和通知
  kind 推导，普通 metrics 可合并，高优先级来电与 critical checkpoint/flush 可淘汰
  最旧低优先级项，同级 FIFO。当前队列只允许 UI/controller 单线程访问；跨线程 callback
  串行化属于 Q1-C 后半，不得误报为已完成。
- clock source 可注入 wall 与 32-bit monotonic callback；累计 monotonic elapsed 跨 wrap，
  保留最近有效墙钟以检测 `valid -> invalid -> earlier valid` 回拨。无 RTC 时 UI 显示
  `--:--`，model 和 app monotonic tick 继续推进。
- sensor provenance 仍保留 wall timestamp，TTL 走 monotonic elapsed；墙钟回拨会按既有契约
  立即淘汰旧 sensor sample。Timer、Stopwatch、Mines 和 Wooden Fish 的交互 callback 与
  runtime tick 已统一使用同一 monotonic provider。
- 新增 `tests/test_runtime_core.py` 与 GCC/Clang/MSVC CI matrix；coverage 脚本新增四个
  service 独立 `>=90%` 门禁，并保留总体 `>=85%` 门禁。
- 2026-07-20 本机 MSVC `/W4 /WX` 六组生产 C 和完整 UI 链接均通过；Browser `6/6`；
  emulator/Q0/Q1-V harness 分别 `5/5`、`15/15`、`14/14`，各有 1 个 Windows POSIX case
  按设计跳过；Git Bash syntax/rollback 通过。
- 本机没有 GCC，`tests/test_core_coverage.py` 在工具预检明确停止于
  `required coverage tool is unavailable: gcc`。本轮也未重跑真实 openvela build；两项
  都是未确认 Gate，不是已通过结果。

本节改动尚未提交、推送或创建 PR。

### 固定 openvela 构建与真实 runtime

最终成功 run：<https://github.com/918154429/smart-band-demo/actions/runs/29725945155>

- 被验证提交：`82c066cce36b5b05f5a8e90d3093aaaefbb1e04f`（PR7 合并前功能 head；已原样进入 merge commit）。
- openvela manifest commit：`67df2c52308f2579ac50d0cd7413e7f0e092b83a`
- `.claude` commit：`ab5f8be8225ce25c2f808fae0085dbf2db8fadf4`
- Linux emulator commit：`be9cdef6709c2a7aed547c3029d8872c58e5f3f9`
- Emulator tools commit：`37f5024f1d9157b9778d0d9e739ee0fa68743d42`
- NuttX ELF SHA-256：`5f97a280c2478ab94116be111fecef63cd103ce0612ef14e5513933218091d58`
- Emulator SHA-256：`1a8671a1e9a68a25e5cdf006dd291c06a30f7783440a3a974930d85ad7526d7d`
- Headless aarch64 QEMU SHA-256：`760f02aa25f5f041de62adf2b1c60445046918bef0a0572bb661128a21d9d450`
- NSH prompt：`goldfish-armv8a-ap> `
- Boot 到 NSH：`0.522s`
- Emulator console：`ping` 返回 `I am alive!` 和 `OK`
- Native 应用：控制台出现 `smart_band: UI ready`
- 稳定性：启动后 10 秒与 15 秒两次 `pidof smart_band` 都返回 PID `13`
- Fatal marker：未出现 display init failure、UI create failure、LVGL already initialized、PANIC、assertion 或 segmentation fault。
- 清理：console 请求关闭后，再对专属进程组执行受控 TERM；workflow 还有基于记录 PGID 的 always-run 兜底。

Artifact：`openvela-fixed-release-29725945155`，artifact ID `8454484599`，默认保留 14 天。

下载证据：

```powershell
$dest = Join-Path $env:TEMP 'smart-band-run-29725945155'
New-Item -ItemType Directory -Force -Path $dest | Out-Null
gh run download 29725945155 `
  --repo 918154429/smart-band-demo `
  --name openvela-fixed-release-29725945155 `
  --dir $dest
Get-Content (Join-Path $dest 'runtime-smoke.json')
```

### Q0 性能/资源基线

正式证据：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q0-final-audited-20260720T2034CST
```

- 最终审计重跑 20/20 次独立冷启动通过，失败 0；首轮未隔离数据盘的结果只保留、不作 Gate。
- `smart_band -> UI ready`：p50 `0.8065 s`、p95 `0.821 s`、max `0.828 s`。
- emulator -> NSH：p50 `1.312 s`、p95 `1.364 s`、max `1.406 s`。
- NuttX ELF `65,913,280` bytes；SHA 保持 `5f97a280...1d58`。
- 546 个 smart-band 匹配符号，大小求和 `303,999` bytes。
- 静态资源 `14` 个文件，源码侧合计 `2,027,043` bytes。
- 保留 artifact 没有 linker 原始 map；证据明确使用 `elf-symbol-map`，不误报。
- 正式模式不可降到 20 次以下或放宽 2 秒预算；每轮四输入全部 copy2 隔离。
- 20 个 run-id 全部唯一；每轮源 SHA、smoke cleanup、PGID、端口和暂存清理均通过；
  批次前后 artifact 不变。
- Gate receipt/source manifest/资源锚点与 7 个 required checks 全绿；543 项 evidence SHA 复核通过。
- 最终脚本 SHA：collector
  `3839647d7163fb3eb5389b91d79732fadfd4ba2dfb205dadb655cd994eec4181`，smoke helper
  `eff60209d49f8e4c988f22a7e80deea8ced3d59ed3b305e4f7740a70c979811a`。
- compact 判定/启动/资源/完整性/receipt/tree/manifest SHA 依次为
  `0ab92d0b...f8a83`、`689dcb23...62526d`、`911ea594...988d`、
  `994ef6f1...0a597`、`1de30bf4...200e2`、`b750999a...3e630`、
  `6c1b93b2...e6cd`。

### Q1-V native 截图、输入与 sensor E2E

最终可复跑证据：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q1v-final-audited-20260720T2031CST
```

- `scripts/run_native_e2e.py` 在独立 runtime output 中启用 emulator heart-rate capability。
- console framebuffer 生成三张 `1280x800` RGBA8 native PNG。
- goldfish touchscreen 右向左滑动将表盘切到 Heart Rate / Model。
- 连续注入 heart-rate `104` 后，截图显示 `104 bpm / Sensor`。
- 页面区域变化 `143,269` 像素；数值区 `1,874`；Source 区 `612`。
- 第一次 swipe attempt 即成功；`Heart Rate`、`104 bpm`、`Source / Sensor` 三个精确
  RGBA golden ROI 全部匹配。
- 结构化状态为 `page=heart_rate, value_bpm=104, source=sensor, freshness=fresh`；它只证明
  当前 fresh sensor 帧，停止上报后的 stale/TTL fallback 仍未证明。
- uORB 节点、console set/get、PID/ps、fatal marker、固定 artifact 不变均通过。
- `.config`、`nuttx`、`vela_system.bin`、`vela_data.bin` 全部独立复制；唯一 run-id 的
  `/proc` 进程归因通过，进程、端口、runtime output tree、暂存输入最终清理，51 项
  manifest 复核通过。
- 远端 `du` 观测的最终 evidence 目录分配量约 `432 KiB`；harness SHA 为
  `9f3795928d1018154136c34014a1d161720104058198ea915865719bb9c433d2`，
  `journey.json` SHA 为
  `b7cafe35b6eb68005cb34438df4f079d0b836a8f01c77e59b006ebe368ddef36`，manifest SHA 为
  `86683730704098e7bff6766ccb4bbdf7f82270329ee7a56bb175453f6cd243e4`。

本地审阅图和完整判定见 `docs/q0-q1v-baseline-20260720.md`。

## 4. PR7 的重要设计决策

### 为什么不用 `adb shell smart_band`

固定 goldfish 配置启用了 ADB QEMU transport 和 file service，但没有启用：

```text
CONFIG_PSEUDOTERM
CONFIG_SCHED_CHILD_STATUS
CONFIG_ADBD_SHELL_SERVICE
```

因此 `adb devices` 在线并不代表 `adb shell` 可执行命令。PR7 采用 openvela 官方测试框架同类路径：

```text
fixed emulator -> POSIX PTY -> real NSH -> smart_band & -> UI ready -> pidof twice
```

Emulator console 5554 只用于独立 liveness 和关闭，不用于执行 NSH 命令。

### Runtime smoke 的代码位置

- `.github/workflows/openvela-nightly.yml`
- `scripts/smoke_openvela_emulator.py`
- `tests/test_emulator_smoke.py`
- `openvela_app/smart_band/smart_band_main.c`
- `skills/openvela-smart-band-reproduce/versions.env`

脚本会：

1. 校验 emulator/frontend 与 headless QEMU ELF 和动态库。
2. 使用固定 skin、无窗口、关闭硬件加速、固定 console port 启动。
3. 等待真实 NSH prompt。
4. 验证 console `ping`。
5. 后台启动 `smart_band`，要求明确的 `UI ready`。
6. 两次检查 PID，并抓取 `ps`。
7. 捕获 SIGTERM/SIGINT，始终清理独立进程组。
8. 保存完整 transcript、JSON、SHA、ldd 和 cleanup 证据。

Legacy 构建 fallback 现在也会复制 `nuttx/.config` 到 CMake output 目录；没有 `.config` 时官方 `emulator.sh` 不会把该目录识别为 out-of-tree artifact。

## 5. 已闭环的失败，不要重复排查

- Run `29719640560`：`repo` manifest wrapper 表示差异；PR5 已修复。
- Run `29719961198`：Stopwatch 16 字节缓冲区在 `-O3 -Werror` 下触发格式截断；PR6 已修复。
- Run `29724670507`：`repo` 将 emulator remote URL 规范化为不带 `.git`；PR7 follow-up 已兼容两种 HTTPS 表示。
- Run `29725121927`：直接对 headless QEMU 执行 `ldd` 没有使用 emulator 打包的 `lib64`；现已用真实 launcher 等价的 `LD_LIBRARY_PATH` 严格验证。
- Run `29725945155`：最终完整成功。
- 首轮 Q0 `q0-baseline-20260720T1822CST`：未逐轮隔离数据盘，保留但已被审计重跑替代。
- 首轮 Q1-V `q1v-harness-final-20260720T1913CST`：只判断 ROI 变化并使用部分 hardlink，
  画面实际正确但 Gate 不充分，保留并由审计重跑替代。
- 中间 Q0 `q0-audit-hardened-20260720T1953CST` 和 Q1-V
  `q1v-audit-hardened-20260720T1950CST`：保留但不再作为最终 Gate。
- 远端 Q1-V `q1v-final-audited-20260720T2026CST`：console 接受 swipe，但截图仍停在表盘；
  `Heart Rate` title golden 正确判失败，证明页面误绿路径已被拦截。

T2026 的失败 JSON/截图未复制进本地 `docs/evidence/`；本地保存的是最终 compact 子集与
远端清单，只能复算 compact 文件和清单自身，不能离线重哈希清单中的全部 543/51 个
远端工件。

## 6. 当前边界：尚未证明什么

以下事项仍未证明，不应因 Q0/Q1-V 通过而误报：

- 全页面/全部小游戏的 native 像素和交互回归。
- 第二基准分辨率、golden mask/SSIM、长时 native E2E。
- native sensor stale/TTL fallback；本轮启用的 emulator sensor 会持续上报。
- Q1-C 第一半的 Linux coverage 与真实 openvela 构建、Q1-C 后半及 Q1-S。
- 真机烧录、真实传感器和功耗表现。

现有 Browser 结果仍只做设计参考；本轮 Q1-V 证据来自 native framebuffer。

## 7. 下一会话建议

复赛目标已经冻结，不再让用户重新选择。当前 central runtime、事件队列、clock、
capability model 和 controller 接线均已实现，且未增加用户可见功能。下一对话：

1. 在 Linux/CI 运行 `python tests/test_core_coverage.py`，要求新增四个 service `>=90%`、
   总体 `>=85%`。
2. 用当前源码完成真实 openvela 构建；不得使用远端旧 dirty tree 代替本地工作树。
3. 两项通过后才进入 Q1-C 后半：dirty flags、platform no-op/loopback 和 fake LVGL
   第 N 次创建失败注入。

不得同时实现 Q1-S storage codec、表盘或其他 A-G 功能；Q1-C 总项仍保持未完成。

## 8. 下一会话首轮命令

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
git switch master
git status --short --branch
git log -5 --oneline --decorate
Get-Item -LiteralPath 'E:\C_Moved_From_C\Users\Lenovo\Desktop\AI Gateway 工具\Remote Codex.cmd'
```

远端现有独立工作区为 `/home/ubuntu/smart-band-sim-20260720-v1`，其中包含 `openvela/`、
`smart-band-demo/` 和 `evidence/`。下一对话先确认远端磁盘、进程、端口和源码状态，
只操作该项目目录与本轮启动的进程组；不在文档或日志中写入凭据。

用户确认远端磁盘容量为 `150G`，当前无需另租服务器。大型 checkout/build 前必须确认
该容量对应的实际挂载点和剩余空间，并在新隔离目录操作；此前记录的约 `25G` 可用空间
只代表当时检查到的文件系统，不代表远端总容量。

快速 host 验证：

```powershell
python tests\test_watch_model.py
python tests\test_time_apps.py
python tests\test_app_runtime.py
python tests\test_runtime_core.py
python tests\test_app_logic.py
python tests\test_ui_compile.py
python tests\test_emulator_smoke.py
python tests\test_q0_baseline.py
python tests\test_native_e2e.py
```

Windows 本机的 POSIX fake-emulator case 会跳过：emulator helper 共 `5` 项、Q0 共
`15` 项、Q1-V 共 `14` 项，各跳过 `1` 项。Ubuntu 实际执行 emulator `5/5`、Q0
`15/15`、Q1-V `14/14` 通过；CI 已增加双系统 evidence-harness job。Shell 复现测试使用 Git Bash：

```powershell
& 'D:\Program Files\Git\bin\bash.exe' -lc `
  'cd /e/C_Moved_From_C/Users/Lenovo/Desktop/schoolwork/smart-band-demo && bash -n scripts/*.sh skills/openvela-smart-band-reproduce/scripts/*.sh && bash scripts/test_reproduce_failure.sh'
```

手动重跑 fixed-release nightly：

```powershell
gh workflow run openvela-nightly.yml `
  --repo 918154429/smart-band-demo `
  --ref master `
  -f build_jobs=2
```

本机没有可用 GCC，因此不要把本地 coverage tool 缺失误判为代码失败；覆盖率以 GitHub Actions 的 Ubuntu job 为准。

需要复验 Q1-V 时，先把当前脚本安全复制到远端独立 tools 目录，再使用新的空 evidence
目录和空闲偶数端口运行：

```sh
python3 scripts/run_native_e2e.py \
  --openvela-root /home/ubuntu/smart-band-sim-20260720-v1/openvela \
  --evidence-dir /home/ubuntu/smart-band-sim-20260720-v1/evidence/q1v-<timestamp> \
  --console-port 5684
```

远端 `smart-band-demo` 仍停在 `b75cbb0` 且有既有 smoke/test 改动；其产品树 manifest
`aa7821...a246f` 也不同于当前基线 `9a3499...cfdf`。该差异已报告，不得自动 pull、
覆盖或拿它代替本地工作树。最终 Q0 使用独立只读 source snapshot；旧树清单保存在
`tools/audit-hardened-20260720T1944CST/remote-old-smart-band-tree.json`。固定 ELF 和数据
镜像 SHA 已单独复核。最终工具/源码 snapshot 为
`tools/audit-final-20260720T2025CST`；远端最终无 emulator/QEMU/`smart_band` host 进程、
无 `5690/5692/5694` 监听端口、无 runtime-output 目录残留，剩余磁盘
`26,559,209,472` bytes。
