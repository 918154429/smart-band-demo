# Q1-S 版本化存储与故障恢复验收

日期：2026-07-21（Asia/Shanghai）

Q1-S 的生产代码、host 故障矩阵和独立 Linux 覆盖率门禁已经通过。本文档写入时，分支
上的新鲜 fixed openvela build/native smoke 尚待 GitHub Actions 执行，因此 Q1-S 总 Gate
仍保持 pending，不能据此进入 Q2。

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
独立覆盖率门槛曾失败；只有 v6 是当前通过候选。

## 本地回归

- 七组 MSVC `/W4 /WX` 生产 C/UI 门禁通过，包括新增 storage core、central runtime 和
  完整 UI link smoke。
- evidence harness：emulator `4 passed + 1 expected Windows POSIX skip`、Q0
  `14 + 1`、native `13 + 1`。
- Browser `6/6` 与 reproduction shell syntax/rollback 通过。

## 待完成

- 在 `codex/q1s-versioned-storage` 当前提交上运行一次新鲜 fixed openvela release build
  和 native smoke，记录 run ID、artifact 名称及 SHA-256。
- 等 Host/Browser PR CI 通过后更新本摘要、路线图 checkbox 和下一会话交接，再正常合并。

Q2 多表盘和其他 A-G 功能不属于本切片。
