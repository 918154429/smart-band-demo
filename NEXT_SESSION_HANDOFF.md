# smart-band-demo 下一会话交接

更新时间：2026-07-23（Asia/Shanghai）

> 复赛工程以根目录 `FINALS_TOP_TIER_ROADMAP.md` 为主路线。Q0、Q1、W1、Q2 A Gate 与
> Q3 B/D 软件 Gate 已全绿；Q4 主机侧通知闭环、Q5 软件电源策略和 Q6 history
> loopback 切片均已进入独立 PR，当前继续补 Q4 C Gate 与 Q6 后续软件范围。
>
> **当前唯一权威续接状态见第 12 节。** 第 7-11 节仅保留此前阶段历史；其中所有
> `OPEN`、红色 Gate、未运行 soak、heap/fd 未验证和“下一步”命令均已失效，不得执行。

## 1. 仓库、权限与边界

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub：<https://github.com/918154429/smart-band-demo>
- 默认分支：`master`
- Q1-S PR：[PR10](https://github.com/918154429/smart-band-demo/pull/10)，已正常合并。
- W1 integration PR：[PR11](https://github.com/918154429/smart-band-demo/pull/11)，已正常合并。
- Q2 PR：[PR12](https://github.com/918154429/smart-band-demo/pull/12)，已正常合并。
- Q3 PR：[PR13](https://github.com/918154429/smart-band-demo/pull/13)，已正常合并。
- Q1-S 功能提交：`0547953`；NuttX 符号冲突修复：`a9d5a43`；最终证据提交：`9172aef`。
- W1 功能/门禁提交：`298a5c0`；最终证据提交：`5433b9b`。
- Q2 功能提交：`07dece8`；门禁最终提交：`d7ce79b`；证据提交：`2da3d91`；merge：`e7e798c`。
- Q3 最终证据提交：`08b6fa64f1eaace9912cffdf3799c35a900c094e`；merge：
  `4f79ae04d5097effa294eb8dea776222cbb02d40`。
- 当前 master 合并基线：`4f79ae04d5097effa294eb8dea776222cbb02d40`。
- 用户确认其对主仓库拥有完整管理权并承担责任，长期授权 Codex 按持续开发需要修改、
  提交、推送、创建/更新/合并 PR。强制推送、历史改写、删除远端分支/标签和正式 release
  仍需当次明确指示。
- 当前会话允许子智能体，最多五个，且最多二级委派；额度和指令不自动延续到新会话。
- 视频、比赛平台提交和正式发布不属于工程路线。
- 开发板候选暂定为润芯微 Gemini-S1；下单/到货状态、实物 SKU/revision、配套固定 BSP
  和恢复工具仍未确认。未经 HW0 清点、恢复验证和逐次明确批准，不得烧录、写 flash
  或更改 bootloader。

若使用远端 Ubuntu，所有新工作必须留在 `/data`。不得访问 `/data/codex-audit`、
`/data/lxd-storage`、`/data/lost+found` 或旧 `/home/ubuntu/...` 项目树。

### Gemini-S1 板卡决策

- 官方公开事实：Rivotek Gemini-S1、全志 R528 双核 Cortex-A7、openvela 5.2 认证
  `OV20260412`。
- 公开 BSP：`vendor/allwinnertech/boards/r528/r528s3-gemini-s1`，当前在
  `dev-ai-contest-2026`，含 `nsh`、`nsh_minidisplay` 和 `bootloader`。
- 现有模拟器仍固定 `tags/trunk-5.4.xml`；不得为接板而覆盖 goldfish 固定基线。
- 到货前板卡线只允许 G0：隔离 checkout、固定 manifest/项目 SHA、compile/link-only，
  不烧录、不声明真机能力通过。
- 公开资料确认环境传感器与 BLE 配置，但未确认本项目所需的板载心率、step counter、
  accelerometer、电池计量和震动器。
- 完整能力矩阵、证据来源和 HW0-HW9 顺序见
  `docs/gemini-s1-target-board.md`。

## 2. 已完成基线

PR1-PR13 已完成生命周期、传感器来源、应用 runtime、响应式 UI、固定复现、真实 openvela
build/native smoke、Q1-S 存储底座、W1 第一波汇总、Q2 三表盘与 Q3 Workout/History 闭环。
当前 master 基线为 `4f79ae04d5097effa294eb8dea776222cbb02d40`；此前 Q1-C merge commit 为
`9f1f00fea130f7ccf4d79d89652a91e8fdfe0d13`。

按 A-G 七个产品 Gate 计，A、B、D 已完成，即 `3/7 = 42.9%`；C、E 已有 pure core，F 已有
envelope core，G 仅完成候选板与公开资料定位。该比例只表示产品 Gate 数量，不把 Q0/Q1/W1
基础工程和各 Gate 内部工作量折算成虚假的线性百分比。

Q0 正式 20 次冷启动与 Q1-V native fresh sensor 证据仍见：

- `docs/q0-q1v-baseline-20260720.md`
- `docs/evidence/q0-gate-summary-20260720.json`
- `docs/evidence/q1v-native-journey-20260720.json`

Q1-C central runtime、事件、时钟、能力、platform adapters 与 fake LVGL 证据见：

- `docs/q1c-runtime-platform-20260720.md`
- `docs/evidence/q1c-gate-summary-20260720.json`

## 3. Q1-S 已完成

### 格式与 store

- 固定 36-byte little-endian header，magic `SBST`、format `1.0`，最大 payload 512 bytes。
- 显式 record type、schema major/minor、64-bit generation、header/payload IEEE CRC32；
  不落盘裸 C struct，严格验证 frame 精确长度。
- 每种 record 使用两个显式 object ID。commit 读取两槽、写另一槽、flush、回读验证。
- 单槽损坏/缺失/不支持可回退；双槽损坏或同 generation 冲突降级默认值；不确定 read
  error 在任何 write 之前终止；`UINT64_MAX` generation 拒绝继续提交。
- migration 使用类型化 OK/unsupported/buffer-too-small/invalid 结果并校验输出长度。

### backend 与 runtime

- memory backend 固定 `16 x 4096` bytes，无堆分配。
- file backend 映射 `object-%08x.bin`，单对象 4096 bytes，上限 16 个 dirty object，
  `fsync/_commit` flush，不依赖 rename。
- fault plan 支持第 N 次 EIO、ENOSPC、EROFS、短写、截断、腐坏和 interrupted mixed image。
- runtime 拥有 store，启动加载固定 runtime-checkpoint A/B record；存储错误不会中止 UI
  初始化。`LVX_DEMO_SMART_BAND_STORAGE_PATH` 为空时使用 no-op，非空目录必须预先存在。
- 本切片没有新增任何 A-G 用户功能或历史 payload schema。

### 覆盖率与故障矩阵

独立 Linux 目录：

```text
/data/smart-band-q1s-20260721T132034CST
```

- 最终 coverage snapshot：`source-v6`
- snapshot archive SHA-256：
  `1c8fbe9a27793133ce604c29b2472c75509f46313e85acb9c5f07e7e056792ad`
- coverage log：`evidence/coverage-v6.log`
- overall `92.2% (2125/2305)`；codec `100%`、store `95.9%`、fault `93.3%`、memory
  `93.5%`、file `91.4%`，每个 Q1-S 生产源文件均 `>=90%`。
- 160 个逐字节 crash 切点覆盖空/已占用 inactive slot、generation zero、short write、
  interrupted mixed image；另覆盖 EIO、ENOSPC、EROFS、truncate、CRC、迁移与读前写保护。
- v2-v5 的 POSIX 声明、coverage runner 参数和 file coverage 失败证据已保留。

### GitHub/openvela 最终证据

第一次 fresh run `29805084515` 在 `0547953` 上失败：NuttX 已声明 `file_read/file_write`，
与 file backend 静态回调冲突。失败 artifact 保留；修复提交 `a9d5a43` 改用
`storage_file_*` 回调名。

最终 run：<https://github.com/918154429/smart-band-demo/actions/runs/29806148523>

- source commit：`a9d5a4326063c75dabea4d61c31152ba981b15a7`
- 214 个 openvela project 全部解析为固定 SHA。
- build、镜像链接校验、native smoke、清理、artifact 上传全部通过。
- artifact：`openvela-fixed-release-29806148523`，`23,615,392` bytes。
- artifact digest：
  `sha256:cd24812b5eb4c681a5b03a27a06082a777edbd1d4460521dd9406faa02a3f9e4`
- NuttX：`66,073,888` bytes，SHA-256
  `b6605449990f01ab48c747c5e605ad4136eac7f8e1ef2dd6eb9831e282dff0dc`。
- `.config` SHA-256：`e15bd57b33f7ea33132fd2bfde144b6bdd07291a1795de147dabb8b448d06e10`。
- `vela_system.bin`：
  `c432a814d04355da298268dc6fc6caafecfbe7cd7ce0fe799deed8949c8614a6`。
- `vela_data.bin`：
  `9b4405cb8a1ab36f0cc852300d7810957f218566fd5072d4a80b39d667d685ee`。
- native `UI ready`：`0.303s`；PID 13 在 5 秒检查后仍为 13；清理通过。
- Host run `29806142043` 与 Browser run `29806142041` 全绿。

紧凑证据：

- `docs/q1s-versioned-storage-20260721.md`
- `docs/evidence/q1s-gate-summary-20260721.json`

## 4. W1 第一波完成

五个叶子提交已通过所有权审计并按 T1 -> T5 汇入。完成范围：

- Q2 registry/Lotus lifecycle 与实际 UI 路径。
- Q3-1 step normalizer 与 Walk/Run pure model。
- Q4-1 fixed notification queue/demo pure model。
- Q5-1 ACTIVE/DIMMED/SCREEN_OFF pure policy。
- Q6-1 stateless v1 envelope codec、CRC 与 golden vector。

独立 Linux coverage 在 `/data/smart-band-w1-integration-20260721`：overall
`94.0% (2945/3134)`，六个新生产源分别 `97.6%` 到 `100%`，全部 `>=90%`。最终 Host
run `29816173199`、Browser run `29816173171` 全绿。最终 PR head `5433b9b` 的 Host run
`29817713528`、Browser run `29817713549` 也全绿，PR11 已普通 merge 为
`bda730fd55e34ea7bdf7e75bfc600da9d75709a2`。

最终 fixed openvela run：
<https://github.com/918154429/smart-band-demo/actions/runs/29816300149>

- source：`298a5c0aa45968b341739c3d1c5a3c103f84e2eb`
- artifact：`openvela-fixed-release-29816300149`，`23,861,899` bytes
- digest：`sha256:554fac66d9165ad48ae765a25adef855527eb762c78a8c3705076bcf97a11823`
- NuttX：`66,114,392` bytes，SHA-256
  `61aa4877b597bd956e2ab4b34a2dcc913940979362279ffcf9c94f13a39ec051`
- compact `336x480` mask 外 91.13% 像素零差异。
- framed `1280x800` mask 外 96.64% 像素零差异；第一次横滑进入 `heart_rate`，
  `104 bpm`/`Source / Sensor` 与 cleanup 全通过。

完整说明与结构化结论：

- `docs/w1-first-wave-integration-20260721.md`
- `docs/evidence/w1-gate-summary-20260721.json`
- `docs/evidence/w1-native-journey-20260721.json`

失败 run `29812790986`、`29814087721`、`29815208311` 及对应 PNG/JSON 均保留。

## 5. 证据边界

- host memory/file fault model 不是真实掉电介质，不证明目标板文件系统原子性、目录项持久化
  或 power-loss durability。
- 最终 openvela `.config` 的 `CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH=""`，因此 native
  smoke 证明生产源码编译链接和默认 no-op 启动，不证明非空路径的写入/重启恢复。
- storage load 在 UI tree 创建前同步执行；已证明 returned error 不会中止初始化，但没有
  证明慢或永久阻塞 backend 的时延隔离。backend 必须有界、可响应。
- degraded 状态在 store 中可观测，本切片未添加用户提示或运行日志遥测。
- Q2 Lotus/Activity/Minimal 与 picker 的 compact/framed framebuffer 已证明；Q3 已完成
  5 分钟 warmup + 30 分钟定向 soak。全旅程 2 小时 RC、模拟器/真机 8 小时压力、真机
  24 小时待机、其他功能页面和所有真机能力仍未证明。
- Gemini-S1 的公开认证和 defconfig 不是本项目真机证据；板载 2.8 寸显示方向/分辨率、
  触摸、存储、传感器、BLE、恢复与功耗均保持未证明。

## 6. Q2 A Gate 已完成

1. Lotus、Activity、Minimal 共用 registry、lazy lifecycle 与单个 256-byte context。
2. 600ms 长按 picker、手势仲裁、100 次切换与非空 file backend fd soak 全过。
3. selected face 复用 Q1-S A/B store，覆盖损坏、旧/未来版本和写/flush 失败。
4. Host `29824971500`、Browser `29824971555`、fixed openvela `29825089126` 全绿。
5. 完整证据见 `docs/q2-multi-watch-faces-20260721.md` 与
   `docs/evidence/q2-gate-summary-20260721.json`。

## 7. 历史计划：Q3 Workout + History（已完成）

本节是进入 Q3 前的计划快照，已被第 11 节最终证据覆盖，不再是续接入口。

先接现有 step normalizer/Walk/Run pure core 到后台 service，再实现 Workout UI、checkpoint、
daily record 与 7 天/30 天历史聚合。必须复用 Q1-S store 和已冻结 W1 model，不另写第二套
步数归一化或 workout 状态机。Q4-Q6 继续保持 pure core ready，不提前交叉接线。

Gemini-S1 不改变 Q3 软件主线。若并行启动板卡工作，到货前仅执行独立的 G0
compile/link-only；到货后从 HW0 只读清点开始，烧录仍需当次明确授权。

当时的下一会话开场命令如下，仅作历史留档，不得在当前状态重复执行：

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw FINALS_TOP_TIER_ROADMAP.md
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short
git log -3 --oneline
gh pr list --state open
```

开始实现前仍须读取用户最新指令；如有冲突，以最新指令为准。

## 8. 2026-07-21 Q3 未提交工作区交接（历史，已失效）

本节记录 2026-07-21 的阶段性现场，已被第 11 节完整覆盖。以下“尚未提交、尚未运行、
红色 Gate”和所有动作清单均只描述当时状态，不得作为当前判断或命令来源。

### 8.1 当前 Git 状态与并行改动边界

- 分支：`codex/q3-workout-history`
- 基线：`420d1b9`（`origin/master` / `master`）
- 工作树有大量 tracked/untracked Q3 改动，禁止 reset、checkout 或清理。
- `FINALS_TOP_TIER_ROADMAP.md`、`README.md`、本 handoff、
  `docs/implementation-notes.md`、`docs/openvela-runbook.md` 和未跟踪的
  `docs/gemini-s1-target-board.md` 同时包含另一条 Gemini-S1 板卡决策工作。
  这些改动不是 Q3 service 审计产生的，必须保留；Q3 提交时应逐文件/逐 hunk 暂存，
  不得把板卡文档误删，也不得默认把它们混入 Q3 PR。

### 8.2 已实现的 Q3 主体

- raw step counter 在 UI `%100000` 截断前保留，runtime 有 typed workout event。
- runtime 拥有 Workout/History service，使用 64-bit monotonic elapsed time。
- Workout 支持倒计时、active、暂停/恢复、结束/放弃、重启确认、summary dismiss。
- FINALIZING checkpoint + idempotent session append + EMPTY tombstone 已实现。
- daily 30 日历日窗口、最近 30 个连续 session、7 天缺失日趋势与详情已实现。
- checkpoint `0x00010000..1`；daily `0x00030000..3`；session
  `0x00040000..5`，显式 little-endian；daily/session 最大 payload 为 428/508 bytes。
- Workout/History 是两个 lazy system view，不改变原八应用 registry。
- Browser reference 已有 Workout -> History journey。

### 8.3 本轮审计后新增的 correctness 修复

- daily 由“最近 30 条”收紧为最近 30 个日历日；稀疏日期不会让 `%2` shard 超过
  15 条固定上限。
- 新 session ID 必须等于 `next_session_id`，但完全相同的旧 ID 仍可用于
  FINALIZING 幂等重放；避免 `%3` shard 偏斜越过 10 条/508 bytes。
- store load 为 RECOVERED/DEGRADED/错误代时阻止整 shard 回写，避免一次性 read fault
  后用旧快照覆盖仍可恢复的新 generation。
- 跨午夜的 steps/active/calories/heart 按 tick 墙钟区间比例拆分；daily HR 改用本 tick
  开始时的 current HR，与 workout model 的 elapsed weighting 对齐。
- RTC invalid/回拨成为 checkpoint 中的 session-level sticky anomaly；session 不再在回拨后
  错标 RTC_VALID，已有 trusted day 会标 RECOVERY_GAP/incomplete。
- Abort 在清 EMPTY checkpoint 前先 flush dirty daily。
- checkpoint 失败按 30 秒、FINALIZING 按 5 秒节流重试，不再故障时每秒写 storage。
- UI 即时命令派发前重新 sample clock；正常 runtime deinit 会 checkpoint live workout 并
  flush daily。
- summary 持久化未完成时 Done 不再关闭页面；History 的 7 天锚点不会因 RTC 回拨隐藏
  较新的已存日期。
- native app detail 高度从错误的 `94 + 452 = 546px` 改为剩余屏高；fake LVGL 断言
  Pause 控件底部不越过 480px。

### 8.4 新增但尚未实跑的 native 证据路径

- `CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS`：仅显式启用时每秒输出 bounded
  `smart_band:q3:v1` marker，包含 workout/history/store/queue/object/tick-gap 状态。
- reproduce script 新增受控环境变量：
  `SMART_BAND_STORAGE_PATH` 与 `SMART_BAND_E2E_DIAGNOSTICS=0|1`。
- 新脚本 `scripts/run_q3_native_e2e.py` 计划在同一个 staged `vela_data.bin` 上完成：
  boot1 pause/checkpoint -> boot2 recovery/finalize/history -> boot3 history reload。
- runner 可选 5 分钟 warmup + 30 分钟测量，每 10 秒采 marker；heap/fd 目前明确未证明。
- `openvela-nightly.yml` 新增手动输入 `q3_native_e2e`、`q3_soak_seconds=0|1800`；
  schedule/default run 仍保持 storage path 空、diagnostics off。
- 上述 runner/workflow **从未在 Linux/OpenVela 上运行**；触控坐标、guest `/data` 创建、
  三次 emulator 生命周期、YAML 表达式、截图和 cleanup 都仍需真实验证。
- 新脚本还是 untracked，Git executable bit 尚未写入 index；提交前必须执行
  `git add --chmod=+x scripts/run_q3_native_e2e.py` 并确认 mode `100755`。

### 8.5 当前已通过的验证

最终 CSS 调整后：

```text
npm run test:browser                         7/7 passed
python tests/test_history_service.py         passed (MSVC /W4 /WX)
python tests/test_workout_service.py         passed (MSVC /W4 /WX)
python tests/test_runtime_core.py            passed (MSVC /W4 /WX)
python tests/test_ui_compile.py              passed (diagnostic path compiled)
python tests/test_q3_native_e2e.py           5/5 passed
python -m py_compile ...                     passed
bash -n reproduce.sh                        passed
bash scripts/test_reproduce_failure.sh       passed
git diff --check                            passed
```

在本轮较早阶段，除 coverage 外的完整本地 `tests/test_*.py` host suite 也通过；之后改过
runtime/service/UI，因此下一对话仍应重新跑完整 suite。`test_core_coverage.py` 本机只能通过
source-list validation，Windows 没有 GCC；不能据此声称 coverage 通过。

### 8.6 仍阻断 B/D Gate 的问题

1. **跨对象事务未闭合**：periodic checkpoint 与 daily flush，以及 finish 的
   daily -> FINALIZING -> session -> EMPTY，是多个独立 A/B object。任意两个 object
   commit 之间 crash 仍可能得到 workout 较新/daily 较旧或部分 mixed workflow。
   必须先定义并实现 transaction/manifest/epoch 或可证明的 replay 协议，再做逐 write/
   flush/verify crash-cut matrix。
2. **UNAVAILABLE 语义仍锁存**：history/checkpoint 将 no-op backend 的 UNAVAILABLE 当作
   永久无存储以支持 browser/fake 路径；若真实 backend 暂时返回 UNAVAILABLE 后恢复，
   FINALIZING 仍可能被当作成功清掉。需要区分“永久 no-op capability”与“可恢复暂时错误”，
   并补 backend recovery test。
3. 跨超过 24 小时的单次 tick、forward jump、亚秒午夜 remainder 还没有完整测试。
4. native recovery/summary/history reviewed golden 尚未生成；Browser PNG 不能替代 native。
5. GCC、Clang、overall `>=85%`、Q3 每个新 production source `>=90%` 尚未得到 CI 结果。
6. 非空 file backend native restart、完整 storage fault matrix、5+30 分钟 soak 未运行。
7. soak 的 heap/fd 指标当前 runner 标记为 unverified；在拿到可靠 guest/host采样前 Gate
   必须保持红色。

### 8.7 远端与预览状态

- `ssh codex-2c8g` 只能从 Git Bash 正确使用 ProxyCommand；PowerShell 直接调用会把
  Unix `exec` 误交给 PowerShell。
- 内部 host 可登录，但没有 handoff 要求的 `/data`，也没有 GCC/Clang；本轮没有在
  `/home` 创建任何工作目录。不要绕过“新远端工作只进 `/data`”边界。
- 本地 browser preview 曾在 `http://127.0.0.1:4174/demo/index.html`，结束本轮时应已关闭。

### 8.8 下一对话精确步骤

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short --branch
git diff --check
python tests/test_history_service.py
python tests/test_workout_service.py
python tests/test_runtime_core.py
python tests/test_ui_compile.py
python tests/test_q3_native_e2e.py
npm run test:browser
```

随后按以下顺序推进：

1. 先解决 8.6 的跨对象 transaction 与 UNAVAILABLE 语义，补完整 crash-cut tests。
2. 审查 `run_q3_native_e2e.py` 和 `openvela-nightly.yml`，先在短 journey 上迭代；不要直接
   启动 35 分钟 soak。
3. 逐 hunk 暂存 Q3 文件，保留/隔离 Gemini-S1 并行改动；设置 runner mode `100755`。
4. 提交、push 并建 PR，让 Host/Browser/GCC/Clang/coverage 先全绿。
5. 短 native 可手动触发：

```powershell
gh workflow run openvela-nightly.yml --ref codex/q3-workout-history `
  -f build_jobs=2 -f q3_native_e2e=true -f q3_soak_seconds=0
```

6. 审计短 run 的 artifact、三次启动 marker、PNG、storage object 和 cleanup；修正后才触发
   `q3_soak_seconds=1800`。
7. 只有 transaction crash matrix、native restart/golden、coverage 和完整 soak 全绿后，
   才更新 roadmap 勾选 B/D Gate、撰写 Q3 evidence、合并 PR。

## 9. 2026-07-22 Q3 原生分页圆点遮挡修复（历史，已失效）

本节记录最终长 soak 之前的短程证据。其 `OPEN`、红色 Gate 与“不得合并”结论已被第 11 节
覆盖，不得据此回退 Q3 状态。

- 分支仍为 `codex/q3-workout-history`，PR 仍为
  [PR13](https://github.com/918154429/smart-band-demo/pull/13)。
- 提交 `cebeb02` 保存分页圆点行指针；Workout/History 系统视图打开成功时隐藏，关闭或
  mount 失败时恢复。fake LVGL smoke 覆盖 Workout、History 两条打开/关闭路径。
- 本地 `python tests/test_ui_compile.py`、`python tests/test_q3_native_e2e.py`（7/7）、
  runner `py_compile` 与 `git diff --check` 全过。
- Host run `29863367448`、Browser run `29863367483` 全绿。
- 不含 soak 的 fixed OpenVela run
  [29863478956](https://github.com/918154429/smart-band-demo/actions/runs/29863478956) 全绿；artifact
  `openvela-fixed-release-29863478956`，ZIP digest
  `sha256:4234a4ffdd8117091dece96a6be9373d82b54a29b71e62872441c72afbbd9d2e`。
- 本地审计目录：
  `C:\Users\Lenovo\AppData\Local\Temp\smart-band-openvela-29863478956`。Q3 JSON
  `status=passed`，3 boots、全部 checks 为 true；117 项 `evidence.sha256` 全部本地复算通过。
- boot1 pause/checkpoint 为 `steps=7/state=3`；boot2 从
  `state=6/recovery=1/steps=7` 恢复，结束后 `sessions=1/state=4` 并进入 History；boot3
  重载为 `sessions=1/view=2`。
- staged `vela_data.bin` SHA-256 从
  `4ba173c546357192e9d542a508951acb4e1f9e8fd93486083bd5d5c21ae134b5` 变为
  `aef2aa04121abbfdc0a2930eb6005ab0f32c00ec68bf461ce47a0a64993bd19d`；源构建产物不变，
  runtime tree 删除和三次进程/端口清理均通过。
- 7 张 selection/paused/recovery/confirmation/summary/history/history-reloaded PNG 均非空、
  PNG magic 正确并经人工复核。全局分页圆点已不再覆盖 Finish、Confirm、Done 或 History
  底部详情；History 与 history-reloaded SHA-256 相同。
- `q3_soak_seconds=0`，heap/fd 仍为 unverified。未经用户对昂贵远端批次的明确授权，不触发
  `q3_soak_seconds=1800`；B Gate、D Gate 继续保持红色，PR13 不合并。

## 10. Q3 长 soak 前续接入口（历史，已失效）

本节是获得长跑授权之前的交接快照。第 11 节已经证明 soak、heap/fd、证据复算与 PR 合并
全部完成；以下命令、禁止合并条件和红色 Gate 结论均不得继续执行。

### 10.1 一句话状态

Q3 Workout + History 的实现、事务/恢复测试、跨平台 CI、coverage、fresh OpenVela build、
非空 file backend 三启动恢复和 7 张 reviewed native PNG 均已通过；PR13 保持 OPEN/CLEAN。
当前只差经用户明确授权后的 30 分钟定向 soak，以及 soak 中仍未解决的 heap/fd 可观测性，
因此 B Gate、D Gate 仍为红色，不得合并 PR、勾选 Gate 或声称 Q3 完成。

### 10.2 仓库与远端现状

- 仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- 分支：`codex/q3-workout-history`
- HEAD / origin：`cebeb02153581a8ffd945f54783d2ed4d48b4965`
- PR：[PR13](https://github.com/918154429/smart-band-demo/pull/13)，状态 `OPEN`、
  `mergeStateStatus=CLEAN`
- 最新 PR checks：Host `29863367448`、Browser `29863367483`，GCC、Clang、MSVC、Chromium、
  production coverage、UI compile 和 evidence harness 全部 `SUCCESS`
- 最新短程 fixed OpenVela：
  [29863478956](https://github.com/918154429/smart-band-demo/actions/runs/29863478956)，
  `headSha=cebeb02153581a8ffd945f54783d2ed4d48b4965`、`conclusion=success`
- artifact：`openvela-fixed-release-29863478956`，ZIP digest
  `sha256:4234a4ffdd8117091dece96a6be9373d82b54a29b71e62872441c72afbbd9d2e`
- 已下载审计目录：
  `C:\Users\Lenovo\AppData\Local\Temp\smart-band-openvela-29863478956`

### 10.3 已确认的短程原生证据

- `q3-native/q3-native-journey.json` 为 `status=passed`，3 boots、全部 checks 为 true。
- boot1：暂停并落 checkpoint，`state=3/steps=7`；boot2：以
  `state=6/recovery=1/steps=7` 恢复，完成后 `sessions=1/state=4` 并进入 History；boot3：
  `sessions=1/view=2`，证明 History 重启重载。
- staged `vela_data.bin` SHA-256 从
  `4ba173c546357192e9d542a508951acb4e1f9e8fd93486083bd5d5c21ae134b5` 变为
  `aef2aa04121abbfdc0a2930eb6005ab0f32c00ec68bf461ce47a0a64993bd19d`。
- `evidence.sha256` 共 117 项，已在本机全部复算一致；源 build output 未被修改，三次
  emulator/端口清理和 runtime tree removal 全过。
- selection、paused、recovery、finish-confirmation、summary、history、history-reloaded
  七张 PNG 非空、magic 正确并完成人工视觉复核；分页圆点不再覆盖 Finish、Confirm、Done
  或 History 底部详情。history 与 history-reloaded 哈希相同。
- 该 run 明确为 `q3_soak_seconds=0`，不能替代 30 分钟 soak；heap/fd 仍为 unverified。

### 10.4 工作树保护边界

下列内容是用户保留的未提交文档/板卡并行工作，禁止 reset、checkout、clean 或默认混入
Q3 提交：

```text
M  FINALS_TOP_TIER_ROADMAP.md
M  NEXT_SESSION_HANDOFF.md
M  README.md
M  docs/implementation-notes.md
M  docs/openvela-runbook.md
?? docs/gemini-s1-target-board.md
```

`NEXT_SESSION_HANDOFF.md` 本身包含本节交接更新，仍故意保持未提交。Q3 代码提交已推送，
当前没有待提交的 Q3 源码或测试文件。

### 10.5 下一对话开场命令

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw FINALS_TOP_TIER_ROADMAP.md
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short --branch
git log -3 --oneline --decorate
gh pr view 13 --json url,state,headRefOid,mergeStateStatus,statusCheckRollup
gh run view 29863478956 --json status,conclusion,url,headSha
```

先读取用户在新对话中的最新指令。若用户没有明确授权昂贵远端长批次，只能核对状态、解释
边界或继续改善 heap/fd 采样方案，**不得**自行触发 1800 秒 soak。

### 10.6 获得明确授权后的唯一远端动作

仅当用户明确同意运行 30 分钟 Q3 soak 后，触发：

```powershell
gh workflow run openvela-nightly.yml --ref codex/q3-workout-history `
  -f build_jobs=2 -f q3_native_e2e=true -f q3_soak_seconds=1800
```

触发后必须持续监控到终态，并下载/审计 artifact，至少核对：

1. fresh build、link、compact golden、通用 native 和 Q3 三启动仍全绿；
2. warmup/measurement 时长、marker 采样间隔、最大 tick gap、事件队列丢弃/驱逐/合并；
3. 非空 file backend 的 storage objects、三次启动恢复、源产物不变与 runtime 隔离；
4. 三次 emulator 进程组、端口和 runtime tree 清理；
5. `evidence.sha256` 全量本地复算与全部 PNG 人工复核；
6. heap/fd 是否仍为 unverified。若仍无法可靠采样，必须保持对应 Gate 红色，不得用进程存活
   或 object count 冒充 heap/fd 证据。

### 10.7 禁止事项与完成条件

- 未经明确授权：不跑 1800 秒 soak，不烧录硬件，不 force-push，不改写历史，不删除远端。
- soak 或 heap/fd 任一不满足：不合并 PR13，不勾选 B/D Gate，不发布正式 release。
- 即使长 soak 成功，也要先把 run ID、artifact digest、结构化结果、哈希复算、PNG 视觉结论、
  heap/fd 结论和剩余边界写回本 handoff/正式 Q3 evidence，再决定 Gate 与合并。

## 11. 2026-07-22 Q3 最终 Gate 闭环

### 11.1 一句话状态

Q3 Workout + History 的 B/D 软件 Gate 已全绿并普通合并：PR13 head `08b6fa6` 的
Host/Browser/coverage 全部通过，最终 fixed OpenVela run `29888365756` 完成三启动、非空
file backend、5 分钟 warmup + 30 分钟测量、180 组 heap/fd 样本、1437 项哈希复算和
七张 native PNG 人工复核。
下一主线切片进入 Q4 C 消息提醒闭环；不得回头重复 Q3，除非出现新的回归证据。

### 11.2 最终证据

- PR13 head：`08b6fa64f1eaace9912cffdf3799c35a900c094e`；PR13 已于
  `2026-07-22T04:21:45Z` 普通 merge，merge commit
  `4f79ae04d5097effa294eb8dea776222cbb02d40`，远端 `origin/master` 已指向该提交。
- Host `29887461218`、Browser `29887461190` 全绿；overall coverage
  `92.8% (4304/4640)`，transaction `94.0%`、history `90.3%`、workout `90.1%`。
- 最终 run：<https://github.com/918154429/smart-band-demo/actions/runs/29888365756>，
  `48m41s`，artifact `24,965,512` bytes，digest
  `sha256:5adc9b82cb3019f6c8317cffeb3159c6adeeaed667afe3205aa2653165ff1d9e`。
- 180 marker + 180 resource 样本；间隔 `10000..11036ms`，最大 tick gap `1146ms`，对象恒定
  `220`，queue/drop/evict/coalesce/inbox drop 最大值均为 `0`。
- PID/成员集合恒为 `13`；应用 heap `404800 -> 404800`，high-water `406976`；fd
  `18 -> 18`，high-water `18`；全 guest heap `5275152 -> 5275152`。
- staged `vela_data.bin` 从 `c9cc1477...f26e7a` 变为 `de8654ce...22bc92`，源产物不变；
  三次 emulator/端口/runtime tree 清理全过。
- `evidence.sha256` 1437 项本机全量复算零失败；七张 PNG 均人工复核通过，History 与重载图
  SHA-256 同为 `977c45bfe1a439246d8772c9811463edeb5d3b9fa9d14c66802ec87a3b83fa95`。
- 正式文档：`docs/q3-workout-history-20260722.md`；结构化结论：
  `docs/evidence/q3-gate-summary-20260722.json`。

### 11.3 保留边界

- Q3 软件 Gate 通过不等于 Gemini-S1 真机 Gate；未证明真实 flash 掉电 durability、功耗、
  真实传感器、BLE、震动或 8/24 小时真机稳定性。
- `/proc/<tid>/heap` 是固定 NuttX FLAT build 的 allocation-record PID 归因，不是 Linux RSS。
- 用户保留的 README、implementation notes、runbook 与 Gemini-S1 文档改动仍不得 reset/clean；
  后续提交需继续按文件/逐 hunk 隔离。

### 11.4 下一对话精确起点

1. 当前工作树仍在 `codex/q3-workout-history`，并保留用户文档/Gemini-S1 脏改动；不要为切到
   `master` 执行 reset/checkout/clean。新 Q4 分支必须从 `origin/master@4f79ae0` 创建隔离
   worktree。
2. 读取路线图 Q4，仅推进 C 消息提醒：notification center、overlay、call、haptic log、DND
   与输入隔离；复用 W1 notification pure core，不接 power/BLE transport。
3. 首个 Q4 切片应先完成 notification service + deterministic injector + host fault/pressure
   tests，再接 LVGL overlay。

开场命令：

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw FINALS_TOP_TIER_ROADMAP.md
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short --branch
git fetch origin master
git rev-parse origin/master
git show --no-patch --oneline 4f79ae04d5097effa294eb8dea776222cbb02d40

$q4Worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo-q4-notifications'
if (Test-Path -LiteralPath $q4Worktree) {
  throw "Q4 worktree path already exists: $q4Worktree"
}
git worktree add -b codex/q4-notifications $q4Worktree 4f79ae04d5097effa294eb8dea776222cbb02d40
Set-Location $q4Worktree
git status --short --branch
```

若 `codex/q4-notifications` 已存在，不得删除或覆盖；先用 `git worktree list`、
`git branch --list codex/q4-notifications` 查明现场，再选择新的明确分支名和空目录。

### 11.5 Q4 第一可验证切片

只推进 service 层，不先做 LVGL：

1. 复用 `notification_model.c` 的固定容量、重复 ID 幂等、来电保护、action、DND 与 workout
   presentation decision；不得另写第二套 notification model。
2. 新增 runtime-owned notification service，并让 deterministic injector 只通过统一 ingress/
   event 路径送入；外部回调不得直接操作 LVGL。
3. host tests 至少覆盖 1000 条混合注入、队列满载、重复 ID、超长文本、Accept/Reject、DND、
   workout 前台策略、事件队列压力和创建/处理失败后的可恢复性。
4. MSVC 严格警告和现有 host suite 先全绿；随后用 GCC/Clang/coverage 独立复核，新增 production
   source 单文件覆盖率目标 `>=90%`、overall 不低于当前门禁。
5. service/injector/fault-pressure 证据闭合后，下一切片才接 notification center 与普通 overlay；
   来电全屏层、haptic log 和 workout 真实集成继续按 Q4 顺序推进。

### 11.6 禁止事项

- 没有新的回归证据，不重复 PR13、`29888365756` 或其他 Q3 build/native/soak。
- 不提前进入 Q5 power、Q6 sync/BLE transport 或 Q8 全功能 RC；Q4 只定义 synthetic
  `wake_request` contract，不实现真实低功耗唤醒。
- 不烧录 Gemini-S1，不写 flash/bootloader，不把模拟器 haptic log 声称为真实震动证据。
- 不 reset、checkout、clean 或覆盖当前工作树中的 README、implementation notes、runbook、
  roadmap、handoff 和 `docs/gemini-s1-target-board.md` 改动。
- 不 force-push、不改写历史、不删除远端分支/标签、不发布正式 release；如与用户新指令冲突，
  以新指令为准。

## 12. 2026-07-23 Q4/Q5/Q6 软件并行续接状态

### 12.1 分支与 PR 拓扑

- `origin/master`：`3e1aa5819f3818e1e06a2e2d4749d72dbd8dc1ea`，PR15 普通 merge 后的
  Q3 最终文档基线。
- Q4 worktree：`smart-band-demo-q4-notifications`，分支 `codex/q4-notifications`，
  [PR14](https://github.com/918154429/smart-band-demo/pull/14)，以 `master` 为 base。
- Q5 worktree：`smart-band-demo-q5-power`，分支 `codex/q5-power`，
  [PR17](https://github.com/918154429/smart-band-demo/pull/17)，以 Q4 分支为 stacked base。
- Q6 worktree：`smart-band-demo-q6-sync`，分支 `codex/q6-sync`，
  [PR16](https://github.com/918154429/smart-band-demo/pull/16)，以 Q4 分支为 stacked base。
- 三条分支均使用普通提交/普通 merge；没有 force-push、历史改写或远端分支删除。

### 12.2 Q4 当前结论

Q4 已完成 application event mutex、50 ms event pump、Notification Center、普通 overlay、
全屏 Call、workout non-blocking Call、输入捕获、platform haptic adapter、BUSY/IO retry、
UNAVAILABLE structured fallback、logger drop 统计，以及 DND/长文本/same-ID UI journey。
notification effect 仍以 service generation 为 ACK key；Q4 standalone 的 wake marker 明确是
`synthetic=1 power_transition=0`。

- 功能修复：`9a5ab901a8e15e81cf61e8de98ba7a888687f592`。
- master 集成：`f6185d402b9847210e69e228ce51e95205ce1c3b`。
- 已验证 Host run `29972675675` 55/55、Browser run `29972675670` 1/1。
- 证据：`docs/q4-notification-effects-20260723.md` 与对应 JSON。

Q4 explicit-length UTF-8 ingress 与 cross-inbox/main total ordering 已在后续切片闭合：
严格 UTF-8/embedded-NUL 校验、完整码点前缀截断、共享 64-bit sequence、wrap 与同优先级
满载保序均已覆盖；外层开发机 GCC coverage 为 overall `93.0%`，event queue `97.5%`、
inbox `98.9%`、notification service `94.9%`。证据见
`docs/q4-notification-ingress-order-20260723.md` 与对应 JSON。
修复提交为 `cc0be669d859908b1beb672aaebbf6961d73f640`；最终 Host run
`29975646888` 与 Browser run `29975646864` 全绿。

Q4 C Gate 仍保持 open。剩余项仅为 reviewed native notification journey，以及 target
ELF/真实 linker map/task stack 证据。不得把 fake-LVGL marker 声称为真实震动、真实显示
唤醒或真机能力。

### 12.3 Q5 当前结论

Q5 软件策略切片已经接入 runtime-owned power manager：ACTIVE/DIMMED/SCREEN_OFF、typed
wake、同 pump bitmask、render/heart due、sensor sampling mask、checkpoint/sync gate、
display/backlight/sleep adapter 与 lifecycle restore。notification wake 的唯一 owner 是
runtime power consumer，流程固定为 `peek_wake -> 合并 power events -> policy accepted ->
ack_wake`；application 只消费 haptic，并观察已完成的 power wake 生成 marker，不再二次 ACK。

- 功能提交：`c49929b18cc21b88cc3ababc516a63fbe944193e`。
- Q4 集成提交：`9ba5e9069a794eca5b7ad6a14e141c46a6b8492e`。
- 已验证 Host run `29973330134` 58/58、Browser run `29973330198` 1/1。
- 证据：`docs/q5-runtime-power-20260723.md` 与对应 JSON。

本切片证明主机侧软件策略和调度，不证明 LVGL 实际帧耗、MCU sleep residency、目标板电流、
transition energy 或 battery life。真实功耗 Gate E 保持红色。

### 12.4 Q6 当前结论

Q6 阶段切片已经完成 v1 daily-history capabilities/request/data/ACK、stop-and-wait cursor、
early-ACK 拒绝、首包 total 锁定、ACK 编码失败不提交、不可变 30-record transaction snapshot，
以及 drop/duplicate/reorder/delay/disconnect loopback fault。stop/disconnect 会清 queue 和 held
reorder；golden vectors 覆盖 request/data/ACK 和完整 little-endian daily record。

- 功能提交：`8c52e7bd552e62f8b54c790f81157b2765ee915b`。
- Q4 集成提交：`f0f227936dda48a221f14c0ac619ba05dbc39032`。
- 已验证 Host run `29972934272` 58/58、Browser run `29972934359` 1/1。
- 证据：`docs/q6-history-sync-loopback-20260723.md` 与对应 JSON。

Q6 总 Gate 未完成。仍缺 automatic timeout/retry driver、live metrics、workout session history、
device config、Notification Inbox、Linux client、simulator bridge、GATT mapping 与真实 BLE。

### 12.5 下一续接顺序与边界

1. 先确认 PR14/PR16/PR17 最新文档 head 的 Host/Browser checks 全绿且工作树 clean。
2. Q4 继续关闭 12.2 剩余的 native reviewed journey 与 target ELF/map/stack 证据；未闭合前
   不把 C Gate 标绿。
3. Q6 下一软件切片优先 automatic timeout/retry driver 和可观测 metrics，再扩业务 domain；
   所有同步调度必须咨询 Q5 的 `smart_band_runtime_allows_sync()`。
4. Q5 只可继续改善 host/native 软件证据；没有目标板测量时实际功耗 Gate 始终红色。
5. 不烧录 Gemini-S1、不写 flash/bootloader、不声明真实 BLE/震动/功耗，不 force-push、不
   删除远端分支、不发布 release。硬件动作仍需逐次明确授权。

## 13. 2026-07-23 Q4 native Gate harness 待实跑状态

### 13.1 当前实现

Q4 C Gate 最后两项的采证路径已经实现，但尚未完成 fresh OpenVela run：

- diagnostics-only `--q4-native-scenario=<ordinary|center|calls|workout>`；
- ordinary 701 initial + 2.5 秒 same-ID 长 UTF-8 update；
- DND Center IDs 711–715、Mark read、Delete；
- Alice 721 Accept、Bob 722 Reject 与全屏输入隔离；
- Workout ACTIVE 后 Coach Call 731、non-blocking overlay、Pause、Reject；
- 四个隔离 emulator boot，严格 Q3/Q4/inject/haptic/wake marker；
- 每个 checkpoint 的 NSH `ps` stack coloration，逐样本强制最差路径余量 `>=25%`；
- PNG、transcript、run-id/runtime attributed cleanup 与根/嵌套哈希 manifest；
- `vela_ap.elf`、`nuttx.map`、`System.map`、SHA、section/readelf、symbols/map 行采集。

所有场景只走 production explicit-length external ingress。正常无参数启动零副作用；
simulator haptic 与 wake marker 继续明确为 simulated/synthetic，不代表真实震动或显示唤醒。
详细契约见 `docs/q4-native-gate-harness-20260723.md`。

### 13.2 已通过、尚未通过

已通过：

- 本地 Host-equivalent 22 个脚本；
- `tests/test_q4_native_e2e.py` 13/13；
- MSVC `/W4 /WX` production UI；
- Browser/Chromium 7/7；
- 外层 `ubuntu24-hushen` 的 GCC strict UI、notification service、central runtime 和
  Q4 runner 当时快照 12/12，目录 `/data/smart-band-q4-native-20260723T1150CST`；
- C/runner 独立静态审计完成；发现的 stack threshold、兜底清理与相对哈希三项证据漏洞
  已修复，最新已暂存快照的只读复审无阻断项。

尚未通过：

- 当前代码的 PR14 Host/Browser CI；
- fresh fixed OpenVela build 与四场景真实 emulator journey；
- native PNG 人工复核；
- artifact manifest 全量复算；
- target ELF/linker map/task stack 数值审计。

因此 Q4 C Gate 仍为 **open**，路线图勾选保持 `[ ]`。

### 13.3 精确续接动作

1. 普通提交并推送当前实现，等待 PR14 Host/Browser 全绿。
2. 触发：

```powershell
gh workflow run openvela-nightly.yml `
  --ref codex/q4-notifications `
  -f build_jobs=4 `
  -f q3_native_e2e=false `
  -f q3_soak_seconds=0 `
  -f q4_native_e2e=true
```

3. 监控到终态，下载 artifact，复算全部 `evidence.sha256`。
4. 人工逐张复核所有 native PNG，尤其确认 ordinary initial/update 标题不同。
5. 审计四场景 JSON/checks/cleanup、ELF/map/SHA/symbols 与 stack high-water/minimum margin。
6. 只有全部通过后，才写正式 Q4 C Gate evidence、更新路线图并决定 PR14 合并。

### 13.4 远端拓扑与硬件边界

- 默认开发远端是外层 `ubuntu24-hushen`；新工作只留在允许的 `/data` 路径。
- `codex-2c8g` 是两核 Codex/共享 Sub2API 中转子机，不是默认开发机。
- RK3568 是独立硬件子机；当前 Goldfish native Gate 不需要触达它。
- 单独执行 `test -d /data` 只能探测当前登录主机，不能判定整体拓扑。
- 未经逐次明确授权，不烧录、不写 flash、不修改 bootloader。
