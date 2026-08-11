# Apex Arc Assist — 高抛角度辅助（像素级、极限轻量）

中文使用/构建说明。本工具是**纯本地像素识别**的高抛角度辅助：只用 Win32 GDI 抓取你校准好的一个极小的屏幕区域，用二进制字形模板做本地识别，计算目标仰角并在自己的置顶悬浮窗上显示。**不使用系统 OCR、不使用 GPU/DirectX、不依赖 OBS**，不读写游戏进程、不注入、不模拟键鼠，也不会向游戏发送任何消息。

核心公式（高抛分支，不可替换为低抛分支）：

```
x = 90 - degrees(asin(y / 60)) / 2
```

- `x`：向上仰角（度），高抛分支，45–90°
- `y`：ping 距离（米），UI 有效范围 1–60
- 锚点：30m → 75°；60m → 45°

---

## 1. 极简工作流

| 步骤 | 按键 | 说明 |
|---|---|---|
| 1 | 启动项加入 `+cl_showpos 1`（或控制台执行 `cl_showpos 1`） | 建议**无边框窗口化**（见第 6 节） |
| 2 | `F9` | 校准：用方向键把红色方框**只框住 `ang:` 后面的第一个 pitch 数字** |
| 3 | `F5` | 单次截取当前字形 → 原样输入屏幕上的 pitch（含负号/小数点）→ Enter。可在不同角度**多次 F5** 补齐全部数字字形 |
| 4 | `F8` | 输入距离（1–60m），Enter 确认；**空 Enter 复用上次距离** |
| 5 | 看悬浮窗 | ARMED → 靠近目标自动进 READY 并发一声提示音（仅进入瞬间响一次） |
| 6 | `F6` | 可随时暂停/恢复；**暂停期间零屏幕采集** |

全部按键（全局热键）：

| 按键 | 功能 |
|---|---|
| `F5` | 一次性字形学习：立刻抓取当前 ROI 一次，按屏幕显示的 pitch 原样输入标签后 Enter（可重复多次，累计保存） |
| `F6` | 暂停/恢复监控。**暂停 = 零 BitBlt、零轮询** |
| `F7` | 切换符号约定：默认 Source 约定「抬头为负」（cl_showpos 向上看为负数）；切换为「抬头为正」 |
| `F8` | 距离输入。0–9/小键盘、小数点、Backspace、Enter、Esc；Enter 确认 1–60m，空 Enter 复用上次，Esc 取消 |
| `F9` | ROI 校准：方向键 1px、Shift+方向键 10px、Ctrl+方向键缩放、PgUp/PgDn 调阈值（±5）、Enter 保存、Esc 取消并还原 |
| `F10` | 提示音开关 |
| `Ctrl+Shift+Q` | 退出 |

> 符号约定细节：Source 引擎 `cl_showpos` 中抬头的 pitch 读数为负值（向上 = -xx°）。F7 默认保持该约定；若你习惯「抬头为正」，按 F7 切换。程序将 0–360 读数归一化到 [-180,180)，再按约定映射为向上仰角。

---

## 2. 状态机与采集节奏（极限轻量设计）

| 状态 | 何时进入 | 屏幕采集 | 采样率 |
|---|---|---|---|
| IDLE | 启动后 / 未输入距离 | **零采集**（无定时器、无 BitBlt） | — |
| 距离输入 | F8 | 零采集（热键输入） | — |
| F9 校准 / F5 训练 | F9 / F5 | 仅 F5 确认时抓**一次**；校准过程零采集 | — |
| ARMED | F8 确认距离后 | 只抓校准好的小 ROI | 远处 **5Hz**；距目标 ≤8° 内 **~12Hz**（83ms） |
| READY | 连续两次窗内样本（时间跨度 120–500ms）后 | 同上 | **3Hz**（333ms），仅进入边缘响一声 |
| PAUSED | F6 | **零采集** | — |
| OCR 丢失 | 连续识别失败 | 同上 | 指数退避 4Hz→2Hz→1Hz |

实现要点（与源码一致）：

