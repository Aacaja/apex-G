# PERFORMANCE_TEST — 性能预算与 A/B 测量协议

本文档给出**验收标准（P0）**、**可复现测量方法**与**结果记录模板**。所有数字与源码常量一致（src/apex_arc_assist.cpp）；测量对象为 MSVC 构建的 `dist/ApexArcAssist.exe`。

---

## 1. 性能预算（验收标准）

| 项目 | 预算 | 对应实现 |
|---|---|---|
| IDLE / F6 暂停 / 距离输入 / F9 校准 / F5 标签输入 | **零屏幕采集、零轮询**（按下 F5 时仅抓一次用于学习）；任务管理器稳定后显示 **0.0%** | `CanCapture()` 门；`KillCaptureTimer()`；阻塞式 `GetMessageW()` |
| ARMED 单样本采集+识别 | **中位数 ≤ 1.5ms，p95 ≤ 3ms** | `CaptureSurface::Grab()` 的单次 `BitBlt`+阈值；`RecognizePitch()`；哈希不变则复用解析 |
| ARMED 默认占空比 | **≤ 12 次/秒**（仅靠近目标时） | 200ms 远处 5Hz；83ms ≤8° 内约 12Hz；333ms READY 3Hz；丢帧退避 250/500/1000ms；见 `DesiredInterval()` |
| READY | ≤ 3 次/秒，**仅进入边缘一声**提示 | `kReadyIntervalMs=333`；`MessageBeep` 仅在 `ReadyEvent::kEntered` |
| GPU | **零** GPU API/导入，GPU 引擎占用 **0%** | 仅链接 user32/gdi32；无 DXGI/D3D/WGC |
| 内存 | 工作集 **< 10MB**，无第三方运行时/后台服务 | 新安装默认 72×18 单 DIB 约 5KB + 模板约百级×216bit；/MT 静态 CRT |
| 帧时间（你的 PC，A/B） | 三次 60 秒对照：**中位 FPS 差值 ≤ 1%**，**p95 帧时间差值 ≤ 0.5ms** | 见第 4 节协议 |

自报指标（悬浮窗右下角实时显示，每秒刷新）：`CAP median/p95 us`（BitBlt+阈值）、`MATCH median/p95 us`（模板匹配或哈希复用）、`interval=…ms`、`hash-reuse=…`。**预算中的「采集+识别」= CAP + MATCH 两行之和**。

## 2. 测量环境准备

1. Windows 10/11 x64，游戏**无边框窗口化**，`cl_showpos 1`，测试期间关闭其它悬浮工具（RTSS/Afterburner 画线除外，见第 4 节）。
2. 关闭杀软实时扫描等可能周期性干扰的进程（或至少记录其在运行）。
3. 启动 `dist\ApexArcAssist.exe`；F9 时若红框过大，按 Home 恢复 72×18 或按住 `-` 缩小，再完成校准、F5 训练、F8 输入固定距离（如 40m），让程序进入 ARMED。
4. **稳定期**：每次切换状态后先等 60s 再开始记录（模板构建、DC 创建等一次性开销不在稳态内）。

## 3. CPU / 内存 / GPU 测量

```powershell
# CPU：60 秒采样（每 1s 一个点），记录平均值与最大值
Get-Counter '\Process(ApexArcAssist*)\% Processor Time' -SampleInterval 1 -MaxSamples 60

# 任务管理器核对：详情页 CPU 列在 IDLE/PAUSED 下稳定后应显示 0.0%
# 工作集（MB）与私有内存：
Get-Process ApexArcAssist | Select-Object WorkingSet64, PrivateMemorySize64

# GPU 核对：任务管理器 → 性能 → GPU 引擎视图；或详情页「GPU」列，
# IDLE/PAUSED/ARMED 全程应为 0%（本进程无任何 GPU API）
```

各状态独立测一遍，至少 60s/状态：

