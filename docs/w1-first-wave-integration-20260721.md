# W1 第一波汇总接线与门禁

日期：2026-07-21（Asia/Shanghai）

W1-I 已完成五个冻结叶子提交的所有权审计、顺序汇入、共享构建/CI/coverage 接线、
Host/Browser、fixed openvela build 和双分辨率 native 门禁。该结论只表示 Q2 registry/Lotus
slice 与 Q3-1/Q4-1/Q5-1/Q6-1 pure core ready；Q2-Q6 产品总 Gate 均保持未完成。

## 输入与所有权审计

| 任务 | 输入提交 | 审计结论 |
| --- | --- | --- |
| T1 | `dca540a2905ddb83ed9261c61722d5fe1261d8d9` | 只修改允许的 UI/build/test 文件；未实现 Activity、Minimal、picker、settings 或 runtime/storage 语义 |
| T2 | `4533e741e1312dccb8b21a0bc465edfbbe0104ce` | 只新增 step normalizer、workout model、独立测试和结果文档 |
| T3 | `dca876cfdd0d250ba69670fafa06f0ed7c34357f` | 只新增 notification model/demo、独立测试和结果文档 |
| T4 | `02088eed1dc6304b2ae96cf0cec836cdae679695` | 只新增 power policy、独立测试和结果文档 |
| T5 | `4352778db03eafa2d02018b2d8f02d0cf865ea92` | 只新增 v1 envelope codec、协议文档、vector、独立测试和结果文档 |

汇入顺序固定为 T1 -> T2 -> T3 -> T4 -> T5，使用普通 `cherry-pick`。T2-T5 未接
runtime/event/UI/platform，没有为了链接制造伪调用。

## 共享接线

- 六个纯 C 生产源进入 CMake、Make、完整 UI compile、shell parity 和 production core
  coverage source 集。
- watch-face registry 以及 T2-T5 每个 wrapper 均进入 GCC、Clang、MSVC 严格警告矩阵。
- Linux GCC 总 coverage 继续要求 `>=85%`，六个 W1 新源逐文件要求 `>=90%`。
- native nightly 同时验证 `336x480` compact Lotus 和 `1280x800` framed Lotus；动态值区域
  使用审阅后的固定 mask，mask 外必须逐像素完全一致。

## Host、Browser 与 coverage

本地 Windows 最终运行五个叶子入口、全部既有 production C/UI 入口、三组 evidence
harness、reproduction rollback、`npm ci` 和 Browser `6/6`，全部通过。Windows 的 POSIX
harness 用例保持预期 skip，未写成 native 通过。

独立 Linux 目录：

```text
/data/smart-band-w1-integration-20260721
```

coverage source commit 为 `e7df36e218a19e1652fd8b59d9b924f3a5e5fa3e`，日志 SHA-256：

- `linux-host.log`：`9e1c45dea621d965c8781e6f1e033c29655145dfb6ebaa7e4aaffd83f479673c`
- `coverage.log`：`c2fb7327958c5746b50a9bd66f8c910a8d5305b7e1eef98549db4dd1098ea9b2`

| 范围 | 行覆盖率 |
| --- | ---: |
| production core overall | 94.0% (`2945/3134`) |
| `step_normalizer.c` | 99.0% (`104/105`) |
| `workout_model.c` | 97.6% (`203/208`) |
| `notification_model.c` | 99.0% (`195/197`) |
| `notification_demo.c` | 100% (`17/17`) |
| `power_policy.c` | 99.5% (`184/185`) |
| `sync_protocol.c` | 100% (`117/117`) |

最终功能/门禁提交 `298a5c0aa45968b341739c3d1c5a3c103f84e2eb` 的 PR checks：

