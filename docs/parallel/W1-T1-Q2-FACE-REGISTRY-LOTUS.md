# W1-T1：Q2 face registry 与 Lotus 原样迁移

把本文件全文作为一个独立对话框的任务指令。不要同时执行其他 W1 任务。

## 身份与目标

你是第一波唯一的 UI/shared-files owner。只建立 `watch_face_ops_t`、表盘 descriptor/registry，
并把当前表盘原样迁入独立 Lotus 实现。用户可见布局、数据、页面顺序、横滑、颜色和文字
必须保持不变。

本任务不实现 Activity Rings、Minimal Digital、picker、长按、settings 持久化或 Q3-Q6。
完成后只能标记“Q2 registry/Lotus slice ready for integration”，不得勾选 Q2 总 Gate。

## 冻结基线与 worktree

```powershell
$repo = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
$worktree = 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-wt-w1-q2'
$base = '927001a772a4431ae1cf74b745d9abdb884cd336'
git -C $repo worktree list
git -C $repo worktree add -b codex/w1-q2-face-registry $worktree $base
Set-Location $worktree
git status --short --branch
```

若分支或目录已存在，先核对 `git worktree list`，不要再次执行 `-b`。

## 可修改范围

- 新增 `openvela_app/smart_band/include/smart_band_watch_face.h`
- 新增 `openvela_app/smart_band/ui/lvgl/faces/**`
- 新增 registry 的内部 `.c/.h`
- `openvela_app/smart_band/ui/lvgl/watch_pages.c/.h`
- `openvela_app/smart_band/app_lvgl.c`
- `openvela_app/smart_band/CMakeLists.txt`
- `openvela_app/smart_band/Makefile`
- `tests/fake_lvgl/**`、`tests/ui_compile_smoke.c`、`tests/test_ui_compile.py`
- 新增 `tests/watch_face_registry_test.c`、`tests/test_watch_face_registry.py`
- 可新增任务专属 coverage runner
- 新增 `docs/parallel/results/W1-T1-RESULT.md`

禁止修改 runtime、watch model 数据语义、storage、event、platform、现有 app、Host workflow、
coverage 总表、README、路线图和 `NEXT_SESSION_HANDOFF.md`。

## 设计约束

1. registry 必须是固定容量/静态 descriptor，不使用堆分配。
2. ops 至少覆盖构建或 mount、render、可见性/卸载清理所需生命周期；失败后可完整清理并重试。
3. Lotus 的 LVGL object context 必须从通用 `smart_band_watch_pages_t` 中形成清晰所有权，不能
   复制出两套同时常驻的现有表盘对象。
4. 当前只有 Lotus 一个注册项，但 API 必须允许下一波新增 Activity 和 Minimal，而无需再改
   `app_lvgl.c` 的 face-specific switch。
5. 不增加 `smart_band_state_t` 字段，不改健康指标含义，不引入 settings schema。
6. 页面横滑仍是 Face -> Heart -> Steps -> Apps；Lotus 内不得吞掉现有横滑。
7. compact/framed 两种布局保持现状；不借迁移重做视觉样式。
8. 所有创建失败路径必须释放已创建对象，随后重试成功。

## 测试先行

至少新增并通过：

- registry 数量、ID 唯一、越界/未知 ID、默认 Lotus descriptor。
- Lotus build/render 的空指针与创建失败扫点。
- compact/framed 主 UI 全创建调用失败后清理与重试。
- 1000 次 create/navigation/destroy 后 object/event/timer 零净增长。
- 现有页面顺序、step goal、app mount 与 Heart/Steps render 回归。

运行：

```powershell
python tests/test_watch_face_registry.py
python tests/test_ui_compile.py
python tests/test_runtime_core.py
python tests/test_emulator_smoke.py
python tests/test_q0_baseline.py
python tests/test_native_e2e.py
git diff --check
```

Windows 的 POSIX skip 必须保持为预期 skip，不得把 skip 写成 native 通过。无需在本任务运行
远端 openvela build；native Lotus 精确像素回归由 W1-I 对
`docs/evidence/q1v-native-e2e-watch-face-20260720.png` 使用审阅后的动态区域 mask 验证。

## 交付

`docs/parallel/results/W1-T1-RESULT.md` 必须记录：API、对象所有权、旧函数到新模块映射、
测试结果、未运行的 native 门禁和 W1-I 需要完成的接线事项。

提交并推送：

```powershell
git status --short
git diff --check
git add -- <仅本任务允许文件>
git commit -m "Add watch face registry and migrate Lotus"
git push -u origin codex/w1-q2-face-registry
```

不要合并分支，不要更新总路线图，不要继续实现第二款表盘。
