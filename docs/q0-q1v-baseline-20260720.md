# Q0 / Q1-V native 基线与首条垂直旅程

- 日期：2026-07-20（Asia/Shanghai）
- 本地基线：`master` / `1ddc533cb3096e541fe5cadaed810df39029ebc1`
- 固定 native artifact 来源：`82c066cce36b5b05f5a8e90d3093aaaefbb1e04f`
- NuttX SHA-256：`5f97a280c2478ab94116be111fecef63cd103ce0612ef14e5513933218091d58`

## 1. 范围、来源与审计修正

本轮只关闭路线图 Q0 与 Q1-V，不修改产品 C 功能。`82c066c` 与当前 `master` 的
`openvela_app/smart_band` Git tree 均为 `1b1d3d2...bed1830`。本地产品工作树无改动。

远端旧源码仍停在 `b75cbb0`，且产品树 manifest SHA-256 为 `aa7821...a246f`，与当前
基线 `9a3499...cfdf` 不同。因此没有 pull、覆盖或拿旧树充当当前源码；正式资源采集只读
独立 source snapshot，并由 46 文件逐项 SHA manifest 与 gate receipt 绑定。
最终工具与 source snapshot 位于
`/home/ubuntu/smart-band-sim-20260720-v1/tools/audit-final-20260720T2025CST`。

最终审计发现首轮 harness 存在可误绿路径：Q0 复用了可写数据盘，Q1-V 只判断 ROI
发生变化且部分输入使用 hardlink。旧 evidence 全部保留，但下文只把最终审计重跑
作为最终 Gate 依据。当前脚本 SHA-256：

```text
collect_q0_baseline.py   3839647d7163fb3eb5389b91d79732fadfd4ba2dfb205dadb655cd994eec4181
run_native_e2e.py        9f3795928d1018154136c34014a1d161720104058198ea915865719bb9c433d2
smoke_openvela_emulator.py eff60209d49f8e4c988f22a7e80deea8ced3d59ed3b305e4f7740a70c979811a
```

## 2. Q0 最终结果

正式证据目录：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q0-final-audited-20260720T2034CST
```

| 指标 | 结果 |
| --- | --- |
| 冷启动 | `20/20` 通过，失败 `0` |
| `smart_band` 到 `UI ready` | p50 `0.8065 s`，p95 `0.821 s`，max `0.828 s` |
| emulator 到 NSH | p50 `1.312 s`，p95 `1.364 s`，max `1.406 s` |
| NuttX ELF | `65,913,280` bytes |
| smart-band 匹配符号 | `546` 个，符号大小求和 `303,999` bytes |
| 静态资源清单 | `14` 个文件，源码侧合计 `2,027,043` bytes |
| 远端磁盘 | 最终检查可用 `26,559,209,472` bytes（约 `24.7 GiB`） |

正式模式硬性要求不少于 20 次且 p95 预算不得放宽到 2 秒以上。每轮都从固定 output
独立复制 `.config`、`nuttx`、`vela_system.bin` 和 `vela_data.bin`；每轮结束验证源 SHA、
smoke cleanup 日志、记录 PGID、console 端口和暂存输入清理。20 轮全部满足，批次结束
再次验证四个固定输入，`vela_data.bin` 仍为 `051b8406...f8e2b58`。20 个 run-id 全部
唯一，逐轮与批次级 cleanup/integrity checks 全真。

Gate receipt 绑定了 `master@1ddc533` 的
[Host run `29730739442`](https://github.com/918154429/smart-band-demo/actions/runs/29730739442)、
[Browser run `29730739529`](https://github.com/918154429/smart-band-demo/actions/runs/29730739529)、
Shell、固定构建/native smoke、源码树等价性与已归因工作树。GitHub fixed build run 为
`29725945155`，artifact ID `8454484599`；本地又重跑了 MSVC `/W4 /WX` 五组生产 C
门禁、Browser `6/6` 和 Shell rollback。Q0 机器 required checks `7/7` 全真，543 个
原始 evidence 文件逐项复算 SHA 无失败。

保留的 Actions artifact 不含 linker 生成的 `nuttx.map`。本次明确使用 ELF section、
`size -A` 和 `nm -a -S --size-sort`，并要求 `smart_band_main` 与
`smart_band_icon_heart_map` 锚点；`map_evidence` 记录为 `elf-symbol-map`，不误报 linker map。

关键哈希：

```text
q0-baseline.json             0ab92d0bc27a8663ffbbb582d52b001187b8d9bcbcb8c53a7ee4092863ff8a83
startup-summary.json         689dcb232f7653d1bf826c90d0c1c837b3839527481521f186c1e820db62526d
resource-snapshot.json       911ea5947546684934d769d64862793f9c2e9d05d71216736a4cbb4de867988d
fixed-output-integrity.json  994ef6f1b48ccbb3ed0e3dcc3a0f84f7308703b8ec3a2384d88ebeef89d0a597
gate-receipt.json            1de30bf417ba0fcbb68005bdf684288034ba47b113bdf7a39a800a0c039200e2
smart-band-tree.json         b750999ad0e06e5b95dae794c44d08db2becde8faf1be334aa52d59ac7a3e630
evidence.sha256              6c1b93b297e04437daf1f3f816ba138d6cef1e5c5d8383c62eaf14c0c144e6cd
```

仓库内 compact 副本：[Q0 判定](evidence/q0-baseline-20260720.json)、
[启动汇总](evidence/q0-startup-summary-20260720.json)、
[资源快照](evidence/q0-resource-snapshot-20260720.json)、
[固定 output 完整性](evidence/q0-fixed-output-integrity-20260720.json)、
[Gate receipt](evidence/q0-gate-receipt-20260720.json)、
[源码树清单](evidence/q0-smart-band-tree-20260720.json)和
[完整远端清单](evidence/q0-evidence-20260720.sha256)。
本地可独立复算 compact 文件与清单文件本身；清单所列 543 个原始远端工件没有全部
复制回仓库，因此“逐项复算通过”引用的是正式远端采集结果，而不是本地离线重哈希。

Q0 Gate：通过。

## 3. Q1-V 最终结果

固定 emulator console 原生提供 `screenrecord screenshot`，因此使用 headless framebuffer，
没有用 Browser 或 Xvfb 替代。正式证据：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/q1v-final-audited-20260720T2031CST
```

