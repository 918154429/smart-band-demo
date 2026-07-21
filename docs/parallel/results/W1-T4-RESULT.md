# W1-T4 Q5 power policy core 结果

## 交付边界

本任务只交付确定性的纯 C power policy。实现不调用 LVGL、runtime、platform power ops、
sensor bridge，也没有修改构建入口、共享测试、CI、README、路线图或总交接。

这表示 Q5 power policy core ready，不表示 Q5 已接入产品，也不表示真实省电或真机电流
Gate 已完成。

## 默认 config

超时都从最后一次已接受的 wake event 开始按 monotonic time 计算：

| 配置项 | 默认值 |
| --- | ---: |
| dim timeout | 10000 ms |
| screen-off timeout | 30000 ms |
| large time-step threshold | 300000 ms |

每个 profile 的三种状态都显式配置 brightness、render period、heart sampling period 和三个
后台允许位：

| Profile | State | Brightness | Render | Heart | Motion | Checkpoint | Sync |
| --- | --- | ---: | ---: | ---: | --- | --- | --- |
| idle | ACTIVE | 100% | 16 ms | 1000 ms | 是 | 是 | 是 |
| idle | DIMMED | 25% | 100 ms | 2000 ms | 是 | 是 | 是 |
| idle | SCREEN_OFF | 0% | 1000 ms | 5000 ms | 否 | 否 | 否 |
| workout | ACTIVE | 100% | 16 ms | 1000 ms | 是 | 是 | 是 |
| workout | DIMMED | 35% | 100 ms | 1000 ms | 是 | 是 | 是 |
| workout | SCREEN_OFF | 0% | 500 ms | 1000 ms | 是 | 是 | 否 |

默认 idle SCREEN_OFF 相对 idle ACTIVE 的 render 频率降低 98.4%，heart polling 频率降低
80%。这只是 policy 数值比例，不代表实际调用次数或功耗实测结果。

## 状态与事件语义

```text
                    inactivity >= dim_timeout
            +------------------------------------+
            |                                    v
         ACTIVE ------------------------------> DIMMED
            |                                    |
            | inactivity >= off_timeout          | inactivity >= off_timeout
            +-----------------------+             |
                                    v             v
                                  SCREEN_OFF <----+

BUTTON / TOUCH / NOTIFICATION / WRIST / CHARGING from any state -> ACTIVE
WORKOUT_START / WORKOUT_STOP: change profile only; power state is unchanged
```

状态迁移只在显式 `TICK` 或 wake event 到达时计算。单次输入是 event bitmask，因此可以表达
同时到达的事件。wake 优先级固定为：

```text
BUTTON > TOUCH > HIGH_PRIORITY_NOTIFICATION > WRIST > CHARGING
```

wake event 与 `TICK` 同时到达时，wake 胜出并刷新最后活动时间。最后一次选中的 wake reason
保留在 snapshot 中，直到下一个 wake event 或 reset。

## Snapshot

输出只描述策略，不执行副作用：state、wake reason、display enabled、brightness、render 和
heart sampling period、workout 标志，以及 motion/checkpoint/sync 允许位。SCREEN_OFF 的
`display_enabled` 固定为 false，其他状态固定为 true。

## 非法输入与时间语义

| 情况 | 返回值 | 模型是否变化 |
| --- | --- | --- |
| 空指针 | `NULL_ARGUMENT` | 否 |
| 非法 config | `INVALID_CONFIG` | 否 |
| 未初始化 | `NOT_INITIALIZED` | 否 |
| 空 event、未知 bit、workout start/stop 冲突 | `INVALID_EVENT` | 否 |
| monotonic 回拨 | `TIME_ROLLBACK` | 否 |
| 重复 timestamp | `DUPLICATE_TIMESTAMP` | 是，合法事件仍确定性应用 |
| 超过 large time-step threshold | `LARGE_TIME_JUMP` | 是，直接计算目标状态，不循环追赶 |
| 非法 state/wake enum 或损坏的时间关系 | `INVALID_STATE` | 否 |

`init` 从给定 timestamp 建立 ACTIVE、idle、无 wake reason 的状态。`reset` 保留 config，
允许以任意新 monotonic epoch 重新建立同一初始状态。transition counter 为饱和 `uint64_t`，
不会发生 wraparound。

## 测试与 coverage

执行：

```powershell
$env:CC = "$env:TEMP\w64devkit-2.8.0\w64devkit\bin\gcc.exe"
python tests/test_power_policy.py
git diff --check
```

结果：

- GCC 16.1.0，C11，`-Wall -Wextra -Werror -pedantic`：通过。
- MSVC 19.44，C11，`/W4 /WX`：通过。
- `power_policy.c` line coverage：99.46%（185 行中覆盖 184 行），目标 >=90%。
- branch executed：100.00%；branch taken at least once：95.00%。
- 边界前 1 ms、恰好边界、边界后 1 ms：通过。
- 五种 wake reason 与同时事件优先级：通过。
- 三种状态下 idle/workout profile 切换：通过。
- 30 分钟加速循环 300 次：通过，无卡死或漂移。
- 1000 次 ACTIVE/DIMMED/SCREEN_OFF/wake 循环：通过，3000 次迁移计数正确。
- 空指针、非法 enum/config/event、重复时间、回拨和巨大跳变：通过。
- `git diff --check`：通过。

便携编译器只用于仓库外的本机 coverage 验证，没有加入提交。runner 也支持 PATH 中的
GCC/gcov 或 Clang/llvm-cov；只有 MSVC 时会先完成严格测试，再明确报 coverage 工具缺失，
不会把未测覆盖率当作通过。

## W1-I integration request

W1-I 应把 snapshot 当成唯一策略输入，映射关系如下：

1. `display_enabled`、`brightness_percent` 映射到 platform power ops；只在 snapshot 值变化
   时调用，core 不负责去重或重试。
2. `render_period_ms` 交给 runtime render scheduler；SCREEN_OFF 仍应先服从
   `display_enabled=false`，period 是调度策略值而不是强制绘制请求。
3. `heart_sampling_period_ms` 与 `allow_motion_sampling` 映射到 sensor scheduler。
4. `allow_checkpoint`、`allow_sync` 映射到后台任务门禁，platform 失败不得反写或偷偷改变
   core 状态。
5. runtime 应将同一轮收集到的事件合并成一个 bitmask，再调用一次 handle，以保留冻结的
   wake 优先级；timeout 必须作为显式 `TICK` 输入。
6. W1-I 需要新增共享构建/CI/总 coverage 接线，并在 native 双分辨率和 openvela 环境验证；
   真机电流与唤醒行为仍需单独硬件 Gate。

已知风险：当前 core 没有真实硬件电流、display/sensor 调用次数或 platform 失败恢复证据；
这些都不属于 W1-T4 的纯模型边界。
