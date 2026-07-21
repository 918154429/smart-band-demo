# Q1-S 版本化存储与故障恢复验收

日期：2026-07-21（Asia/Shanghai）

Q1-S 的生产代码、host 故障矩阵、独立 Linux 覆盖率、GitHub Host/Browser CI，以及
新鲜 fixed openvela build/native smoke 已全部通过。Q1-S 总 Gate 完成；本切片没有实现
Q2 或其他 A-G 用户功能。PR10 已以 merge commit
`7a99c8a19049d4a7f06538424e10df66e0a3d2ee` 合入 master。

## 格式与恢复契约

- frame 使用显式 36-byte little-endian header，不序列化 C struct；magic 为 `SBST`，
  format 为 `1.0`，最大 payload 为 512 bytes，最大 frame 为 548 bytes。
- header 记录 `uint16` record type、schema major/minor、`uint64` generation、payload 长度、
  IEEE CRC32 payload 校验和 header 校验；解码要求精确长度并返回类型化错误。
- 每种 record 由 spec 指定两个不同 object ID。commit 读取两槽后写入另一槽，执行 flush，
  再回读、解码并核对 schema、generation 和 payload。
- 单槽损坏、缺失或新槽 schema 不可消费时，可回退到另一份 CRC-valid 完整记录；双槽
  损坏、同 generation 内容冲突或均不可消费时，返回默认值并暴露 degraded 状态。
- read 的 `IO_ERROR/UNAVAILABLE` 无法证明槽内容时，commit 在任何 write 前终止；
  `UINT64_MAX` generation 拒绝继续提交，避免回绕误选旧数据。
- migration 返回 OK、unsupported、buffer-too-small 或 invalid；成功后仍验证输出长度。

独立 golden vector 硬编码预期 frame bytes，并以标准 `123456789 -> 0xcbf43926` 向量
校验 CRC32 实现。

## 后端与接线

- memory backend 固定容纳 `16 x 4096` bytes，无堆分配。
- file backend 将数字 object ID 映射为 `object-%08x.bin`，单对象上限 4096 bytes，最多
  跟踪 16 个 dirty object；flush 使用 `fsync/_commit`，不依赖 rename。
- fault plan 可在指定第 N 次 read/write/flush 注入 EIO、ENOSPC、EROFS、短写、截断、
  corruption 和 interrupted mixed-image write。
- runtime 拥有 store，并在启动时加载固定 A/B runtime-checkpoint record。空、损坏或不可用
  的 storage 不会中止 runtime/UI 初始化。
- `LVX_DEMO_SMART_BAND_STORAGE_PATH` 为空时保持 no-op；非空时必须指向已经存在的目录，
  应用不会创建目录或猜测目标板分区。

host memory/file 结果只证明 codec/store 的恢复决策和通用 backend 行为，不证明目标板文件
系统具备原子写、目录项持久化或真实掉电 durability。runtime 启动仍同步调用 backend；当前
只证明错误不会导致初始化失败，不证明慢或永久阻塞 backend 的时延隔离。degraded 状态可以
从 store 读取，但本切片未新增用户提示或日志遥测。

## 故障与覆盖率 Gate

故障测试逐字节扫描 160 个切点，覆盖空目标及已存在旧尾的 short write、interrupted write、
generation-zero image 和 populated-target mixed image。另覆盖 EIO、ENOSPC、EROFS、truncate、
header/payload CRC、迁移回退、容量错误、同代冲突、read-before-write、file reopen/flush 和路径
校验。

独立 Linux 工作目录：

```text
/data/smart-band-q1s-20260721T132034CST
```

最终候选源码快照为 `source-v6`，归档 SHA-256：
`1c8fbe9a27793133ce604c29b2472c75509f46313e85acb9c5f07e7e056792ad`。覆盖率日志为
`/data/smart-band-q1s-20260721T132034CST/evidence/coverage-v6.log`。