- 只有 `armed && 非暂停 && 正常模式` 才会 `SetTimer` 并执行 `BitBlt`（见 `src/apex_arc_assist.cpp` 的 `CanCapture()` / `ScheduleCapture()`）。
- 全进程只复用 **1 个屏幕 DC + 1 个内存 DC + 1 张 top-down 32 位 DIB**（96×24 默认 ROI 仅约 9KB 像素）。采集面在首次抓取时才惰性创建。
- ROI 二进制哈希（FNV-1a 64 位）：像素不变则**复用上次解析结果**，不做模板匹配（悬浮窗显示 `hash-reuse` 计数）。
- 不使用 `timeBeginPeriod` 等定时器分辨率修改，不排队采集任务；`WM_TIMER` 每次只做一件小工作。
- 阈值默认 205（F9 模式 PgUp/PgDn 调节，范围 120–250）：提取「亮且近中性」的 HUD 像素（`high>=threshold` 且 `(max-min)<=92 || high>=248`）。

---

## 3. 识别原理（无需系统 OCR）

1. GDI `BitBlt` 抓取 ROI → 阈值化得到二值网格。
2. 8 连通分割字形 → 过滤噪点 → 归一化到 12×18 二值网格。
3. 与模板匹配：启动时用系统字体（Tahoma/Consolas/Arial，14/17/20 号，共 117 个）渲染生成的通用模板 + 内置 5×7 兜底模板 + **F5 学到的精确模板**。
4. 评分 = `0.78 × 前景重叠 + 0.22 × 全像素一致`，最低 0.43 才算有效；语法只接受 `[+-]?dd.dd` 形式的数字，|pitch|>90° 直接拒绝。

F5 学习（可多次）：

- 按 F5 会**立即抓取当前 ROI 一次**并分割出 1–8 个字形；
- 把屏幕上该数字**原样**输入（含 `-`/`+`/`.`，字符数必须与分割出的字形数一致）后 Enter；
- 保存到 exe 同目录 `ApexArcAssist.templates`（自描述格式，魔数 `AAT1`，12×18 网格，≤512 条）；
- 建议在不同角度、不同光照下**重复 F5**，把 `-+.0123456789` 全部补齐，识别率最高。启动时自动加载。

---

## 4. READY 判定（迟滞 + 距离自适应容差）

- 容差 `tolerance = clamp(0.5 × |de/dd|, 0.35°, 2.0°)`：由公式导数导出，60m 附近整数 ping 取整仍可用（导数发散处封顶 2°）。
- **进入 READY**：连续两个窗内样本（|误差| ≤ tolerance），时间跨度 ≥120ms 且 ≤500ms。
- **退出 READY**：连续两个样本超出 `2 × tolerance`，或出现无效/过期样本。
- 提示音**只在进入 READY 的边缘响一次**（`MessageBeep(MB_OK)`，F10 可关）。

---

## 5. 构建

### 5.1 Windows（MSVC，官方路径）

需要 Visual Studio（含「使用 C++ 的桌面开发」与 Windows SDK）。在 **x64 Native Tools Command Prompt for VS** 中执行：

```bat
build-windows.cmd
```

产物：`dist\ApexArcAssist.exe`（约几百 KB，静态链接 `/MT`，无需任何运行时/服务/第三方库）。

构建细节（与脚本一致）：

- `rc.exe` 编译 `resources\ApexArcAssist.rc`（manifest `asInvoker`、PerMonitorV2 DPI、版本信息）
- `cl.exe /std:c++17 /O2 /GL /MT /W4 /DUNICODE` + `/link /LTCG /OPT:REF /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup`
- **只链接 `user32.lib gdi32.lib`**（可用 `dumpbin /imports dist\ApexArcAssist.exe` 审计）

### 5.2 GitHub Actions（CI，Windows MSVC）

