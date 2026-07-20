# smart-band-demo 下一会话交接

更新时间：2026-07-21（Asia/Shanghai）

> 复赛工程以根目录 `FINALS_TOP_TIER_ROADMAP.md` 为主路线。Q0、Q1-V、Q1-C 已完成；
> 下一独立切片只做 Q1-S versioned storage/fault backend，不得提前实现 A-G 功能。

## 1. 仓库、权限与边界

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub：<https://github.com/918154429/smart-band-demo>
- 默认分支：`master`
- 本文件写入时的提交基线：`28bd52e`；下一会话以实际 `git log -1` 为准。
- 用户确认其对主仓库拥有完整管理权并承担责任，长期授权 Codex 按持续开发需要修改、
  提交、推送、创建/更新/合并 PR。强制推送、历史改写、删除远端分支/标签和正式 release
  仍需当次明确指示。
- 当前会话允许子智能体，最多五个，且最多二级委派；该额度和指令不自动延续到新会话，
  以用户最新指令为准。
- 视频、比赛平台提交和正式发布不属于工程路线。
- 开发板型号/revision 未确认；未经逐次明确批准不得烧录、写 flash 或更改 bootloader。

若使用远端 Ubuntu，所有新工作必须留在 `/data`。不得访问 `/data/codex-audit`、
`/data/lxd-storage`、`/data/lost+found` 或旧 `/home/ubuntu/...` 项目树。

## 2. 已合并历史

PR1-PR8 已闭环生命周期、传感器来源、应用 runtime、响应式 UI、固定复现、真实
openvela build/native smoke 和独立 simulator 证据。最近既有合并基线为：

| PR | Merge commit | 内容 |
| --- | --- | --- |
| PR7 | `a41d076c3822165a69db1e48ad2b32c86a3fb09e` | 固定 emulator、NSH/native smoke、清理 |
| PR8 | `1ddc533cb3096e541fe5cadaed810df39029ebc1` | 独立 simulator 复跑与 artifact 证据 |

路线图/远端 Q1-C 执行计划提交为 `2afca84`、`28bd52e`。Q0/Q1-V/Q1-C 的实际 PR 和最终
合并提交若未在本文件中记录，应从 `gh pr list --state merged` 与 `git log` 获取。

## 3. Q0 与 Q1-V 基线

Q0 正式证据目录：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q0-final-audited-20260720T2034CST
```

- 20/20 独立冷启动通过；`smart_band -> UI ready` p50 `0.8065s`、p95 `0.821s`、
  max `0.828s`。
- NuttX `65,913,280` bytes，SHA-256
  `5f97a280c2478ab94116be111fecef63cd103ce0612ef14e5513933218091d58`。
- 每轮输入隔离、run-id、进程/端口/暂存清理、artifact 前后不变和 evidence manifest
  全部通过。

Q1-V 正式证据目录：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q1v-final-audited-20260720T2031CST
```

- native framebuffer 为 `1280x800` RGBA8；第一次 swipe 进入 Heart Rate。
- 注入心率 `104` 后，`Heart Rate`、`104 bpm`、`Source / Sensor` 精确 golden ROI 匹配。
- 结构化状态为 `page=heart_rate, value_bpm=104, source=sensor, freshness=fresh`。
- 只证明 fresh 帧，不证明停止上报后的 stale/TTL fallback。

本地紧凑证据与说明见 `docs/q0-q1v-baseline-20260720.md` 和 `docs/evidence/`。

## 4. Q1-C 已完成

### 架构与行为

- `smart_band_runtime_t` 统一持有 model、sensor bridge、app registry、event queue/inbox、
  clock、capabilities 和 platform adapters；周期顺序固定为 model -> sensor -> apps。
- 主事件队列容量 16、无堆分配，支持 metrics 合并、优先级、同级 FIFO 和关键事件
  淘汰最旧低优先级事件。
- 外部 callback 只向带锁定长 inbox 投递；UI tick 排入主队列。runtime deinit 先停 sync
  transport，再关闭 inbox。
- 32-bit monotonic clock 可注入且支持 wrap；wall clock 可检测回拨。RTC 启动时无效不会
  永久禁用能力，后续恢复后墙钟可重新有效。
- dirty flags 实现 page-specific render，不再每秒无差别刷新所有页面。
- storage、power、haptic、sync 接口均可注入；默认 explicit no-op。sync loopback 固定
  `8 x 64` bytes，无堆分配。
- fake LVGL 已支持对象树、递归清理、文本、flag、event、timer、虚拟 tick/wrap、live
  counters 和第 N 次 object/timer 创建失败注入。
