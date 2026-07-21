# smart-band-demo 下一会话交接

更新时间：2026-07-21（Asia/Shanghai）

> 复赛工程以根目录 `FINALS_TOP_TIER_ROADMAP.md` 为主路线。Q0、Q1-V、Q1-C、Q1-S
> 已全绿；下一独立切片只做 Q2 第一段：表盘 registry 与现有 Lotus 表盘迁移。

## 1. 仓库、权限与边界

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub：<https://github.com/918154429/smart-band-demo>
- 默认分支：`master`
- Q1-S PR：[PR10](https://github.com/918154429/smart-band-demo/pull/10)
- Q1-S 功能提交：`0547953`；NuttX 符号冲突修复：`a9d5a43`。
- 本文件写入时 PR10 尚待最终证据文档提交和正常合并；合并后必须把此处更新为实际
  merge commit，不能把 feature commit 当成 master 基线。
- 用户确认其对主仓库拥有完整管理权并承担责任，长期授权 Codex 按持续开发需要修改、
  提交、推送、创建/更新/合并 PR。强制推送、历史改写、删除远端分支/标签和正式 release
  仍需当次明确指示。
- 当前会话允许子智能体，最多五个，且最多二级委派；额度和指令不自动延续到新会话。
- 视频、比赛平台提交和正式发布不属于工程路线。
- 开发板型号/revision 未确认；未经逐次明确批准不得烧录、写 flash 或更改 bootloader。

若使用远端 Ubuntu，所有新工作必须留在 `/data`。不得访问 `/data/codex-audit`、
`/data/lxd-storage`、`/data/lost+found` 或旧 `/home/ubuntu/...` 项目树。

## 2. 已完成基线

PR1-PR9 已完成生命周期、传感器来源、应用 runtime、响应式 UI、固定复现、真实 openvela
build/native smoke 和独立 simulator 证据。Q1-C 既有 master 基线为
`5805fbd`，其中 Q1-C merge commit 为 `9f1f00fea130f7ccf4d79d89652a91e8fdfe0d13`。

Q0 正式 20 次冷启动与 Q1-V native fresh sensor 证据仍见：

- `docs/q0-q1v-baseline-20260720.md`
- `docs/evidence/q0-gate-summary-20260720.json`
- `docs/evidence/q1v-native-journey-20260720.json`

Q1-C central runtime、事件、时钟、能力、platform adapters 与 fake LVGL 证据见：

- `docs/q1c-runtime-platform-20260720.md`
- `docs/evidence/q1c-gate-summary-20260720.json`

## 3. Q1-S 已完成

### 格式与 store

- 固定 36-byte little-endian header，magic `SBST`、format `1.0`，最大 payload 512 bytes。
- 显式 record type、schema major/minor、64-bit generation、header/payload IEEE CRC32；
  不落盘裸 C struct，严格验证 frame 精确长度。
- 每种 record 使用两个显式 object ID。commit 读取两槽、写另一槽、flush、回读验证。
- 单槽损坏/缺失/不支持可回退；双槽损坏或同 generation 冲突降级默认值；不确定 read
  error 在任何 write 之前终止；`UINT64_MAX` generation 拒绝继续提交。
- migration 使用类型化 OK/unsupported/buffer-too-small/invalid 结果并校验输出长度。

### backend 与 runtime

- memory backend 固定 `16 x 4096` bytes，无堆分配。
- file backend 映射 `object-%08x.bin`，单对象 4096 bytes，上限 16 个 dirty object，
  `fsync/_commit` flush，不依赖 rename。
- fault plan 支持第 N 次 EIO、ENOSPC、EROFS、短写、截断、腐坏和 interrupted mixed image。
- runtime 拥有 store，启动加载固定 runtime-checkpoint A/B record；存储错误不会中止 UI
  初始化。`LVX_DEMO_SMART_BAND_STORAGE_PATH` 为空时使用 no-op，非空目录必须预先存在。
- 本切片没有新增任何 A-G 用户功能或历史 payload schema。

### 覆盖率与故障矩阵

独立 Linux 目录：

```text
/data/smart-band-q1s-20260721T132034CST
```

- 最终 coverage snapshot：`source-v6`
- snapshot archive SHA-256：
  `1c8fbe9a27793133ce604c29b2472c75509f46313e85acb9c5f07e7e056792ad`
- coverage log：`evidence/coverage-v6.log`
- overall `92.2% (2125/2305)`；codec `100%`、store `95.9%`、fault `93.3%`、memory
  `93.5%`、file `91.4%`，每个 Q1-S 生产源文件均 `>=90%`。
- 160 个逐字节 crash 切点覆盖空/已占用 inactive slot、generation zero、short write、
  interrupted mixed image；另覆盖 EIO、ENOSPC、EROFS、truncate、CRC、迁移与读前写保护。
- v2-v5 的 POSIX 声明、coverage runner 参数和 file coverage 失败证据已保留。

### GitHub/openvela 最终证据

第一次 fresh run `29805084515` 在 `0547953` 上失败：NuttX 已声明 `file_read/file_write`，
与 file backend 静态回调冲突。失败 artifact 保留；修复提交 `a9d5a43` 改用
`storage_file_*` 回调名。

最终 run：<https://github.com/918154429/smart-band-demo/actions/runs/29806148523>

- source commit：`a9d5a4326063c75dabea4d61c31152ba981b15a7`
- 214 个 openvela project 全部解析为固定 SHA。
- build、镜像链接校验、native smoke、清理、artifact 上传全部通过。
- artifact：`openvela-fixed-release-29806148523`，`23,615,392` bytes。
- artifact digest：
  `sha256:cd24812b5eb4c681a5b03a27a06082a777edbd1d4460521dd9406faa02a3f9e4`
- NuttX：`66,073,888` bytes，SHA-256
  `b6605449990f01ab48c747c5e605ad4136eac7f8e1ef2dd6eb9831e282dff0dc`。
- `.config` SHA-256：`e15bd57b33f7ea33132fd2bfde144b6bdd07291a1795de147dabb8b448d06e10`。
- `vela_system.bin`：
  `c432a814d04355da298268dc6fc6caafecfbe7cd7ce0fe799deed8949c8614a6`。
- `vela_data.bin`：
  `9b4405cb8a1ab36f0cc852300d7810957f218566fd5072d4a80b39d667d685ee`。
- native `UI ready`：`0.303s`；PID 13 在 5 秒检查后仍为 13；清理通过。
- Host run `29806142043` 与 Browser run `29806142041` 全绿。

紧凑证据：

- `docs/q1s-versioned-storage-20260721.md`
- `docs/evidence/q1s-gate-summary-20260721.json`

## 4. 证据边界

- host memory/file fault model 不是真实掉电介质，不证明目标板文件系统原子性、目录项持久化
  或 power-loss durability。
- 最终 openvela `.config` 的 `CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH=""`，因此 native
  smoke 证明生产源码编译链接和默认 no-op 启动，不证明非空路径的写入/重启恢复。
- storage load 在 UI tree 创建前同步执行；已证明 returned error 不会中止初始化，但没有
  证明慢或永久阻塞 backend 的时延隔离。backend 必须有界、可响应。
- degraded 状态在 store 中可观测，本切片未添加用户提示或运行日志遥测。
- native sensor stale/TTL、全页面/全应用像素、第二分辨率、长时 soak 和所有真机能力仍未证明。

## 5. 下一独立切片：Q2 第一段

只做表盘 registry 与现有 Lotus 表盘迁移：

1. 冻结现有 Lotus UI 的结构化状态、交互与 native golden。
2. 定义 `watch_face_ops_t`、descriptor/registry 和受 runtime 管理的 lifecycle。
3. 把现有表盘实现原样迁入 `face_lotus.c`，保持页面顺序、手势和渲染结果。
4. 使用 host/fake LVGL 覆盖 mount/unmount、失败回滚、重试和资源零净增长。
5. 用既有 native harness 证明 Lotus 零视觉与行为回归。

明确不做：Activity、Minimal、picker、settings UI、Workout、History、通知、power、BLE 或其他
A-G 功能。Q2 后续 settings 必须复用 Q1-S store，不建立第二套持久化。

下一会话开场：

```powershell
Set-Location 'E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo'
Get-Content -Raw FINALS_TOP_TIER_ROADMAP.md
Get-Content -Raw NEXT_SESSION_HANDOFF.md
git status --short
git log -3 --oneline
gh pr list --state open
```

开始实现前仍须读取用户最新指令；如有冲突，以最新指令为准。
