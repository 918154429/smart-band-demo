# smart-band-demo 下一会话交接

更新时间：2026-07-20（Asia/Shanghai）

## 1. 仓库与权限

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub 主仓库：<https://github.com/918154429/smart-band-demo>
- `origin`：`git@github.com:918154429/smart-band-demo.git`
- `upstream`：`https://www.gitlink.org.cn/dhy000123/smart-band-demo.git`
- 默认分支：`master`
- 本轮功能基线：`a41d076c3822165a69db1e48ad2b32c86a3fb09e`
- 工作约定：用户已明确授权自由修改、提交、推送、创建/合并 PR，并允许使用子智能体。

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

当前没有开放 PR。

## 3. 当前验证结果

### Host 与浏览器门禁

- GCC、Clang、MSVC 均以严格警告运行生产 C 测试。
- 生产 C core 行覆盖率：`86.4%`，`909/1052`，门槛为 `85%`。
- Chromium 浏览器测试：`6/6`。
- 已覆盖 `320×568`、`667×375`、焦点/DOM identity、独立 ARIA live、44×44 触控目标、4.5:1 对比度和 reduced-motion。
- PR7 最新快速门禁：
  - Host：<https://github.com/918154429/smart-band-demo/actions/runs/29725933656>
  - Browser：<https://github.com/918154429/smart-band-demo/actions/runs/29725933647>

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

## 6. 当前边界：尚未证明什么

以下事项没有被 runtime smoke 覆盖，不应误报为已完成：

- Native emulator 像素截图是否与浏览器原型完全一致。
- Native 触摸/滑动和每个小游戏的端到端交互。
- Emulator sensor console 注入后，UI 值是否按来源/新鲜度模型更新。
- 真机烧录、真实传感器和功耗表现。

现有浏览器测试覆盖响应式、焦点、触控尺寸和对比度；它不替代 native 像素/输入测试。

## 7. 下一会话建议

本轮请到此收尾，不要自动继续扩展。下一会话先让用户选择后续目标。建议候选顺序：

1. Native 交互与传感器 E2E：截图、触摸/滑动、console sensor 注入与 UI/日志断言。
2. 继续拆分 `app_lvgl.c`（当前约 995 行），把导航/controller 与 view 进一步隔离。
3. 比赛/发布交付：固定 source archive、SHA-256、演示录屏清单和最终复现报告。

不要回头重做 PR1–PR7；除非新证据推翻现有门禁。

## 8. 下一会话首轮命令

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
git switch master
git pull --ff-only origin master
git status --short --branch
git log -5 --oneline --decorate
gh pr list --repo 918154429/smart-band-demo --state open
gh run list --repo 918154429/smart-band-demo --limit 10
```

快速 host 验证：

```powershell
python tests\test_watch_model.py
python tests\test_time_apps.py
python tests\test_app_runtime.py
python tests\test_app_logic.py
python tests\test_ui_compile.py
python tests\test_emulator_smoke.py
```

Windows 本机的 POSIX fake-emulator case 会跳过；它已在 Ubuntu 实际执行 4/4 通过。Shell 复现测试使用 Git Bash：

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
