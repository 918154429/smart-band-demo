# Q3 Workout 与 History 垂直闭环

日期：2026-07-22（Asia/Shanghai）

Q3 B 运动记录与 D 持久化/历史软件 Gate 已在功能提交
`8f8d788fa80f19dff23c1474b83a8f80570d03ed` 上通过。Walk/Run pure core 已接入后台
service、Workout UI、异常恢复、daily/session A/B transaction、7 天趋势与 30 天保留；
最终 fixed OpenVela 运行完成三次启动、非空 file backend、30 分钟定向 soak、资源取证和
七张 reviewed native PNG。本结论不包含目标板掉电耐久、真实传感器、BLE 或功耗。

## 产品与恢复闭环

- Walk/Run 支持倒计时、开始、暂停、恢复、结束、放弃和结束摘要；离开 Workout view 后
  service 继续持有状态。
- step normalizer 复用 W1 pure core，处理 reset、32 位回绕、来源切换、stale 恢复、负增量
  和异常跳变，没有第二套步数状态机。
- 每 30 秒及状态转换 checkpoint；异常重启进入待确认恢复态，不静默继续计时。
- daily ring 保留 30 天，History 显示 7 天缺失日/步数趋势与最近运动详情；session/daily
  逐字段复用同一服务数据。
- daily、checkpoint、session 通过 transaction manifest/epoch 恢复；crash-cut、CRC、半写、
  ENOSPC、旧/未来版本、午夜、时钟回拨和暂时 backend 错误由 host fault matrix 覆盖。

## Host、Browser 与 coverage

- Host run [29887461218](https://github.com/918154429/smart-band-demo/actions/runs/29887461218)：
  GCC、Clang、MSVC、production coverage、完整 UI compile 和 evidence harness 全绿。
- Browser run [29887461190](https://github.com/918154429/smart-band-demo/actions/runs/29887461190)：
  Chromium 全绿。
- overall production C line coverage：`92.8% (4304/4640)`。
- `storage_transaction.c`：`94.0% (188/200)`。
- `history_service.c`：`90.3% (594/658)`。
- `workout_service.c`：`90.1% (455/505)`。

## Fixed OpenVela 与最终长跑

最终 run [29888365756](https://github.com/918154429/smart-band-demo/actions/runs/29888365756)
在 `8f8d788fa80f19dff23c1474b83a8f80570d03ed` 上通过，耗时 `48m41s`。214 个
OpenVela project 全部解析为固定 SHA；fresh build、链接、compact golden、通用 native、Q3
三启动、清理和 artifact 上传全部成功。

- artifact：`openvela-fixed-release-29888365756`，`24,965,512` bytes。
- ZIP digest：
  `sha256:5adc9b82cb3019f6c8317cffeb3159c6adeeaed667afe3205aa2653165ff1d9e`。
- NuttX SHA-256：
  `950f2f8a97bedf83753e1d0faf04abada0c715c1642c1adf5c17982aa93df9aa`。
- `vela_system.bin`：
  `9571c4dcf8b2dc1687667879ce0b2744c052fc05c631b971736fdec1846f92b4`。
- staged `vela_data.bin` 从
  `c9cc1477dbc1bf9e37fbabc4fe0c185a83169312f202359b4724ab54b0f26e7a` 变为
  `de8654ceb81c57d8680a8edf021b8e3725830c2aecbbc42b50ca34188022bc92`；源构建产物保持不变。
- 本地审计目录：
  `C:\Users\Lenovo\AppData\Local\Temp\smart-band-openvela-29888365756`。

### 30 分钟测量

- warmup `300s`，正式 soak `1800s`，marker `180/180`，资源样本 `180/180`。
- marker 间隔 `10000..11036ms`，平均 `10077.7ms`；最大 tick gap `1146ms`。
- LVGL object count 恒为 `220`。
- queue、dropped、evicted、coalesced、inbox_dropped 最大值均为 `0`。
- smart_band PID 与成员集合全程固定为 `13`。
- 应用归因 heap：baseline `404800`，final `404800`，high-water `406976`，净增长 `0`。
- group fd：baseline `18`，final `18`，high-water `18`，净增长 `0`。
- 全 guest heap 污染哨兵：baseline/final `5275152`，high-water `5293648`，净增长 `0`。

资源数据来自固定 NuttX 的 `/proc/<pid>/group/status`、每个成员的 `/proc/<tid>/heap`、
`/proc/<pid>/group/fd` 与 `/proc/meminfo`。任一解析缺失、PID/成员变化、应用 heap 净增长超过
1 KiB 或 fd 净增长非零都会让 runner fail closed。

## Native 三启动与视觉证据

- boot1：启动 Walk、注入运动、暂停并持久化 checkpoint。
- boot2：恢复为待确认态，继续完成约 35 分钟 session，写入 daily/session 后进入 History。
- boot3：重新加载 History，session/daily 与 boot2 完成态一致。
- `evidence.sha256` 共 `1437` 项，本机全量复算 `0` 失败；三次进程组、console port、runtime
  tree 与固定输入清理全过。
- selection、paused、recovery、finish-confirmation、summary、history、history-reloaded 七张
  `1280x800` PNG 均非空、结构正确并完成人工复核；按钮、统计与底部详情无裁切或分页圆点
  遮挡。
- History 与 history-reloaded SHA-256 均为
  `977c45bfe1a439246d8772c9811463edeb5d3b9fa9d14c66802ec87a3b83fa95`。

## 保留的边界

- host fault backend 和 Goldfish file image 不证明 Gemini-S1 或其他目标板文件系统的真实掉电
  原子性、flash 磨损、目录项 durability 或介质寿命。
- `/proc/<tid>/heap` 是 NuttX allocation-record PID 归因，不是 Linux RSS；结论仅适用于本次固定
  FLAT build 与 `CONFIG_MM_RECORD_PID=y`。
- Q5 尚未接入真实 screen-off/runtime power adapter；本轮不声明实际电流或功耗 Gate。
- 未烧录硬件，未证明真实触摸、心率/计步传感器、BLE、震动、RTC 或 8/24 小时真机稳定性。
