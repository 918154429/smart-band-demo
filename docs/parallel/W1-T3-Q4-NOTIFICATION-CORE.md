# W1-T3：Q4 notification 队列纯模型

把本文件全文作为一个独立对话框的任务指令。该对话只做固定容量通知模型和测试。

## 身份与目标

实现无堆分配、可确定性重放的 Call/SMS/App/System 通知队列，冻结去重、满载、已读、删除、
action、DND 与 workout 非阻断决策。不得创建通知中心 UI、overlay、来电页、震动或 runtime
adapter。

完成仅表示 Q4 notification core ready，C Gate 仍为红色。

## 冻结基线与 worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-q4'
$base = '927001a772a4431ae1cf74b745d9abdb884cd336'
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-q4-notification-core $worktree $base
Set-Location $worktree
git status --short --branch
```

## 唯一可修改文件

- 新增 `openvela_app/smart_band/include/smart_band_notification_model.h`
- 新增 `openvela_app/smart_band/logic/notification_model.c`
- 可新增纯逻辑 deterministic injector：`notification_demo.c/.h`
- 新增 `tests/notification_core_test.c`
- 新增 `tests/test_notification_core.py`
- 可新增任务专属 coverage runner
- 新增 `docs/parallel/results/W1-T3-RESULT.md`

禁止修改现有 `smart_band_event.h`、runtime、haptic、platform、LVGL、构建入口、CI、总 coverage、
README、路线图和交接。未来 event adapter 由 W1-I 或 Q4 集成切片完成。

## 模型 contract

- 固定容量在头文件中显式定义；title/body/source 使用固定数组并始终 NUL 结尾。
- 类型至少 Call、SMS、App、System；保存 ID、priority、source、title、body、wall timestamp、
  read/dismissed/action state。
- 相同 ID 重复到达必须幂等；明确哪些字段允许更新，不能生成第二条。
- 满载时优先淘汰最旧、低优先级、已读项；高优先级未处理来电不能被普通 App 通知淘汰。
- 当没有可合法淘汰项时返回 typed full/protected 结果，不覆盖关键通知。
- action 至少覆盖 read、dismiss、accept、reject；非法 action 不改变记录。
- DND/workout policy 只输出 abstract presentation decision，例如 center-only、overlay、
  full-screen、haptic-kind、wake-request；不调用平台接口。
- 确定性 injector 使用显式 seed/sequence，不读取随机设备或墙钟。

## 必测矩阵

- 1000 条 Call/SMS/App/System 混合输入，无越界、崩溃或 ID 重复。
- 队列未满/刚满/满载、全高优先级、全未读、混合已读的淘汰顺序。
- 重复 ID、timestamp 相同、ID 边界、超长 source/title/body、空字符串。
- accept/reject 只对 call 合法；重复 action 幂等。
- DND、workout、普通和高优先级组合的 presentation decision。
- 遍历、按 ID 查询、删除头/中/尾后顺序稳定。
- 空指针、非法 enum 与容量边界保持状态不变。

测试入口使用 GCC/Clang/MSVC C11 严格警告；生产源行覆盖率目标 `>=90%`。

```powershell
python tests/test_notification_core.py
git diff --check
```

## 交付

结果文档必须包含容量、字符串上限、淘汰比较顺序、重复 ID 更新规则、action 状态表、
presentation decision 表、测试与 coverage 结果，以及未来 event/haptic/UI adapter 请求。

```powershell
git add -- <仅本任务允许文件>
git commit -m "Add fixed-capacity notification core model"
git push -u origin codex/w1-q4-notification-core
```

不要合并，不要接 overlay/haptic/runtime，不要修改共享文件。
