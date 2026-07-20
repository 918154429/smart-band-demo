# openvela 远端 native 模拟器试运行记录（2026-07-20）

## 验证范围

本次在独立 Ubuntu 24.04 Linux 主机上复用了固定版本的 openvela NuttX 构建产物，
重新准备固定 emulator 运行时，并通过真实 PTY/NSH 启动 `smart_band`。这不是浏览器
原型测试，也不是只检查 ELF 字符串。

远端主机资源为 4 核 CPU、31 GiB 内存；验证工作区为：

```text
/home/ubuntu/smart-band-sim-20260720-v1
```

## 固定输入

| 输入 | 提交或 SHA-256 |
| --- | --- |
| smart-band 构建来源 | GitHub Actions run `29725945155`，head `82c066cce36b5b05f5a8e90d3093aaaefbb1e04f` |
| NuttX ELF | `5f97a280c2478ab94116be111fecef63cd103ce0612ef14e5513933218091d58` |
| Linux emulator | `be9cdef6709c2a7aed547c3029d8872c58e5f3f9` |
| emulator 可执行文件 | `1a8671a1e9a68a25e5cdf006dd291c06a30f7783440a3a974930d85ad7526d7d` |
| headless aarch64 QEMU | `760f02aa25f5f041de62adf2b1c60445046918bef0a0572bb661128a21d9d450` |
| emulator tools | `37f5024f1d9157b9778d0d9e739ee0fa68743d42` |
| emulator skins | `2231b133d74f772ae4a23d9918e94bc2fd61dc78` |
| vendor_openvela | `fe78612bb65fee522ce179526aa3d470d48b6e20` |

`vela_system.bin` 与 `vela_data.bin` 按成功构建日志中的原命令，从上述固定
`vendor_openvela` 提交重新生成。远端宿主补齐了 `genromfs`、`mtools` 和
`libGL.so.1` 运行依赖。

## 最终结果

候选修复后的真实运行证据目录：

```text
/home/ubuntu/smart-band-sim-20260720-v1/evidence/native-smoke-candidate-20260720T1725CST
```

关键结果：

- 状态：`passed`。
- 启动到 `goldfish-armv8a-ap> `：`1.209s`。
- emulator console：`ping` 返回 `I am alive!` 与 `OK`。
- native 应用：出现 `smart_band: UI ready`。
- 稳定性：等待 10 秒后 PID 为 `13`，再等待 5 秒仍为 `13`。
- fatal marker：未出现 display/UI 初始化失败、重复 LVGL 初始化、PANIC、断言失败或段错误。
- 清理：console 请求关闭后终止专属进程组；最终没有残留 emulator/QEMU 进程，端口 `5554` 未占用。

对应的 `runtime-smoke.json` 核心内容：

```json
{
  "boot_seconds": 1.209,
  "console_port": 5554,
  "initial_pids": [13],
  "nsh_prompt": "goldfish-armv8a-ap> ",
  "settle_seconds": 10.0,
  "stability_seconds": 5.0,
  "stable_pids": [13],
  "status": "passed"
}
```

## 试运行发现并闭环的问题

1. 旧 Nightly artifact 只有 `nuttx` 与 `nuttx.config`，缺少 emulator 启动必需的
   `vela_system.bin` 和 `vela_data.bin`，因此不能独立复跑。
2. 原 smoke 只对 emulator 与 headless QEMU 执行 `ldd`；`libOpenglRender.so` 是
   启动时动态加载的，最小 Ubuntu 缺少 `libGL.so.1` 时只能在 emulator abort 后发现。
3. 本轮已让 Nightly artifact 保存两张运行镜像，并让 smoke 在启动前检查四项运行
   产物及 `libOpenglRender.so` 的宿主动态库。

## 尚未证明的边界

本次已证明 native LVGL 初始化、NSH 命令执行、console liveness 和应用进程稳定性，
但仍没有覆盖：

- native emulator 像素截图与浏览器原型的一致性；
- native 触摸、滑动、小游戏端到端交互；
- console sensor 注入后 UI 数值的变化；
- 真机烧录、真实传感器与功耗表现。