- 没有加入 Q1-S codec/schema/CRC/A-B slot，也没有新增 A-G 用户功能。

### 最终远端验证

独立 run：

```text
/data/smart-band-q1c-20260720T223937CST
```

- 最终源码快照：`source-q1c-final`
- 快照 SHA-256：`c2d7fa8757761c3fabbe29e9b7e2f5e37aa381789f90633144a8f44254ee26c3`
- GCC/gcov `13.3.0` + `gcovr==8.6` 总覆盖率：`90.9%`，`1388/1527`。
- 新生产源覆盖率：event queue `93.2%`、event inbox `100%`、clock `100%`、
  capabilities `100%`、runtime `96.6%`、platform no-op `100%`、sync loopback `100%`。
- compact/framed 主 UI 和全部 8 个 lazy app 的每一个创建调用失败点均验证清理和重试。
- 1000 次 create/navigation/app mount/tick/back/destroy 后 object/event/timer 零净增长。
- 214 个 openvela 项目全部固定 SHA；fresh 和 incremental build 均通过。
- manifest `67df2c52308f2579ac50d0cd7413e7f0e092b83a`
- `.claude` `ab5f8be8225ce25c2f808fae0085dbf2db8fadf4`
- emulator `be9cdef6709c2a7aed547c3029d8872c58e5f3f9`
- emulator tools `37f5024f1d9157b9778d0d9e739ee0fa68743d42`

最终 artifact：

| 文件 | SHA-256 |
| --- | --- |
| NuttX (`65,911,432` bytes) | `88d3242eb9605eff3891d5ae215b3ffede4f0f0c80276fa605e890b06770c912` |
| `.config` | `54c2e65469974193053d057412671054e106c26334c750241d3946f73db13eb0` |
| `vela_system.bin` | `77ace50ffd23ec79e68e258251bf6bd42697676c0789d5900b772aef7cf2c4bf` |
| `vela_data.bin` | `8832ff31b9c4ede07a8c2277296c5348e3f37c064404f429aee2552d9874ab85` |

Native E2E：

```text
/data/smart-band-q1c-20260720T223937CST/evidence/q1c-native-final
```

- `UI ready`：`0.780311s`。
- 第一次 swipe 到 Heart Rate；`104 bpm` 与 `Source / Sensor` 精确 golden 匹配。
- 应用、emulator/QEMU、端口 5700 和 runtime output 清理全部通过。

本机最终矩阵：六组 MSVC `/W4 /WX` 生产 C/UI 门禁；emulator `4 passed + 1 skip`、
Q0 `14 + 1`、native `13 + 1`，三个 skip 均为 Windows POSIX 预期跳过；Browser `6/6`；
shell syntax/rollback 通过。

紧凑证据：

- `docs/q1c-runtime-platform-20260720.md`
- `docs/evidence/q1c-gate-summary-20260720.json`
- `docs/evidence/q1c-native-journey-20260720.json`
- `docs/evidence/q1c-native-heart-sensor-20260720.png`

## 5. 尚未证明

- Q1-S 的版本化持久化、CRC、A/B slot、migration 和 storage fault recovery。
- native sensor stale/TTL fallback。
- 全页面/全部应用 native 像素与交互、第二分辨率和长时 native soak。
- 两小时/八小时最终长稳。
- 真机显示、触摸、RTC、存储、传感器、震动、BLE 与功耗。

Browser 仍只做设计参考，不能替代 native 或真机证据。

## 6. 下一独立切片：Q1-S only

目标：versioned storage codec 与 fault backend。建议顺序：

1. 冻结 header、record type、schema version、CRC、generation 和 golden vectors。
2. 实现 memory backend 与可测试 file backend，保持现有 `smart_band_storage_t` 接口。
3. 采用 A/B slot；若使用 rename，必须先证明目标文件系统的原子语义。
4. 覆盖短写、EIO、ENOSPC、EROFS、截断、CRC、旧版本、写中断和双槽损坏。
5. 验证只读旧完整代或新完整代；单槽损坏回退，双槽损坏降级默认，失败不阻塞 UI。
6. 新增纯逻辑生产代码每个源文件覆盖率 `>=90%`，再做 fake LVGL、Host、openvela 和
   所需 native 回归。

明确不做：表盘、Workout、History UI、通知、power policy、协议/BLE 或其他 A-G 功能。
host memory/file backend 结果不得误报为真机文件系统原子性证据。

下一会话开场：

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw FINALS_TOP_TIER_ROADMAP.md
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short
git log -1 --oneline
gh pr list --state open
```

开始实现前仍须读取用户最新指令；如有冲突，以最新指令为准。
