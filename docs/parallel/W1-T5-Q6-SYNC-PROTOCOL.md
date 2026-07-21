# W1-T5：Q6 sync protocol v1 envelope codec

把本文件全文作为一个独立对话框的任务指令。该对话只冻结无状态 envelope，不做 BLE。

## 身份与目标

实现与 transport 完全分离的纯 C v1 frame codec：显式 little-endian、固定 header、payload
长度、transaction ID、sequence/chunk index、flags/status 和 CRC16。payload 在本任务中是
opaque bytes；不得提前冻结 history/settings/notification 业务 payload。

完成仅表示 Q6-1 envelope core ready，不表示 sync service、loopback、Linux client 或真实 BLE
完成，也不得宣称 pairing/encryption 安全性。

## 冻结基线与 worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-q6'
$base = '927001a772a4431ae1cf74b745d9abdb884cd336'
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-q6-sync-protocol $worktree $base
Set-Location $worktree
git status --short --branch
```

## 唯一可修改文件

- 新增 `openvela_app/smart_band/include/smart_band_sync_protocol.h`
- 新增 `openvela_app/smart_band/services/sync_protocol.c`
- 新增 `tests/sync_protocol_test.c`
- 新增 `tests/test_sync_protocol.py`
- 可新增任务专属 coverage runner
- 新增 `tests/vectors/sync-v1-envelope.json`
- 新增 `docs/protocol/smart-band-sync-v1-envelope.md`
- 新增 `docs/parallel/results/W1-T5-RESULT.md`

禁止修改现有 `smart_band_sync_transport.h`、loopback、runtime、event、storage、构建入口、CI、
总 coverage、README、路线图和交接。

## 协议 contract

- 不得序列化 raw struct；header 每个 offset、宽度、endianness 和 reserved bits 写入文档。
- protocol major/minor、frame type、payload length、transaction ID、sequence/chunk index、
  flags/status、CRC16 都必须有明确位宽。
- 明确 CRC16 variant 的 polynomial、init、reflection、xorout 和 coverage 范围，并使用标准
  CRC vector 证明实现，不允许只用实现自生成的期望值。
- 至少一个完整 frame golden vector 必须硬编码独立期望 bytes；encode/decode 双向验证。
- 最大 payload/frame 固定并适合后续 MTU 分片，但本任务不实现分片。
- decode 要求精确 frame 长度，区分 truncated、trailing、bad version、bad type、bad flags、
  bad status、bad CRC、bounds、buffer-too-small 和 invalid argument。
- unknown major 必须拒绝；minor 兼容规则必须写清。reserved bits 非零必须拒绝。
- duplicate、out-of-order、ACK、retry 和 transaction state 属于后续 sync service；本 codec
  不维护会话状态，也不能假装已验证这些行为。

## 必测矩阵

- 标准 CRC vector、独立完整 frame golden vector、空 payload、最大 payload。
- 从 0 到完整长度减 1 的每个截断前缀。
- trailing byte、声明长度过大/过小、未知 major、未来 minor、非法 type/flags/status/reserved。
- header/payload/CRC 每个关键 byte corruption。
- encode output capacity 从 0 到 required-1；decode 输出 view 不越界。
- 源/目标 buffer aliasing 规则明确并测试。
- 至少 10,000 个确定性 pseudo-random malformed frames，无崩溃和越界；不是安全 fuzz 结论。

测试入口使用 GCC/Clang/MSVC C11 严格警告；生产源行覆盖率目标 `>=90%`。

```powershell
python tests/test_sync_protocol.py
git diff --check
```

## 交付

结果文档必须列出最终 header 表、CRC 参数、兼容规则、错误 enum、golden vector 来源、测试与
coverage、尚未冻结的业务 payload，以及 W1-I 所需的 build/CI 接线。

```powershell
git add -- <仅本任务允许文件>
git commit -m "Add sync protocol v1 envelope codec"
git push -u origin codex/w1-q6-sync-protocol
```

不要合并，不要修改 transport，不要实现 BLE/GATT、分片或 sync service。