旅程为：

```text
启动到表盘
  -> goldfish touchscreen 右向左滑动
  -> Heart Rate / Model
  -> emulator heart-rate=104
  -> Heart Rate / 104 bpm / Sensor
```

自动断言结果：

- 三张截图均为 native `1280x800`、RGBA8、nonblank PNG。
- 表盘到心率页、数值区、Source 区分别变化 `143,269`、`1,874`、`612` 像素。
- 首次 swipe attempt 即进入心率页；最终 evidence 只保留这一轮成功尝试。
- 三个审阅后的精确 RGBA golden ROI 分别匹配 `Heart Rate`、`104 bpm`、`Source / Sensor`。
- 结构化渲染状态为 `page=heart_rate, value_bpm=104, source=sensor, freshness=fresh`。
- `Sensor` golden 证明这一帧是 fresh sensor 状态；它没有执行或证明停止上报后的
  stale/TTL fallback，该项仍为红项。
- `/dev/uorb/sensor_hrate0` 存在，console 连续注入并 `get` 返回 `104`。
- `pidof`、`ps`、fatal marker、NSH 和 console checks 全部通过。
- 四个固定运行输入均为独立 `copy2` 副本，源文件前后 SHA 全部不变。
- 唯一 run-id 的 `/proc` 归因通过；emulator 进程组、端口、runtime output tree 和四个
  暂存运行输入全部清理。51 项 manifest 逐项复算通过，远端 `du` 观测的 evidence
  目录分配量约 `432 KiB`。

最终 `journey.json` SHA-256 为
`b7cafe35b6eb68005cb34438df4f079d0b836a8f01c77e59b006ebe368ddef36`；
`evidence.sha256` SHA-256 为
`86683730704098e7bff6766ccb4bbdf7f82270329ee7a56bb175453f6cd243e4`。

仓库内 compact 副本：[Q1-V journey](evidence/q1v-journey-20260720.json)和
[完整远端清单](evidence/q1v-evidence-20260720.sha256)。本地审阅图：

- [native 表盘](evidence/q1v-native-e2e-watch-face-20260720.png)
- [滑动后的 Heart Rate / Model](evidence/q1v-native-e2e-heart-model-20260720.png)
- [注入后的 104 bpm / Sensor](evidence/q1v-native-e2e-heart-sensor-20260720.png)

Q1-V Gate：通过。

## 4. 保留的失败与被替代证据

- 首轮 Q0 `/evidence/q0-baseline-20260720T1822CST` 没有逐轮隔离可写数据盘，已被最终 Q0 替代。
- 中间 Q0 `/evidence/q0-audit-hardened-20260720T1953CST` 缺少最终 receipt/source
  绑定与清理加固，只作保留证据。
- 首轮 Q1-V `/evidence/q1v-harness-final-20260720T1913CST` 使用只读 hardlink 且仅断言 ROI 变化，已被最终 Q1-V 替代。
- 中间 Q1-V `/evidence/q1v-audit-hardened-20260720T1950CST` 只作保留证据。
- 远端 `/evidence/q1v-final-audited-20260720T2026CST` 的 console 接受了 swipe，但截图仍停在
  表盘；`Heart Rate` title golden 正确判失败，证明 harness 会拦住真实的页面误绿。
- 默认 emulator 的 `heart-rate` 为 disabled，初次 `sensor set/get` 返回 `KO`。
- 直接追加或替换 `-android-hw` 的方案被 frontend 后置配置覆盖。
- 早期 stale/fallback 尝试没有形成可验收证据，失败结果仍保留。
- 第一版 harness 曾误把可写数据盘 hardlink；运行被立即终止，固定数据盘 SHA 复核未变。

相关目录均保留在远端 `evidence/q0-*`、`evidence/q1v-*` 下，没有用新成功目录覆盖旧失败。
T2026 失败运行的 JSON、截图与清单没有复制进 `docs/evidence/`，这里只记录其远端目录和
已观察到的 Gate 失败原因。
当前仍只证明一条固定分辨率旅程，不代表全应用、第二分辨率、长稳、真机或功耗已通过。

## 5. 复跑与下一切片

本地 helper：

```powershell
python tests\test_q0_baseline.py
python tests\test_native_e2e.py
python tests\test_emulator_smoke.py
```

最终 helper 计数：Windows 本地 Q0 `15` 项、Q1-V `14` 项，各有 `1` 项 POSIX-only
case 跳过；Ubuntu Q0 `15/15`、Q1-V `14/14`，emulator helper `5/5` 全部通过。

Q0 正式运行还必须提供匹配当前基线的 gate receipt、source snapshot、完整 revision 和预期
NuttX SHA；collector 会拒绝少于 20 次或放宽预算。Q1-V 必须使用新的空 evidence 目录和
空闲偶数端口。

下一最小切片是 Q1-C 第一半：runtime、event queue、clock abstraction 和 capability
model。Q1-S storage codec 仍是其后的独立切片。
