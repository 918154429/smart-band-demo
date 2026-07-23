# Gemini-S1 暂定目标板与适配计划

更新时间：2026-07-21（Asia/Shanghai）

## 1. 决策状态

本项目的开发板采购与适配候选暂定为 **润芯微 Rivotek Gemini-S1**。这是目标选择，
不是到货或真机验收结论：当前未确认已经下单、实物 PCB revision、配套软件版本、恢复
工具和可写入范围，也未授权任何烧录操作。

选择它的核心原因是公开证据比通用开发板完整：Gemini-S1 已列入 openvela 官方兼容性
认证表，并有精确的 R528 board BSP 和显示配置。它适合作为比赛功能验证板，但不是手环
尺寸或低功耗量产硬件。

## 2. 公开证据

| 项目 | 当前可确认事实 | 证据边界 |
| --- | --- | --- |
| 官方认证 | Rivotek Gemini-S1，openvela 5.2，证书 `OV20260412` | 认证不等于本仓库已经构建或运行 |
| SoC | 全志 R528，双核 Arm Cortex-A7 | 实物丝印与 revision 仍需 HW0 核对 |
| BSP | `vendor/allwinnertech/boards/r528/r528s3-gemini-s1` | 当前公开在 `dev-ai-contest-2026`，默认 `dev`/`trunk-5.4` 不含该目录 |
| configs | `nsh`、`nsh_minidisplay`、`bootloader` | 本项目尚未冻结对应 manifest 和项目 SHA |
| 显示 | 板载 2.8 寸 SPI 屏，支持 7 寸 MIPI 扩展；mini config 启用 ILI9341 | 分辨率、方向、颜色和背光尚未在实物验证 |
| 触摸 | config 启用 GT911 与 LVGL NuttX touchscreen | shipped panel/controller 和坐标映射仍待确认 |
| 无线 | 公开说明为 Wi-Fi + Bluetooth 双模；config 启用 BLE advertise/scan、ZBlue LE 与 GATT | HCI、天线、GATT server 和稳定性尚未真机验证 |
| 环境传感器 | 公开说明列出温湿度、光照、接近；config 启用 SHTC3、LTR553、SGP30/uORB | 焊接料号、地址和节点要逐项读取确认 |
| 内存/存储 | 候选商品标题标注 `128+256MB` | RAM/Flash 对应关系、分区和可用容量不能仅按标题推定 |

官方入口：

