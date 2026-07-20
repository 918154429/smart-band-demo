# Q1-C Runtime 与可注入平台验收

日期：2026-07-20（Asia/Shanghai）

Q1-C 已完成。该切片只建设 runtime、事件、时钟、能力、平台接口和 fake LVGL
可测试性，没有实现 Q1-S 的 codec/schema/CRC/A-B slot，也没有新增复赛 A-G 功能。

## 实现结果

- `smart_band_runtime_t` 统一持有 model、sensor bridge、app registry、clock、capability、
  主事件队列、外部事件 inbox 和 platform adapters。
- 主事件队列固定容量 16，支持 metrics 合并、优先级、同级 FIFO，以及满载时由关键
  事件淘汰最旧低优先级事件。
- 外部 callback 只向带锁的定长 inbox 复制事件；UI tick 再将其排入主优先级队列。
  deinit 先停止 sync transport，再关闭 inbox，避免 teardown 期间继续投递。
- wall clock 与 32-bit monotonic clock 均可注入；单调累计支持 wrap。RTC 启动时无效
  不会永久禁用能力，后续恢复有效时间后可重新显示墙钟。
- dirty flags 按 time、heart、steps、battery、environment、status、page、app 分类，
  controller 只渲染受影响的当前页面或应用。
- storage、power、haptic 和 sync 使用显式 ops/context 接口；默认 no-op 后端返回明确
  unavailable 状态。sync loopback 使用固定 8 帧、每帧最多 64 bytes 的内存队列。
- fake LVGL 维护真实父子对象树、递归删除、文本、flag、event、timer、虚拟 tick/wrap、
  live counters 和第 N 次对象/timer 创建失败注入。

## 自动化 Gate

Linux `GCC/gcov 13.3.0 + gcovr 8.6`：

| 范围 | 行覆盖率 | 门槛 |
| --- | ---: | ---: |
| 完整 host-testable production core | 90.9% (`1388/1527`) | 85% |
| `services/event_queue.c` | 93.2% | 90% |
| `services/event_inbox.c` | 100% | 90% |
| `services/clock.c` | 100% | 90% |
| `services/capabilities.c` | 100% | 90% |
| `services/runtime.c` | 96.6% | 90% |
| `platform/platform_noop.c` | 100% | 90% |
| `platform/loopback/sync_loopback.c` | 100% | 90% |

fake LVGL 对 compact/framed 两种主布局逐一注入全部创建调用失败，并对 8 个 lazy
mounted app 的每个创建调用逐一注入失败；每次都验证完整清理和随后重试成功。另执行
1000 次 UI create、导航、app mount、tick、back、destroy，object/event/timer 均零净增长。

本机最终回归通过六组 MSVC `/W4 /WX` 生产 C/UI 门禁、Browser `6/6`、emulator
harness `4 passed + 1 skip`、Q0 harness `14 + 1`、native harness `13 + 1` 及 shell
syntax/rollback；三个 skip 均为只适用于 POSIX 的 Windows 预期跳过。

## openvela 与 Native 证据

独立远端工作目录：

```text
/data/smart-band-q1c-20260720T223937CST
```

最终源码快照 SHA-256：
`c2d7fa8757761c3fabbe29e9b7e2f5e37aa381789f90633144a8f44254ee26c3`。
固定 manifest 解析出 214 个项目，全部固定到 SHA；fresh 和 incremental openvela 构建
均通过。最终 NuttX 为 `65,911,432` bytes，SHA-256 为
`88d3242eb9605eff3891d5ae215b3ffede4f0f0c80276fa605e890b06770c912`。

native E2E 在 `0.780311s` 看到 `UI ready`，第一次滑动进入 Heart Rate，注入后
`104 bpm` 与 `Source / Sensor` 精确 golden ROI 匹配。运行结束后进程、端口 5700 和
runtime output 均清理。紧凑判定见
[`evidence/q1c-gate-summary-20260720.json`](evidence/q1c-gate-summary-20260720.json)，
完整本地 journey 和审阅截图位于同一 evidence 目录。

## 保持红色的范围

Q1-S 仍未开始：没有持久化格式、版本迁移、CRC、A/B slot 或短写/EIO/ENOSPC/EROFS
故障矩阵。native stale/TTL fallback、全页面像素交互、第二分辨率、长时 soak、真机与
功耗也仍未证明。下一独立切片只能进入 Q1-S。