- Host run [29816173199](https://github.com/918154429/smart-band-demo/actions/runs/29816173199)：通过。
- Browser run [29816173171](https://github.com/918154429/smart-band-demo/actions/runs/29816173171)：通过。

## Fixed openvela 与 native

最终 run [29816300149](https://github.com/918154429/smart-band-demo/actions/runs/29816300149)
在 `298a5c0aa45968b341739c3d1c5a3c103f84e2eb` 上通过。固定 manifest 解析出 214 个
project，build、链接校验、compact/framed native、清理和 artifact 上传均成功。

artifact `openvela-fixed-release-29816300149` 为 `23,861,899` bytes，GitHub 官方 digest：
`sha256:554fac66d9165ad48ae765a25adef855527eb762c78a8c3705076bcf97a11823`。

| 文件 | bytes | SHA-256 |
| --- | ---: | --- |
| NuttX | 66,114,392 | `61aa4877b597bd956e2ab4b34a2dcc913940979362279ffcf9c94f13a39ec051` |
| `.config` | 103,975 | `e15bd57b33f7ea33132fd2bfde144b6bdd07291a1795de147dabb8b448d06e10` |
| `vela_system.bin` | 1,024 | `f600f2ed0a4000ae2b96e6ea8d12fa2eff72884c3d95e48161b154c2ed28235f` |
| `vela_data.bin` | 268,435,456 | `35044e5a05d861094d2ab0f4c24b67e3b71d91f964249ee45f6e0a4be1a75063` |

固定依赖为 manifest `67df2c52308f2579ac50d0cd7413e7f0e092b83a`、official skills
`ab5f8be8225ce25c2f808fae0085dbf2db8fadf4`、emulator
`be9cdef6709c2a7aed547c3029d8872c58e5f3f9` 和 emulator tools
`37f5024f1d9157b9778d0d9e739ee0fa68743d42`。

### Compact Lotus

- skin：`xiaomi_smart_band_8_pro`，截图 `336x480`。
- 迁移前 reference 来自 Q1-C NuttX
  `88d3242eb9605eff3891d5ae215b3ffede4f0f0c80276fa605e890b06770c912`。
- mask 外比较 `146,971/161,280` 像素（91.13%），差异 `0`。
- UI ready `0.238s`，PID 13 在 5 秒稳定检查后仍为 13。

### Framed Lotus 与 Heart Rate

- framed Lotus `1280x800`；mask 外比较 `989,570/1,024,000` 像素（96.64%），差异 `0`。
- UI ready `0.271724s`；第一次横滑即进入结构化 page ID `heart_rate`。
- `Heart Rate`、`104 bpm`、`Source / Sensor` 精确 golden ROI 全部匹配。
- emulator/uORB sensor、PID/ps、固定输出未变、runtime input、进程组、归因进程和端口清理
  全部通过。
- journey SHA-256：`ad2439e15795b254854dd6dcc774d92ca5edefa123a38d353f64220aabb474a3`。

仓库内 compact 证据见 `docs/evidence/w1-native-compact-*`，framed/Heart Rate 证据见
`docs/evidence/w1-native-framed-lotus-20260721.png`、
`docs/evidence/w1-native-heart-*-20260721.png` 和
`docs/evidence/w1-native-journey-20260721.json`。

## 保留的失败证据

| run | source | 失败点 | artifact digest |
| --- | --- | --- | --- |
| [29812790986](https://github.com/918154429/smart-band-demo/actions/runs/29812790986) | `36ff701` | compact 动态 glyph mask 少 20 个边缘像素 | `sha256:a6518924d75aa38259aea2e6ef9548f78eff6cc4051d355fc8e495aaf9add382` |
| [29814087721](https://github.com/918154429/smart-band-demo/actions/runs/29814087721) | `714cdb1` | compact smoke 后端口 5554 尚未释放 | `sha256:a7877208202d30e7ffab18ee8aa4add447a9f7c78950dfcc0b868d9d2ba6127d` |
| [29815208311](https://github.com/918154429/smart-band-demo/actions/runs/29815208311) | `d2e2827` | framed 分钟 glyph mask 少 3 个边缘像素 | `sha256:f8408eeb58b7c4f4f6c62b35b25fff43cb1e3547c4450f4fdf5e0f5925e11a44` |

对应 compact/framed PNG 与 JSON 已固化在 `docs/evidence/w1-failed-*`，没有覆盖旧 run。

## Gate 结论与下一步

- Q2：registry/Lotus slice 完成；Activity、Minimal、picker、settings、持久化和 100 次用户
  切换仍未完成，Q2 总 Gate 保持红色。
- Q3：step normalizer/workout pure core ready；service、UI、checkpoint/history 未接，B/D Gate
  保持红色。
- Q4：notification queue/demo pure core ready；event、center、overlay、call、haptic 未接，C Gate
  保持红色。
- Q5：power policy pure core ready；timer/scheduler/sensor/platform/wrist 和真机功耗未接，E Gate
  保持红色。
- Q6：v1 envelope codec ready；业务 payload、session、loopback fault、Linux client、GATT/BLE
  未完成，F Gate 保持红色。

下一波先完成 Q2 Activity、Minimal、picker/settings，再分别启动 Q3-Q6 adapter/UI/service
切片；所有 settings 必须复用 Q1-S store。