- [openvela 兼容性认证开发板](https://github.com/open-vela/docs/blob/dev/zh-cn/dev_board/Certified_Board.md)
- [Gemini-S1 BSP](https://github.com/open-vela/vendor_allwinnertech/tree/dev-ai-contest-2026/boards/r528/r528s3-gemini-s1)
- [Gemini-S1 中文板级说明](https://github.com/open-vela/vendor_allwinnertech/blob/dev-ai-contest-2026/boards/r528/r528s3-gemini-s1/README_zh-cn.md)
- [`nsh_minidisplay` defconfig](https://github.com/open-vela/vendor_allwinnertech/blob/dev-ai-contest-2026/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/defconfig)
- [Gemini-S1 AI/LVGL 配置说明](https://github.com/open-vela/packages_ai_agent/blob/dev-ai-contest-2026/defconfigs/gemini-s1/README.md)
- [润芯微官方资料入口](https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc)

候选商品页：

- [京东商品 `10216954676904`](https://ic-item.jd.com/10216954676904.html)，页面标题为
  `RIVOTEK 润芯微 Gemini-S1`，包装清单显示开发板、屏幕、数据线和触控笔。库存、价格、
  seller、revision 和实际包装必须以下单页及收货实物为准。

## 3. 双基线策略

现有模拟器复现由 `skills/openvela-smart-band-reproduce/versions.env` 固定到
`tags/trunk-5.4.xml`，manifest commit 为
`67df2c52308f2579ac50d0cd7413e7f0e092b83a`。这个基线继续负责 goldfish CI、native
framebuffer 和回归证据，不因选择 Gemini-S1 而替换。

Gemini-S1 使用独立板卡基线：

1. 从包含 `r528s3-gemini-s1` 的 contest manifest 建立隔离 checkout。
2. 把 manifest、`vendor_allwinnertech` 及所有项目解析为 40 位 SHA，禁止 Gate 跟随浮动分支。
3. 先以 `nsh_minidisplay` 做 compile/link-only，再决定最小 defconfig 差异。
4. goldfish 与 Gemini-S1 分别保存 `.config`、ELF/镜像哈希和 build receipt，证据不得混用。
5. G0 通过前不修改现有 nightly 的固定 `trunk-5.4` 输入。

2026-07-21 只读审计时的上游起点为：

| 仓库 | `dev-ai-contest-2026` HEAD |
| --- | --- |
| `open-vela/manifests` | `4557360c5df19ad53cd634a14208228487df0ba7` |
| `open-vela/vendor_allwinnertech` | `1b55d0b46298cb97b03978f1a59a0e96c16a1e16` |

该 manifest 的 `openvela.xml` 默认 revision 仍是浮动的 `dev-ai-contest-2026`。以上 SHA
只是 G0 的日期化入口，不是完整固定基线；正式证据必须执行 `repo manifest -r`，保存
解析后的所有项目 SHA 和 manifest 文件 SHA-256。

上游公开的参考构建入口为：

```sh
./build.sh \
  vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay \
  -j"$(nproc)"
```

该命令只有在 board manifest 和项目 SHA 已冻结的隔离 checkout 中才可作为 G0 输入；
目前不是本仓库已验证命令，更不是烧录授权。

## 4. 与 smart-band 的能力矩阵

| 能力 | 公开板卡状态 | 本项目缺口 | 首个 Gate |
| --- | --- | --- | --- |
| NSH / LVGL / libuv | defconfig 已启用 | 应用尚未对 board config compile/link | G0/HW1 |
| Display / backlight | ILI9341/显示配置存在 | 当前只验证 `336x480` 与 framed；需读实物分辨率和方向 | HW2 |
| Touch | GT911/NuttX touchscreen 配置存在 | 坐标旋转、长按和滑动尚未验证 | HW2 |
| RTC / watchdog | 不能由宣传页确认 | 节点、有效性、reset cause 和恢复行为未知 | HW1/HW3 |
| Storage | 商品宣称 flash，BSP 有分区配置 | `LVX_DEMO_SMART_BAND_STORAGE_PATH`、fsync/掉电语义未知 | HW3 |
| Temperature / humidity | 板载说明与部分 driver config 存在 | 映射到当前 uORB 路径并验证 freshness | HW4/HW6 |
| Heart rate | 未在公开板载清单中确认 | 需要受支持的外接传感器或明确 simulation 降级 | HW4/HW6 |
| Step / accelerometer | 未在公开板载清单中确认 | 需要受支持的 accel/step 节点或 simulation 降级 | HW4/HW6 |
| Battery / charging | 未确认 | 当前 `sensor_bridge.c` 仍使用 `/dev/charge/goldfish_battery` | HW4/HW6 |
| Haptic | 未确认震动器 | 需要硬件、driver 和 `smart_band_haptic_t` adapter | HW4 |
| BLE | 双模硬件说明和 LE/GATT config 存在 | HCI、Peripheral/GATT Server transport、重连尚未完成 | HW7 |
| Low power | 无本项目测量 | R528 评估板不代表手环续航，必须使用功耗仪 | HW8 |

当前 UI 坐标以 `330x626` 为设计基准，X/Y 分别缩放，而字体不会随屏幕同比缩小。
因此应把 `240x320` portrait 与 `320x240` landscape 都加入 fake LVGL 布局/创建失败测试；
实物优先验证 portrait，landscape 若出现裁切则建立独立紧凑 profile。已有 `336x480` 和
`1280x800` native 证据不能替代这两种目标尺寸。

外挂心率、加速度、电池计量和震动模块的具体料号暂不冻结。只有 openvela/NuttX 已有驱动
或能以受控范围补齐 driver，并且总线、电压和引脚与实物 revision 匹配时才采购。

## 5. 适配顺序

| Gate | Gemini-S1 任务 | 通过证据 |
| --- | --- | --- |
| G0 | 固定 contest board 基线，应用 compile/link-only | resolved manifest、`.config`、日志、ELF 符号和 SHA-256 |
| HW0 | 只读清点实物、revision、接口、原厂启动与恢复资料 | 照片/记录、串口/USB 枚举、原厂版本，不写 flash |
| HW1 | 验证恢复模式、NSH、时钟、内存、watchdog；优先临时启动 | 原厂/临时启动日志与可恢复证明 |
| HW2 | 读取 framebuffer/input 参数，完成 240x320/320x240 布局、背光、旋转、触摸 | 两方向 host gate、原生截图、结构化输入、1000 次触控无漂移 |
| HW3 | 冻结应用目录和文件系统，验证 A/B store 与掉电恢复 | 重启、故障和断电记录 |
| HW4 | 逐颗核对环境/健康传感器、电池和震动能力 | driver、总线地址、设备节点和独立读数 |
| HW5 | 所有硬件 provider 关闭，先跑完整 simulation UI | 真板 `UI ready`、页面/应用旅程 |
| HW6 | 每次只切换一个真实 provider | value/source/freshness/fallback 证据 |
| HW7 | BLE HCI、广播、GATT、同步与重连 | Bleak 客户端、100 次重连、1000 次通知 |
| HW8 | ACTIVE/DIM/OFF/WORKOUT/BLE SYNC 功耗与唤醒 | 功耗仪原始记录和重复测量 |
| HW9 | 掉电、看门狗、8h 交互、24h 待机与回滚 | 整机 RC evidence |

## 6. 到货前与到货后边界

到货前允许：读取公开资料、固定 board checkout、compile/link-only、准备 capability matrix
和只读清点表。

到货前禁止：声称真机显示/触摸/BLE/传感器/功耗通过，编造设备节点或恢复流程，把
contest 分支结果冒充 `trunk-5.4` 回归。

到货后仍须先完成 HW0。任何 flash、bootloader 或分区写入都必须同时满足路线图第 9.2
节，并由用户对当次具体操作明确授权。