工作流已在 `.github/workflows/windows-build.yml` 中配置：`windows-latest` → MSVC x64 核心测试 → 调用同一个 `build-windows.cmd` → `dumpbin` 导入审计 → 生成 SHA-256 → 上传 `ApexArcAssist-windows-x64` artifact。可在 GitHub 仓库的 **Actions → Build Windows x64 → Run workflow** 手动触发；运行完成后在该 run 页面底部 **Artifacts** 下载 `ApexArcAssist-windows-x64`。本地与 CI 共用一份构建脚本。

### 5.3 测试

```bash
./test.sh   # macOS/Linux 可用
```

- `clang++` 编译并运行纯核心测试（`tests/test_arc_core.cpp`）：0/30/40/55/60m 锚点、无效值、导数容差、0/360 归一化、两种符号约定、数字语法、字形归一化/匹配、二进制哈希复用、READY 进入/退出迟滞、过期/无效重置。
- 静态扫描：`rg` 检查 `src/ resources/` 不含任何禁用 API 名（与 SECURITY_BOUNDARY.md 第 4 节同一正则）。

---

## 6. 使用注意事项与已知限制

- **必须开启 `+cl_showpos 1`**（Steam/EA 启动参数；若用控制台则执行不带 `+` 的 `cl_showpos 1`），否则没有可识别的 pitch 读数。
- **推荐无边框窗口化**：GDI 屏幕 DC 无法可靠抓取「独占全屏」DirectX 画面（可能抓到黑屏）。请使用 `-windowed -noborder`（或游戏内无边框选项）。
- **F9 只框 pitch 数字本身**：`ang:` 行里通常还有其它角度值，必须框住**紧跟 `ang:` 后的第一个 pitch 数字**，不要框到 `ang:` 文本或 yaw/roll。建议 ROI 48–96px 宽、12–24px 高（默认 96×24）；框得越小越省时越准。
- 识别依赖 HUD 文字**亮且近中性**；若 HUD 被染色（如红色血雾），按 F9 用 PgUp/PgDn 调阈值。
- 识别集固定为 `-+.0123456789`；分割出错或阈值不当会显示 `OCR LOST` 并按 4→2→1Hz 退避，此时请 F9 收紧 ROI + F5 补学字形。
- READY 只是**提示**：瞄准与投掷始终由你手动完成；程序不模拟任何键鼠操作。
- 全局热键 F5–F10 / Ctrl+Shift+Q 可能与其它软件冲突；注册失败时悬浮窗会提示。
- 距离为整数 ping，容差已按 60m 端导数放大的原则处理取整误差（0.35°–2.0° 钳位）。
- App 已接入 `ElevationStabilizer`：使用 3 样本中位数显示/判定，单帧跳变 >15° 会被拒绝；若下一帧在 600ms 内与跳变候选相差 ≤1.5°，则视为真实快速转向并重新锚定。显示值因此最多滞后一个采样周期。
- 每次启动位置/阈值/距离/符号约定保存在 exe 同目录 `ApexArcAssist.ini`（明文，可手改）。

---

## 7. 文件清单

| 文件 | 说明 |
|---|---|
| `src/apex_arc_assist.cpp` | Windows 应用：GDI 采集、模板 OCR、调度器、悬浮窗、热键 |
| `src/arc_core.h` | 可移植核心（`original/arc_core.h` 的副本 + 扩展：公式、容差、归一化、语法、字形、哈希、READY 门） |
| `original/arc_core.h` | 原始公式头文件，保持原样不动 |
| `resources/app.manifest` / `ApexArcAssist.rc` | 清单与资源 |
| `build-windows.cmd` / `.github/workflows/windows-build.yml` | MSVC 构建与 GitHub Actions EXE artifact |
| `tests/test_arc_core.cpp` | 纯核心跨平台测试 |
| `README.md` / `SECURITY_BOUNDARY.md` / `PERFORMANCE_TEST.md` | 使用说明 / 可审计边界 / 性能测试方法 |
| `GITHUB_ACTIONS_GUIDE.md` | 从上传源码到手动 Run workflow、下载 artifact、核验 SHA-256 的完整指南 |

更多细节：安全与 API 边界见 `SECURITY_BOUNDARY.md`；性能预算与 A/B 测量协议见 `PERFORMANCE_TEST.md`。
