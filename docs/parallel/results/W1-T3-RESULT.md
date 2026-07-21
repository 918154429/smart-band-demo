# W1-T3 Q4 notification core 结果

日期：2026-07-21（Asia/Shanghai）

分支：`codex/w1-q4-notification-core`

基线：`927001a772a4431ae1cf74b745d9abdb884cd336`

## 结论

已交付固定容量、无堆分配的 Call/SMS/App/System 通知纯模型、确定性 demo injector 和
专属 host test/coverage runner。这里只表示 **Q4 notification core ready**；没有接入
event、runtime、haptic、LVGL 或平台接口，Q4 C Gate 仍保持红色。

## 固定容量与字符串上限

| 项目 | 固定容量 | 可保存字符数 | 行为 |
| --- | ---: | ---: | --- |
| 队列 | 16 条 | - | 头文件显式定义，无堆分配 |
| source | 24 bytes | 23 | 超长输入截断，末尾始终 NUL |
| title | 48 bytes | 47 | 超长输入截断，末尾始终 NUL |
| body | 128 bytes | 127 | 超长输入截断，末尾始终 NUL |

ID `0` 保留为非法值；`1` 到 `UINT32_MAX` 可用。空 source/title/body 合法。wall timestamp
由调用方提供，模型不读取墙钟。

## 重复 ID 规则

相同 ID 到达时不新增、不改变队列位置，并返回 `PUT_UPDATED`：

- `type` 是身份的一部分，必须与原记录一致；不同 type 返回 `PUT_INVALID`，状态不变。
- 允许更新 `priority`、`source`、`title`、`body`、`wall_timestamp`。
- 保留 `read`、`dismissed` 和 `action_state`，重放不能复活已处理通知。

## 满载与淘汰顺序

未 dismiss、未 accept/reject 且 priority 为 high/critical 的 Call 是受保护项；仅标记 read
仍视为未处理来电。受保护项不会被任何新通知淘汰。

从非保护项中按以下顺序选择淘汰候选：

1. priority 更低者优先；
2. 同 priority 时，dismissed/terminal 优先于 read，read 优先于 unread；
3. 再按 wall timestamp 更旧者优先；
4. timestamp 相同时按当前队列中更靠前者优先。

新通知仅可替换 priority 更低的候选，或替换同 priority 的已处理/read 候选。同 priority
unread 不被新 unread 覆盖，返回 `PUT_FULL`。若队列中完全没有非保护候选，返回
`PUT_PROTECTED`。所有拒绝路径保持模型字节状态不变。

## Action 状态表

| command | 适用类型 | 初始状态结果 | 重复结果 | 冲突结果 |
| --- | --- | --- | --- | --- |
| read | 全部 | `read=true`，`APPLIED` | `NO_CHANGE` | 不改变 terminal state |
| dismiss | 全部 | read + dismissed，state=`DISMISSED` | `NO_CHANGE` | accepted/rejected 后 `INVALID` |
| accept | 仅 Call | read + dismissed，state=`ACCEPTED` | `NO_CHANGE` | 非 Call 或其他 terminal state 为 `INVALID` |
| reject | 仅 Call | read + dismissed，state=`REJECTED` | `NO_CHANGE` | 非 Call 或其他 terminal state 为 `INVALID` |

未知 ID 返回 `NOT_FOUND`；空指针、ID 0 和非法 enum 返回 `INVALID`，且不修改记录。

## Presentation decision 表

decision 只包含 abstract `center_only`、`overlay`、`full_screen`、`haptic` 和
`wake_request`，不调用平台接口。

| policy / notification | center | overlay | full-screen | haptic | wake |
| --- | --- | --- | --- | --- | --- |
| 已 dismiss/accept/reject | only | no | no | none | no |
| DND（任意类型/优先级） | only | no | no | none | no |
| workout + normal/low 非 Call | only | no | no | none | no |
| workout + high/critical 非 Call | no | yes | no | normal | yes |
| workout + Call | no | yes | no | urgent | yes |
| 普通模式 + low 非 Call | only | no | no | none | no |
| 普通模式 + normal 非 Call | no | yes | no | subtle | no |
| 普通模式 + high 非 Call | no | yes | no | normal | yes |
| 普通模式 + critical 非 Call | no | yes | no | urgent | yes |
| 普通模式 + Call | no | no | yes | urgent | yes |

DND 优先级高于 workout；workout 下不会产生 full-screen 决策，避免阻断训练交互。

## 确定性 injector

`smart_band_notification_demo_inject(model, seed, sequence)` 只使用显式 seed/sequence 生成
ID、type、priority、文本和 timestamp。同一 seed/sequence 重放得到同一 ID，并按重复 ID
规则更新；不读取随机设备或系统时间。

## 测试与覆盖率

执行：

```powershell
$env:CC = '<WinLibs GCC 16.1 gcc.exe>'
$env:GCOV = '<WinLibs GCC 16.1 gcov.exe>'
python tests/test_notification_core.py
git diff --check
```

结果：

- GCC 16.1，C11，`-Wall -Wextra -Werror -pedantic`：通过，无告警。
- 1000 条 Call/SMS/App/System 确定性混合输入：通过，无越界、崩溃或队内重复 ID。
- 未满/刚满/满载、全 high、全 unread、混合 read、protected Call 淘汰矩阵：通过。
- 重复 ID、相同 timestamp、ID 边界、超长/空字符串：通过。
- read/dismiss/accept/reject 合法性与幂等：通过。
- DND/workout/普通/高优先级 presentation 矩阵：通过。
- 查询、遍历、删除头/中/尾和顺序稳定性：通过。
- 空指针、非法 enum、损坏的容量边界保持状态：通过。
- `notification_model.c`：line 98.98%（197 行），branch executed 100%，branch taken
  94.71%。
- `notification_demo.c`：line 100%（17 行），branch executed/taken 100%。
- `git diff --check`：通过。

## Integration request

W1-I 后续需要统一完成以下共享接线，本分支未越权修改：

1. 把两个 production source 和专属测试纳入共享 CMake/Makefile、CI 与总 coverage 边界。
2. 建立 `SMART_BAND_EVENT_NOTIFICATION_RECEIVED/ACTION` 到 put/apply/remove 的 adapter，明确
   runtime 所有权和线程/队列边界。
3. 由 UI adapter 消费 presentation decision 创建通知中心、overlay 和来电页；由 haptic/
   power adapter 执行 abstract haptic/wake 请求。
4. 集成时完成 openvela 与 native 双分辨率验证，并保持 workout overlay 非阻断。

## 已知风险与未完成项

- 未运行 openvela/full native build，符合 W1-T3 禁止运行五份共享构建的约束。
- 尚无持久化、同步 payload、通知中心 UI、overlay、来电页、震动或 runtime adapter。
- 字符串按 byte 截断；未来若 UI 输入 UTF-8，需要 adapter 避免截断到多字节码点中间。
- wall timestamp 的正确性由上游保证；相同 timestamp 仅依赖稳定队列顺序决胜。
