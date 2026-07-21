# smart-band-demo 下一会话交接

更新时间：2026-07-21（Asia/Shanghai）

> 复赛工程以根目录 `FINALS_TOP_TIER_ROADMAP.md` 为主路线。Q0、Q1-V、Q1-C、Q1-S 与
> W1 汇总 Gate 已全绿；下一主线切片做 Q2 第二段：Activity、Minimal、picker/settings。

## 1. 仓库、权限与边界

- 本地仓库：`E:\C_Moved_From_C\Users\Lenovo\Desktop\schoolwork\smart-band-demo`
- GitHub：<https://github.com/918154429/smart-band-demo>
- 默认分支：`master`
- Q1-S PR：[PR10](https://github.com/918154429/smart-band-demo/pull/10)，已正常合并。
- W1 integration PR：[PR11](https://github.com/918154429/smart-band-demo/pull/11)。
- Q1-S 功能提交：`0547953`；NuttX 符号冲突修复：`a9d5a43`；最终证据提交：`9172aef`。
- 当前 master 合并基线：`7a99c8a19049d4a7f06538424e10df66e0a3d2ee`。
- 用户确认其对主仓库拥有完整管理权并承担责任，长期授权 Codex 按持续开发需要修改、
  提交、推送、创建/更新/合并 PR。强制推送、历史改写、删除远端分支/标签和正式 release
  仍需当次明确指示。
- 当前会话允许子智能体，最多五个，且最多二级委派；额度和指令不自动延续到新会话。
- 视频、比赛平台提交和正式发布不属于工程路线。
- 开发板型号/revision 未确认；未经逐次明确批准不得烧录、写 flash 或更改 bootloader。

若使用远端 Ubuntu，所有新工作必须留在 `/data`。不得访问 `/data/codex-audit`、
`/data/lxd-storage`、`/data/lost+found` 或旧 `/home/ubuntu/...` 项目树。

## 2. 已完成基线

PR1-PR10 已完成生命周期、传感器来源、应用 runtime、响应式 UI、固定复现、真实 openvela
build/native smoke、独立 simulator 证据与 Q1-S 存储底座。当前 master 基线为
`7a99c8a19049d4a7f06538424e10df66e0a3d2ee`；此前 Q1-C merge commit 为
`9f1f00fea130f7ccf4d79d89652a91e8fdfe0d13`。

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

## 4. W1 第一波完成

五个叶子提交已通过所有权审计并按 T1 -> T5 汇入。完成范围：

- Q2 registry/Lotus lifecycle 与实际 UI 路径。
- Q3-1 step normalizer 与 Walk/Run pure model。
- Q4-1 fixed notification queue/demo pure model。
- Q5-1 ACTIVE/DIMMED/SCREEN_OFF pure policy。
- Q6-1 stateless v1 envelope codec、CRC 与 golden vector。

独立 Linux coverage 在 `/data/smart-band-w1-integration-20260721`：overall
`94.0% (2945/3134)`，六个新生产源分别 `97.6%` 到 `100%`，全部 `>=90%`。最终 Host
run `29816173199`、Browser run `29816173171` 全绿。

最终 fixed openvela run：
<https://github.com/918154429/smart-band-demo/actions/runs/29816300149>

- source：`298a5c0aa45968b341739c3d1c5a3c103f84e2eb`
- artifact：`openvela-fixed-release-29816300149`，`23,861,899` bytes
- digest：`sha256:554fac66d9165ad48ae765a25adef855527eb762c78a8c3705076bcf97a11823`
- NuttX：`66,114,392` bytes，SHA-256
  `61aa4877b597bd956e2ab4b34a2dcc913940979362279ffcf9c94f13a39ec051`
- compact `336x480` mask 外 91.13% 像素零差异。
- framed `1280x800` mask 外 96.64% 像素零差异；第一次横滑进入 `heart_rate`，
  `104 bpm`/`Source / Sensor` 与 cleanup 全通过。

完整说明与结构化结论：

- `docs/w1-first-wave-integration-20260721.md`
- `docs/evidence/w1-gate-summary-20260721.json`
- `docs/evidence/w1-native-journey-20260721.json`

失败 run `29812790986`、`29814087721`、`29815208311` 及对应 PNG/JSON 均保留。

## 5. 证据边界

- host memory/file fault model 不是真实掉电介质，不证明目标板文件系统原子性、目录项持久化
  或 power-loss durability。
- 最终 openvela `.config` 的 `CONFIG_LVX_DEMO_SMART_BAND_STORAGE_PATH=""`，因此 native
  smoke 证明生产源码编译链接和默认 no-op 启动，不证明非空路径的写入/重启恢复。
- storage load 在 UI tree 创建前同步执行；已证明 returned error 不会中止初始化，但没有
  证明慢或永久阻塞 backend 的时延隔离。backend 必须有界、可响应。
- degraded 状态在 store 中可观测，本切片未添加用户提示或运行日志遥测。
- Lotus compact/framed 已证明；其他页面/应用 native 像素、sensor stale/TTL、长时 soak 和
  所有真机能力仍未证明。

## 6. 下一独立切片：Q2 第二段

1. 在现有 registry/lazy lifecycle 上新增 Activity 与 Minimal，Lotus 保持默认。
2. 实现 picker、长按入口、手势仲裁和 100 次用户可见切换压力。
3. 复用 Q1-S store 保存 selected face，覆盖损坏/旧版本/写失败回退。
4. 补 GCC/Clang/MSVC、fake LVGL failure sweep、compact/framed native picker journey。

本切片不接 Workout、History、notification、power 或 BLE。Q3-Q6 下一 adapter/service/UI
对话必须消费已冻结 pure core，不另写第二套模型。

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
