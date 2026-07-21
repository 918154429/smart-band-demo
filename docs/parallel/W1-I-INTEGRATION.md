# W1-I：第一波汇总接线与门禁

本文件用于单独的集成对话。只有 W1-T1 至 W1-T5 都停止写入、push 完成并给出结果文档与
commit SHA 后才能启动。它不与五个实现对话并发。

## 身份与目标

你是第一波唯一 integration owner。先审计五个分支是否遵守文件所有权，再按固定顺序汇入
一个 integration branch，统一更新 build/CI/coverage，运行一次完整 Host/Browser/openvela/
native 门禁。不要在集成阶段扩展功能或替叶子任务重写架构。

## 启动前输入

必须收齐：

| 任务 | 分支 | 必需结果文件 |
| --- | --- | --- |
| T1 | `origin/codex/w1-q2-face-registry` | `docs/parallel/results/W1-T1-RESULT.md` |
| T2 | `origin/codex/w1-q3-workout-core` | `docs/parallel/results/W1-T2-RESULT.md` |
| T3 | `origin/codex/w1-q4-notification-core` | `docs/parallel/results/W1-T3-RESULT.md` |
| T4 | `origin/codex/w1-q5-power-core` | `docs/parallel/results/W1-T4-RESULT.md` |
| T5 | `origin/codex/w1-q6-sync-protocol` | `docs/parallel/results/W1-T5-RESULT.md` |

任何分支缺 commit、测试结果或 integration request 时，不要猜测，退回对应任务补齐。

## worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-integration'
git -C $repo fetch origin
$base = git -C $repo rev-parse origin/master
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-integration $worktree $base
Set-Location $worktree
git status --short --branch
```

不得复用任一叶子 worktree，不得强推或 rebase 已发布分支。

## 第一步：所有权审计

对每个分支执行：

```powershell
git diff --name-status 927001a772a4431ae1cf74b745d9abdb884cd336..origin/<branch>
git show origin/<branch>:docs/parallel/results/<result-file>
```

检查事项：

- T2-T5 是否只新增任务 allowlist 文件，没有偷偷修改共享文件。
- T1 是否没有实现 Activity/Minimal/picker/settings 或修改 model/storage/runtime 语义。
- 是否包含生成物、临时 evidence、大二进制、凭据或与任务无关格式化。
- 每个结果文档中的测试结论能否由命令复现。

发现越界时不要在集成分支静默吸收，应要求叶子分支拆分干净 commit。

## 第二步：按固定顺序汇入

顺序固定：T1 -> T2 -> T3 -> T4 -> T5。使用结果文档列出的 commit SHA 做普通
`git cherry-pick`；不要按目录复制文件，不要 squash 掉失败修复历史，也不要把叶子分支
直接合并到 master。

发生冲突时：

1. 先判断是否违反 allowlist。
2. 共享 build/CI 冲突由本对话统一解决。
3. 业务 contract 冲突退回叶子任务，不在集成时临时重新设计。

## 第三步：统一接线

本对话独占以下共享修改：

- `openvela_app/smart_band/CMakeLists.txt` 与 `Makefile`
- `.github/workflows/host-tests.yml`
- `tests/test_core_coverage.py`
- `tests/test_ui_compile.py`
- shell source parity 检查
- 必要的测试 source list
- 第一波 evidence、路线图与 handoff

要求：

- Q2 registry/Lotus 必须进入实际 UI 路径。
- T2-T5 纯模块必须加入 CMake/Make 的 compile source list，并进入独立 Host/coverage 门禁，
  以证明 NuttX toolchain 可编译；但不得在本轮接入 runtime/event/UI/platform，也不得暴露
  半成品用户功能。未被调用的对象由正常链接规则处理，不为“看起来已接入”添加伪调用。
- 每个 T2-T5 test wrapper 在 GitHub 使用 GCC、Clang、MSVC 矩阵。
- `test_core_coverage.py` 的结构边界加入所有新纯 C 源，每个新源独立 `>=90%`，总体仍
  `>=85%`。
- 完整 UI compile 必须编译所有加入 openvela source list 的生产源。
- CMake、Make、UI compile、coverage 与 shell parity 的 source 集不得漂移。

## 第四步：本地门禁

先运行五个任务入口，再运行全量既有入口：

```powershell
python tests/test_watch_face_registry.py
python tests/test_workout_core.py
python tests/test_notification_core.py
python tests/test_power_policy.py
python tests/test_sync_protocol.py
python tests/test_watch_model.py
python tests/test_time_apps.py
python tests/test_app_runtime.py
python tests/test_runtime_core.py
python tests/test_storage_core.py
python tests/test_app_logic.py
python tests/test_ui_compile.py
python tests/test_emulator_smoke.py
python tests/test_q0_baseline.py
python tests/test_native_e2e.py
bash scripts/test_reproduce_failure.sh
npm ci
npm run test:browser
git diff --check
```

测试文件实际命名若经叶子结果文档批准略有不同，使用其真实入口并在集成证据记录映射，
不要通过复制第二套 wrapper 规避。

## 第五步：单次远端与 native Gate

只在源码、测试和文档候选固定后执行一次：

1. PR Host/Browser 全绿。
2. 触发 `openvela-nightly.yml` 的 fresh fixed build/native smoke。
3. 记录 run ID、source SHA、artifact digest、NuttX/runtime image hashes。
4. 对 Lotus 初始截图与
   `docs/evidence/q1v-native-e2e-watch-face-20260720.png` 做审阅后的动态区域 mask 比较，同时
   断言结构化 page ID、关键文字、一次横滑到 Heart Rate。
5. 保留所有失败 run；修复必须使用新 commit 和新 fresh run，不能覆盖失败证据。

不得把 Browser 当 native 证据，不得把默认空 storage path 当真机 durability，不得启动五份
并行 openvela build。

## Gate 与文档口径

第一波通过后：

- 可记录 Q2 registry/Lotus slice 完成，但 Q2 总 checkbox 仍保持未完成。
- 可记录 Q3-1/Q4-1/Q5-1/Q6-1 的 pure core ready，但 Q3-Q6 产品 Gate 全部保持红色。
- 下一波优先拆 Q2 Activity、Minimal、picker/settings；再分别启动 Q3-Q6 adapter/UI/service。
- 更新 `NEXT_SESSION_HANDOFF.md` 时必须保留目标板、烧录、远端 `/data` 和证据边界。

## 提交、PR 与合并

集成分支允许提交、push、创建 PR。PR 描述必须列出五个输入 commit、所有权审计、测试矩阵、
coverage、native/openvela 证据和未完成 Gate。只有全部必需检查通过后才能普通 merge；禁止
force-push、history rewrite、远端分支删除和 release。

合并后更新 master baseline，等待 merge 后 Host/Browser 通过并确认工作树干净。叶子分支
保留，除非用户另行明确授权删除。
