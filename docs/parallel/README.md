# smart-band-demo 第一波并行开发任务包

更新时间：2026-07-21（Asia/Shanghai）

代码基线固定为：

```text
927001a772a4431ae1cf74b745d9abdb884cd336
```

该提交是 Q1-S 合并基线文档提交，工作树和 GitHub Host/Browser CI 均为绿色。本目录的
任务说明属于协调材料；五个实现分支仍从上述代码基线创建，以免启动时间不同造成漂移。

## 为什么这样拆

第一波同时启动五个实现对话，但只有一个对话可以修改现有 UI、构建入口和共享测试。
其余四个对话只新增互相独立的纯 C 模块和独立测试，不接 runtime、不碰 LVGL、不改公共
事件或平台接口。这样能并行产出后续核心逻辑，同时保持 Q2 是唯一用户可见的主线 Gate。

Q3-Q6 对话在第一波只能交付“可集成核心”，不得宣称对应产品 Gate 完成。所有共享接线、
CI、coverage、openvela 和 native 验证由集成对话统一完成，避免五次昂贵构建和多人同时改
`CMakeLists.txt`、`host-tests.yml`、`runtime.c`。

## 对话分配

| ID | 任务 | 分支 | 独立 worktree | 可立即启动 |
| --- | --- | --- | --- | --- |
| W1-T1 | Q2 face registry + Lotus 原样迁移 | `codex/w1-q2-face-registry` | `smart-band-wt-w1-q2` | 是 |
| W1-T2 | Q3 step normalizer + workout 纯模型 | `codex/w1-q3-workout-core` | `smart-band-wt-w1-q3` | 是 |
| W1-T3 | Q4 notification 队列纯模型 | `codex/w1-q4-notification-core` | `smart-band-wt-w1-q4` | 是 |
| W1-T4 | Q5 power policy 纯模型 | `codex/w1-q5-power-core` | `smart-band-wt-w1-q5` | 是 |
| W1-T5 | Q6 sync protocol codec | `codex/w1-q6-sync-protocol` | `smart-band-wt-w1-q6` | 是 |
| W1-I | 汇总接线、CI、coverage、native | `codex/w1-integration` | `smart-band-wt-w1-integration` | 否，等待 T1-T5 |

对应任务文件：

- [W1-T1 Q2 face registry + Lotus](W1-T1-Q2-FACE-REGISTRY-LOTUS.md)
- [W1-T2 Q3 workout core](W1-T2-Q3-WORKOUT-CORE.md)
- [W1-T3 Q4 notification core](W1-T3-Q4-NOTIFICATION-CORE.md)
- [W1-T4 Q5 power policy core](W1-T4-Q5-POWER-POLICY-CORE.md)
- [W1-T5 Q6 sync protocol](W1-T5-Q6-SYNC-PROTOCOL.md)
- [W1-I 集成与最终门禁](W1-I-INTEGRATION.md)

最多同时运行 T1-T5 五个实现对话。W1-I 不是第六个并发实现任务，只能在五个任务停止
写入并交付 commit 后启动。

## 硬性并行规则

1. 每个对话使用自己的 Git worktree，禁止在主仓库目录切换任务分支。
2. 五个分支都从精确 SHA `927001a...` 创建；不得在任务中途 `pull`、merge master 或
   rebase。需要上游修复时停下并交给集成对话处理。
3. T2-T5 只能新增自己任务文件。以下共享文件一律禁止修改：
   `app_lvgl.c`、`watch_model.*`、`smart_band_runtime.*`、`smart_band_event.h`、
   `smart_band_platform.*`、`CMakeLists.txt`、`Makefile`、`.github/**`、
   `tests/test_core_coverage.py`、`tests/test_ui_compile.py`、README、路线图和总交接。
4. T1 独占现有 UI/构建共享文件；T2-T5 不得触碰它们。
5. 每个任务必须提交一个独立结果文件：
   `docs/parallel/results/W1-Tx-RESULT.md`。不得共同修改 `NEXT_SESSION_HANDOFF.md`。
6. 实现对话可以 commit、push 对应分支，但不得自行合并、删除分支、强推或改写历史。
7. 不运行五份 openvela/full native build。远端与 native 只由 W1-I 在源码冻结后运行一次。
8. 任何任务发现必须修改禁区文件才能继续时，应停止并在结果文档记录 integration request，
   不得越权修改。

## worktree 创建

每个任务文件带有自己的 PowerShell 命令。创建前统一检查：

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
git -C $repo status --short --branch
git -C $repo worktree list
git -C $repo cat-file -e '927001a772a4431ae1cf74b745d9abdb884cd336^{commit}'
```

主仓库必须干净。目标路径或分支已存在时不要重复创建，先确认它属于同一任务。

## 统一交付格式

每个实现对话结束前必须：

1. 运行任务文件要求的测试并记录完整结果。
2. 执行 `git diff --check`。
3. 仅提交任务允许的文件与唯一结果文档。
4. push 到表格指定分支，不创建 release，不合并。
5. 最终回复给出 branch、commit SHA、文件清单、测试、覆盖率、integration request、已知风险。

集成对话只接受可复现的 commit，不接受散落在聊天中的未提交补丁。

## 真实完成口径

- T1 完成只表示 Lotus registry 迁移准备就绪，Q2 总 Gate 仍需 Activity、Minimal、picker、
  settings、100 次切换和 native 双分辨率。
- T2-T5 完成只表示纯逻辑 contract 通过，不表示 Q3-Q6 已接入产品。
- 只有 W1-I 全门禁通过并合入 master 后，才能在路线图记录第一波完成情况。
