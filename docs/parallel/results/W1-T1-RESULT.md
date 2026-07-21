# W1-T1 Result: watch face registry and Lotus migration

状态：Q2 registry/Lotus slice ready for integration。本文不表示 Q2 总 Gate 完成。

## 基线与分支

- 基线：`927001a772a4431ae1cf74b745d9abdb884cd336`
- 分支：`codex/w1-q2-face-registry`
- worktree：`smart-band-wt-w1-q2`

## API

公共头文件 `include/smart_band_watch_face.h` 提供：

- 静态 descriptor registry：`count`、`at`、`find`、`default`。
- 固定 256 字节、按指针和 `long double` 对齐的 context storage；不使用堆分配。
- `mount`、`render`、`set_visible`、`root`、`unmount` 生命周期。
- 当前唯一 descriptor 为 `SMART_BAND_WATCH_FACE_LOTUS` / `Lotus`。
- 后续增加 Activity 或 Minimal 时只需增加实现和 registry 项；`app_lvgl.c` 不需要新增 face-specific switch。

## 对象所有权

- `smart_band_ui_t.watch_face` 唯一持有通用实例和固定 context storage。
- `smart_band_lotus_context_t` 唯一持有 Lotus page 及其所有标签、电池和卡片对象指针。
- Heart 和 Steps 对象仍由 `smart_band_watch_pages_t` 持有；其旧 Face 字段和入口已删除。
- Lotus mount 任一创建点失败时删除已创建的 page 子树并清空 context；registry 再清空实例，因此可立即重试。
- destroy 和主 UI 后续创建失败路径都会先 unmount Lotus；不会留下重复常驻对象。

## 旧函数到新模块映射

| 旧入口/所有权 | 新入口/所有权 |
| --- | --- |
| `smart_band_watch_pages_build_face` | `lotus_mount` via `smart_band_watch_face_mount` |
| `smart_band_watch_pages_render_face` | `lotus_render` via `smart_band_watch_face_render` |
| `watch_pages.face_*` | `smart_band_lotus_context_t` 私有字段 |
| `set_page_visible(face_page, ...)` | `smart_band_watch_face_set_visible` |
| `enable_touch_navigation_tree(face_page)` | `smart_band_watch_face_root` 后复用原导航挂载 |
| 根对象递归删除 | `smart_band_watch_face_unmount`，并保留父根兜底删除 |

Lotus 的 compact/framed 坐标、尺寸、颜色、文字、数据格式和页面位置均从旧实现原样迁移。页面顺序仍为 Face -> Heart -> Steps -> Apps，Lotus 子树继续挂载原横滑事件。

## 测试结果

执行日期：2026-07-21（Asia/Shanghai）。

| 命令 | 结果 |
| --- | --- |
| `python tests/test_watch_face_registry.py` | PASS；registry 数量/唯一 ID/越界/未知 ID/默认 Lotus、空指针、compact/framed Lotus 全创建失败扫点、清理与重试通过 |
| `python tests/test_ui_compile.py` | PASS；MSVC `/W4 /WX`；compact/framed 主 UI 全创建失败扫点、timer 重试、页面顺序、step goal、8 个 app mount 失败扫点、1000 次 create/navigation/destroy 通过，object/event/timer 零净增长 |
| `python tests/test_runtime_core.py` | PASS；production central runtime tests passed |
| `python tests/test_emulator_smoke.py` | PASS（4 passed, 1 skipped）；POSIX PTY 用例在 Windows 预期 skip |
| `python tests/test_q0_baseline.py` | PASS（14 passed, 1 skipped）；POSIX signal cleanup 用例在 Windows 预期 skip |
| `python tests/test_native_e2e.py` | PASS（13 passed, 1 skipped）；POSIX attributed-process cleanup 用例在 Windows 预期 skip |
| `git diff --check` | PASS |

本机未找到 GCC/Clang、OpenCppCoverage 或 Visual Studio Code Coverage console，未生成可审计的行覆盖率百分比。不得把上述行为覆盖或 Windows skip 写成 native coverage 通过。

## W1-I integration requests

1. 在 T1-T5 全部停止写入并交付 commit 后集成本分支；处理其他分支对共享构建入口的需求。
2. 统一运行一次远端 openvela/full native build。
3. 对 `docs/evidence/q1v-native-e2e-watch-face-20260720.png` 使用审阅后的动态区域 mask，完成 Lotus 双分辨率精确像素回归。
4. 集成后再次运行 host CI、coverage 总表和 native 双分辨率门禁。

## 未运行门禁与已知风险

- 未运行远端 openvela build；按任务边界留给 W1-I。
- 未运行 Lotus native 双分辨率动态 mask 精确像素回归；源码和 fake LVGL 回归不能替代像素证据。
- 未生成 C 行覆盖率百分比；行为失败扫点和 soak 已通过，但 coverage 总表仍由 W1-I 负责。
- Activity Rings、Minimal Digital、picker、长按、settings 持久化和 100 次用户可见表盘切换均未实现；Q2 总 Gate 仍为未完成。