| 范围 | 行覆盖率 | 门槛 |
| --- | ---: | ---: |
| 完整 host-testable production core | 92.2% (`2125/2305`) | 85% |
| `services/storage_codec.c` | 100% | 90% |
| `services/store.c` | 95.9% | 90% |
| `platform/storage/storage_fault.c` | 93.3% | 90% |
| `platform/storage/storage_memory.c` | 93.5% | 90% |
| `platform/storage/storage_file.c` | 91.4% | 90% |

v2-v5 的失败证据被保留：POSIX `ftruncate` 声明、coverage runner 参数和 file backend
独立覆盖率门槛曾失败；只有 v6 是最终通过的独立覆盖率候选。

## 本地回归

- 七组 MSVC `/W4 /WX` 生产 C/UI 门禁通过，包括新增 storage core、central runtime 和
  完整 UI link smoke。
- evidence harness：emulator `4 passed + 1 expected Windows POSIX skip`、Q0
  `14 + 1`、native `13 + 1`。
- Browser `6/6` 与 reproduction shell syntax/rollback 通过。

## openvela 与 Native 证据

第一次 fresh run [29805084515](https://github.com/918154429/smart-band-demo/actions/runs/29805084515)
在提交 `0547953` 上失败。NuttX `<nuttx/fs/fs.h>` 已声明 `file_read/file_write`，与 file
backend 的同名静态回调冲突；镜像校验与 native smoke 因此前置编译失败被跳过，失败
artifact 保留。提交 `a9d5a43` 将回调改为 `storage_file_*`，没有隐藏或重跑旧提交。

最终 fresh run [29806148523](https://github.com/918154429/smart-band-demo/actions/runs/29806148523)
在 `a9d5a4326063c75dabea4d61c31152ba981b15a7` 上通过：固定 manifest 解析为 214 个 SHA，
openvela build、镜像链接校验、native smoke、进程清理和 artifact 上传全部成功。

artifact `openvela-fixed-release-29806148523` 为 `23,615,392` bytes，GitHub 官方 digest：
`sha256:cd24812b5eb4c681a5b03a27a06082a777edbd1d4460521dd9406faa02a3f9e4`。

| 文件 | bytes | SHA-256 |
| --- | ---: | --- |
| NuttX | 66,073,888 | `b6605449990f01ab48c747c5e605ad4136eac7f8e1ef2dd6eb9831e282dff0dc` |
| `.config` | 103,975 | `e15bd57b33f7ea33132fd2bfde144b6bdd07291a1795de147dabb8b448d06e10` |
| `vela_system.bin` | 1,024 | `c432a814d04355da298268dc6fc6caafecfbe7cd7ce0fe799deed8949c8614a6` |
| `vela_data.bin` | 268,435,456 | `9b4405cb8a1ab36f0cc852300d7810957f218566fd5072d4a80b39d667d685ee` |

native smoke 在 `0.303s` 看到 `smart_band: UI ready`，启动 PID 为 13，5 秒稳定检查后仍为
13；emulator process group 与 console 均清理。固定依赖为 manifest
`67df2c52308f2579ac50d0cd7413e7f0e092b83a`、`.claude`
`ab5f8be8225ce25c2f808fae0085dbf2db8fadf4`、emulator
`be9cdef6709c2a7aed547c3029d8872c58e5f3f9` 和 emulator tools
`37f5024f1d9157b9778d0d9e739ee0fa68743d42`。

该 run 的 `.config` 明确为 `CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH=""`。它证明 Q1-S
生产源码可在固定 openvela/NuttX 中编译链接且默认 no-op 配置不回归启动；不证明配置非空
目录后的目标板持久化、重启恢复或真实掉电 durability。

## 后续边界

- 目标板到货后，以已确认分区运行非空 storage path 的写入、重启和掉电测试。
- 后续如接入可能阻塞的 backend，应增加超时/异步策略和 degraded 日志遥测。

下一独立切片为 Q2 多表盘，但不得把 host storage 证据升级为真机结论。
