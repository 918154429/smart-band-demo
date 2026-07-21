# W1-T2：Q3 step normalizer 与 workout 纯模型

把本文件全文作为一个独立对话框的任务指令。该对话只做纯 C，不接 UI/runtime/storage。

## 身份与目标

实现可独立测试的步数归一化器和 Walk/Run workout 状态机，为 Q3 后续 service/UI/history
提供稳定 contract。必须解决 counter reset、回绕、source switch、stale、异常跳变、暂停、
恢复与 24 小时累计。

完成仅表示 Q3-1 core ready，不表示后台 service、checkpoint、页面或历史已完成。

## 冻结基线与 worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-q3'
$base = '927001a772a4431ae1cf74b745d9abdb884cd336'
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-q3-workout-core $worktree $base
Set-Location $worktree
git status --short --branch
```

## 唯一可修改文件

- 新增 `openvela_app/smart_band/include/smart_band_step_normalizer.h`
- 新增 `openvela_app/smart_band/include/smart_band_workout_model.h`
- 新增 `openvela_app/smart_band/logic/step_normalizer.c`
- 新增 `openvela_app/smart_band/logic/workout_model.c`
- 新增 `tests/workout_core_test.c`
- 新增 `tests/test_workout_core.py`
- 可新增任务专属 coverage runner，例如 `tests/test_workout_core_coverage.py`
- 新增 `docs/parallel/results/W1-T2-RESULT.md`

除此之外一律不改。尤其禁止修改 `watch_model.*`、runtime、event、storage、LVGL、构建入口、
CI、总 coverage、README、路线图和交接。

## 纯模型 contract

### Step normalizer

- 输入显式包含 source identity、raw counter、availability/freshness 和 monotonic timestamp。
- 同一硬件 counter 正常递增时产生非负 delta。
- reset、回绕和异常大跳必须可区分且不把跳变量计入 workout。
- source 从 sensor/derived/simulation 切换时重新建立 baseline，不产生负数或重复累计。
- stale/unavailable 不增加步数；恢复后先 rebase，再从下一有效样本累计。
- 所有阈值必须由初始化 config 固定，不能隐藏 magic number。
- 输出包含 delta、累计值和原因/质量标志，便于未来 history 记录 provenance。

### Workout model

- 模式至少 Walk、Run；状态至少 idle、countdown、active、paused、finished、aborted、
  recovery-confirmation。
- 使用显式 monotonic milliseconds，不读取系统时钟，不依赖 LVGL tick。
- pause 期间 active duration、steps、distance、calories 和 heart aggregates 不增加。
- distance/calorie/pace 使用确定性的整数或 fixed-point 规则，禁止依赖平台浮点舍入差异。
- 心率保存 current/min/max/weighted average 所需累计；无有效心率时不得用 0 冒充测量。
- 导出 checkpoint snapshot 所需纯数据，但本任务不编码、不写 storage。
- 非法状态转换返回类型化结果且保持状态不变。

## 必测矩阵

- 正常递增、32/64-bit 边界、reset、wrap、负向/超阈值跳变。
- sensor -> derived -> simulation -> sensor 的切换与 stale 恢复。
- 三秒 countdown，start/pause/resume/finish/abort 全路径。
- pause 不累计；finish 后不可继续更新；recovery-confirmation 不静默恢复 active。
- 24 小时加速运行无溢出；monotonic 大步进和边界值可预测。
- Walk/Run 配置隔离；零步数、无心率、极端但合法输入。
- 空指针、非法 enum、重复 command 不破坏状态。

测试入口必须在 GCC/Clang/MSVC 使用 C11 严格警告；新生产源文件行覆盖率目标各 `>=90%`。
本地缺少 GCC/gcov 时如实记录，留给 W1-I 在 Linux CI 验证，不能伪造覆盖率。

```powershell
python tests/test_workout_core.py
git diff --check
```

## 交付

结果文档必须写出：所有 enum/单位、counter wrap 假设、跳变阈值、fixed-point 比例、合法状态
转移表、测试结果和未来 adapter 需要把哪些 runtime/sensor 字段映射进来。

```powershell
git add -- <仅本任务允许文件>
git commit -m "Add normalized step and workout core models"
git push -u origin codex/w1-q3-workout-core
```

不要修改共享文件，不要合并，不要开始 workout service、UI 或 history。