| 状态 | 预期 CPU（60s 均值） | 预期 GPU | 说明 |
|---|---|---|---|
| IDLE（启动后未输距离） | ~0.0% | 0% | 无定时器 |
| PAUSED（F6） | ~0.0% | 0% | 无定时器；悬浮窗标题显示 `PAUSED / 0 CAPTURE` |
| ARMED 远处（距目标 >8°） | 5 次/秒 × ≤1.5ms ≈ 极低 | 0% | 悬浮窗 `interval=200ms` |
| ARMED 近处（≤8°） | 12 次/秒 × ≤1.5ms | 0% | `interval=83ms`；最重档位 |
| READY | 3 次/秒 | 0% | `interval=333ms`；确认提示音仅响一次 |
| OCR 丢失（故意框错） | 4→2→1Hz 递减 | 0% | `interval=250→500→1000ms` |

> 任务管理器 CPU 列按核数归一化，单线程轻负载可能显示 0.x%；「归零」判定以 IDLE/PAUSED 下 60s 采样均值为准（应 <0.1%）。若想更精确，用 `Get-Counter` 的原始值。

## 4. 帧时间 A/B 协议（用户实机验收）

工具：PresentMon（`PresentMon.exe -process_name <game>.exe -captureall -stop_after 60`）或 CapFrameX（记录帧时间 CSV）。

流程：

1. **固定场景**：同一地图同一静止视角（不移动相机、无人机干扰），同一距离与 READY 状态；把 `cl_showpos` HUD 保留（游戏内真实使用条件）。
2. **A 组（基线）**：不运行本工具，3 次 × 60s，记录中位 FPS 与 p95 帧时间。
3. **B 组（对照）**：运行本工具并处于 **ARMED 近处档（最重档，83ms 采样）**，3 次 × 60s，同样记录。
4. 计算：中位 FPS 差值 % = |B−A|/A；p95 帧时间差值 ms = |B_p95 − A_p95|。

**通过条件（三次取最大差值）：中位 FPS 差值 ≤ 1%，p95 帧时间差值 ≤ 0.5ms。**

附加检查：

- READY 状态下帧时间曲线无周期性毛刺（3Hz 采样不应产生可感知节奏）。
- 验证「暂停即释放」：F6 后 CPU 60s 均值回落到与 IDLE 相同。
- 验证「哈希复用」生效：视角静止时悬浮窗 `MATCH us` 应接近 0 且 `hash-reuse` 持续增长（证明不做重复模板匹配）。

## 5. 结果记录模板

机器：____  CPU：____  GPU：____  分辨率/刷新率：____  游戏版本：____  距离：____m  状态：____

| 指标 | 预算 | A（基线） | B（本工具） | 最大差值 | 通过? |
|---|---|---|---|---|---|
| 中位 FPS（3×60s） | 差值 ≤1% | | | | |
| p95 帧时间 ms | 差值 ≤0.5ms | | | | |
| ARMED 单样本 CAP+MATCH 中位 | ≤1.5ms | — | 悬浮窗读数 | — | |
| ARMED 单样本 CAP+MATCH p95 | ≤3ms | — | 悬浮窗读数 | — | |
| 峰值采样率（近档） | ≤12/s | — | `interval=83ms` | — | |
| READY 采样率 | ≤3/s | — | `interval=333ms` | — | |
| IDLE/PAUSED CPU 60s 均值 | ≈0.0% | — | | — | |
| 工作集 | <10MB | — | | — | |
| GPU 引擎占用 | 0% | — | | — | |
| READY 提示音 | 仅进入边缘一次 | — | 耳听/计数器 | — | |

## 6. 复测与可复现性

- 每次复测记录上文「机器」行全部参数；对比两轮结果差异 >20% 时，先检查背景进程与稳定期是否满足。
- 自报 CAP/MATCH 为中位/p95 滚动窗（120 样本），读数为近似稳态；若需精确分布，用 PresentMon 无法覆盖本进程时，可临时用 `Get-Counter` 对 `% Processor Time` 高频采样交叉印证。
- CI 只做构建与静态扫描（test.sh + dumpbin）；**性能验收必须在用户实机执行**，本文件即验收脚本。
