# W1-T4：Q5 power policy 纯模型

把本文件全文作为一个独立对话框的任务指令。该对话只产生策略，不调用平台或 LVGL。

## 身份与目标

实现确定性的 ACTIVE -> DIMMED -> SCREEN_OFF 纯 C 状态机，冻结 timeout、wake reason、
目标亮度、render 周期、sensor 采样策略和后台任务允许矩阵。模型只输出 policy snapshot，
不得直接调用 `smart_band_power_ops_t`、sensor bridge 或 runtime。

完成仅表示 Q5 power policy core ready；没有真机电流证据，E Gate 仍为红色。

## 冻结基线与 worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-q5'
$base = '927001a772a4431ae1cf74b745d9abdb884cd336'
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-q5-power-core $worktree $base
Set-Location $worktree
git status --short --branch
```

## 唯一可修改文件

- 新增 `openvela_app/smart_band/include/smart_band_power_policy.h`
- 新增 `openvela_app/smart_band/logic/power_policy.c`
- 新增 `tests/power_policy_test.c`
- 新增 `tests/test_power_policy.py`
- 可新增任务专属 coverage runner
- 新增 `docs/parallel/results/W1-T4-RESULT.md`

禁止修改现有 `smart_band_power.h`、runtime、event、sensor bridge、platform、LVGL、构建入口、
CI、总 coverage、README、路线图和交接。

## 模型 contract

- 状态固定为 ACTIVE、DIMMED、SCREEN_OFF。
- config 显式提供 dim/off timeout、各状态 brightness、render period、heart sampling period、
  是否允许运动采样/checkpoint/sync 等，不隐藏时间常量。
- 输入使用 `uint64_t monotonic_ms` 与显式 event：touch、button、wrist、high-priority
  notification、charging、timeout/tick、workout start/stop。
- wake reason 必须可观测，并为同时到达事件定义稳定优先级。
- workout 与非 workout 的 SCREEN_OFF policy 不同：运动可保留必要采样与 checkpoint，
  非运动应显著降低心率轮询。
- 输出 snapshot 只描述 display enabled、brightness、render/sensor intervals 和后台允许位；
  不执行任何副作用。
- 时间回拨、重复 timestamp、巨大跃迁和无效 config 必须有类型化处理，不能卡死状态机。
- 初始化和 reset 行为确定，非法输入保持原状态。

## 必测矩阵

- timeout 前一毫秒、恰好边界、边界后一毫秒。
- ACTIVE -> DIMMED -> SCREEN_OFF 及每种 wake event 返回 ACTIVE。
- touch/button/wrist/notification/charging 的 wake reason 和优先级。
- workout start/stop 在三种 power state 的 policy 输出。
- 30 分钟加速 ACTIVE/DIM/OFF 循环无卡死。
- 1000 次亮灭状态循环无计数溢出或状态漂移。
- 非运动 SCREEN_OFF render 降频目标至少 90%，heart poll 降频目标至少 80%；这里只验证
  policy 数值，不声明真实功耗或实际调用次数。
- 空指针、非法 enum、无效 timeout/brightness、monotonic rollback。

测试入口使用 GCC/Clang/MSVC C11 严格警告；生产源行覆盖率目标 `>=90%`。

```powershell
python tests/test_power_policy.py
git diff --check
```

## 交付

结果文档必须记录 config 默认值、状态图、wake 优先级、每状态 policy 表、异常时间语义、
测试/coverage，以及 W1-I 未来如何将 snapshot 映射到 platform 和 sensor 调度。

```powershell
git add -- <仅本任务允许文件>
git commit -m "Add deterministic power policy core"
git push -u origin codex/w1-q5-power-core
```

不要合并，不要接 runtime/platform，不要宣称实际省电或真机功耗完成。
