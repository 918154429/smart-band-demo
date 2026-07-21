# Q2 多表盘垂直闭环

日期：2026-07-21（Asia/Shanghai）

Q2 A Gate 已在功能提交 `d7ce79b57c9900dafe083103ab3c37ce72cd321a` 上通过。
Lotus Health、Activity Rings、Minimal Digital 三款表盘已进入同一 registry/lazy
lifecycle，600ms 长按 picker、手势仲裁、Q1-S A/B settings、双分辨率 native journey
和 100 次切换压力均已闭环。本结论不包含目标板、真实掉电介质或其他 A-G 功能。

## 产品行为

- Lotus 继续作为默认表盘；Activity 突出卡路里、步数与心率区间；Minimal 只保留大时间、
  日期、电量和状态。
- 当前表盘长按 600ms 打开 picker。普通表盘页横滑仍切换主页面；picker 内横滑只切预览。
- picker 支持 Previous、Next、Apply、Cancel，只显示 descriptor 名称与三色色板，不 mount
  预览表盘。
- Apply 先卸载旧表盘，再挂载目标表盘；挂载失败会恢复上一表盘并保持 picker 可见。
- 三款表盘共享单个 256-byte context buffer；稳态只存在一棵 face subtree。Lotus、Activity、
  Minimal 分别创建 41、15、9 个 face 对象，新增表盘不会线性增加 context storage。
- picker 是固定 18 个对象和约 104-byte controller 的显式常驻开销；没有声称 Q2 对整个
  `g_ui` 做到零字节增长。

## Settings 与恢复

selected face 复用 Q1-S store：

```text
slot A: 0x00020000
slot B: 0x00020001
schema: 1.1
payload: face id + 3 reserved zero bytes
```

测试覆盖默认 Lotus、正常 commit/reload、单槽损坏回退、双槽 degraded、旧 `1.0` 单字节
payload 迁移、未来 schema、非法 ID/reserved/size、write/flush 失败和无 storage。本次会话在
无 storage 时仍允许切换；重启重新使用安全默认 Lotus。

file backend 的 100 次真实 commit 在 Linux 上执行约 500 组顺序 open/close，进程 fd
计数为 `3 -> 3`；Windows 严格测试的进程 handle 计数也保持不变。fake LVGL 的 100 次
用户可见三表盘轮换同时证明 object/event callback 回到各表盘固定基线。

## Host、Browser 与 coverage

最终 PR checks：

- Host run [29824971500](https://github.com/918154429/smart-band-demo/actions/runs/29824971500)：
  GCC、Clang、MSVC、统一 coverage、完整 UI compile 和 evidence harness 全绿。
- Browser run [29824971555](https://github.com/918154429/smart-band-demo/actions/runs/29824971555)：
  Chromium `6/6`。

独立 Linux 目录为 `/data/smart-band-q2-watch-faces-20260721`。coverage source
`37d584a28cd6e3873067b967f328501c5ec280df`；最终功能提交只在 native framebuffer
刷新等待上增加 0.6 秒，没有修改生产 C 或 coverage source。

- overall：`94.0% (2981/3172)`。
- `watch_face_settings.c`：`92.1% (35/38)`，functions `100%`。
- file backend：`91.4% (223/244)`。
- fd soak：100 commits，`3 -> 3`。
- 日志：`evidence/coverage-37d584a.log`。
- 日志 SHA-256：`4c7c4108b1cf5a01e75bf64040120899ef4fd40b1c00275a6aa1e0d2be5afe30`。

## Fixed openvela 与 native

最终 run [29825089126](https://github.com/918154429/smart-band-demo/actions/runs/29825089126)
在 `d7ce79b57c9900dafe083103ab3c37ce72cd321a` 上通过。214 个 project 全部解析为固定 SHA，
build、链接验证、compact/framed native、sensor、清理和 artifact 上传均成功。

- artifact：`openvela-fixed-release-29825089126`，`24,230,516` bytes。
- artifact digest：`sha256:7f41f8b424d5dace93bddc7e1a2a7cedb53de2443a785b54174e3bf454881d72`。
- NuttX：`66,247,120` bytes，SHA-256
  `2d57434f77c9a91b36fb9ccbbaf9d1fd14487aca29528836f9fadd409062cedf`。
- `.config`：`103,975` bytes，SHA-256
  `e15bd57b33f7ea33132fd2bfde144b6bdd07291a1795de147dabb8b448d06e10`。
- `vela_system.bin`：`064b842c5c963379f74467413568b85fdefb1d88ff3bebb827d1fff65dc06570`。
- `vela_data.bin`：`c9113c5761ee7a35fef47dfb8f744f30ba54e9c879cf3b1223a04189733ecbfa`。

### Compact 336x480

- Lotus 动态 mask 外比较 `146,941/161,280` 像素（91.11%），差异 `0`。
- 600ms 长按、Activity preview/apply、再次长按、Minimal preview/apply 均返回 console OK，
  两个结构化 selection marker 均存在。
- PID 13 在 5 秒稳定检查后仍为 13；所有截图非空且逐阶段 hash 变化。

### Framed 1280x800

- Lotus 动态 mask 外比较 `989,570/1,024,000` 像素（96.64%），差异 `0`。
- Activity 与 Minimal 的 picker preview、Apply 后 framebuffer 和 `id=1`/`id=2` marker 全过；
  两款实际表盘经人工审阅无字形裁切、重叠或圆角遮挡。
- 第一次横滑进入结构化 page ID `heart_rate`；`104 bpm`、`Source / Sensor` golden ROI 全过。
- UI ready `0.364171s`；进程组、归因进程、console port 5564、runtime input 与固定输出清理
  全部通过。

仓库内结构化证据见 `docs/evidence/q2-native-compact-smoke-20260721.json`、
`docs/evidence/q2-native-framed-journey-20260721.json` 与相邻 Q2 PNG。

## 保留的失败证据

| run | source | 失败点 | artifact digest |
| --- | --- | --- | --- |
| [29822578017](https://github.com/918154429/smart-band-demo/actions/runs/29822578017) | `be0e80c` | compact 动态 time mask 左边界漏 3 个抗锯齿像素 | `sha256:ca692b0d7067a512a777d0126980b036aa5bae1986fc7212ba4098c7f9691062` |
| [29824015101](https://github.com/918154429/smart-band-demo/actions/runs/29824015101) | `37d584a` | framed Apply 后 0.4s 抓到 stale picker framebuffer | `sha256:1b7e51f5ac21485ae30077069876bff206516db0babf82f6d92a620b25bde7e9` |

初次 Host run `29822075399` 因独立 settings wrapper 的三个 runner 都没有 `gcovr` 失败；
`be0e80c` 将它明确交给统一 GCC coverage gate。后续 Host run `29823662342` 暴露统一 coverage
target 漏列 `storage_file.c`，由 `37d584a` 修复。所有失败 run 均保留，没有 rerun 覆盖。

## 证据边界

- native `.config` 仍为 `CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH=""`。native 证明真实生产
  UI、picker 和本次会话切换，不证明非空路径重启恢复或目标板掉电 durability。
- host file backend 证明编码、A/B recovery、open/close 平衡与 reload，不等于具体目标板文件系统
  的目录项/介质原子性。
- 未确认开发板型号/revision；本轮没有烧录、写 flash、真实触摸、真实 BLE 或功耗结论。
- Q3-Q6 仍只到 W1 pure core；本轮没有接入 Workout、History、notification、power 或 BLE。

