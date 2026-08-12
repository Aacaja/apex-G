# Apex Arc Assist — 高抛角度辅助（像素级、极限轻量）

中文使用/构建说明。本工具是**纯本地像素识别**的高抛角度辅助：按 F5/F8 时用 Win32 GDI 对每块显示器左上角做一次有界本地扫描，自动找到 `ANG:` 后的第一个 pitch；随后只抓该极小数字区域，计算目标仰角并在自己的置顶悬浮窗上显示。自动路径对符号采用 **fail-closed**：高角度读数的 `+/-` 缺失、粘连或方向突变时显示 `OCR LOST`，不会用可疑数值给出反向提示。**不使用系统 OCR、不使用 GPU/DirectX、不依赖 OBS**，不读写游戏进程、不注入、不模拟键鼠，也不会向游戏发送任何消息。

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
| 1 | 启动项加入 `+cl_showpos 1`（或控制台执行 `cl_showpos 1`） | 保持左上角显示 `NAME / POS / ANG / VEL` |
| 2 | `F8` | 程序会先**自动定位 `ANG:` 后第一个 pitch**，再输入距离（1–60m）并 Enter；空 Enter 复用上次距离 |
| 3 | 看悬浮窗 | 显示已找到的 `ANG:` token；ARMED 后靠近目标自动进 READY 并发一声提示音 |
| 4 | `F5`（仅失败时） | 强制重新自动定位 `ANG:`；正常情况下**不需要输入任何 pitch 数字** |
| 5 | `F9`（最后兜底） | 仅当 F5 自动定位失败时才手调红框；`Home` 恢复紧凑 `72×18`，`-` 缩小、`=`/`+` 放大 |
| 6 | `F6` | 可随时暂停/恢复；**暂停期间零屏幕采集** |

全部按键（全局热键）：

| 按键 | 功能 |
|---|---|
| `F5` | 自动定位：一次扫描每个显示器的左上角有限区域，匹配 `ANG:` 并锁定其第一个 pitch；不要求输入数字 |
| `Shift+F5` | 可选人工补强：仅 F5 失败或 HUD 字体明显变化时，截取当前 ROI 并原样输入 pitch（含符号/小数点）后 Enter |
| `F6` | 暂停/恢复监控。**暂停 = 零 BitBlt、零轮询** |
| `F7` | 切换符号约定：默认 Source 约定「抬头为负」（cl_showpos 向上看为负数）；切换为「抬头为正」 |
| `F8` | 距离输入。0–9/小键盘、小数点、Backspace、Enter、Esc；Enter 确认 1–60m，空 Enter 复用上次，Esc 取消 |
| `F9` | ROI 校准：方向键移动 1px、Shift+方向键移动 10px；`-`/小键盘 `-` 居中缩小，`=`/`+`/小键盘 `+` 居中放大（均可按住连续调整）；`Home` 恢复推荐 `72×18`；Ctrl+方向键精细调宽高；PgUp/PgDn 调阈值；Enter 保存，Esc 取消 |
| `F10` | 提示音开关 |
| `Ctrl+Shift+Q` | 退出 |

> 符号约定细节：Source 引擎 `cl_showpos` 中抬头的 pitch 读数为负值（向上 = -xx°）。F7 默认保持该约定；若你习惯「抬头为正」，按 F7 切换。程序将 0–360 读数归一化到 [-180,180)，再按约定映射为向上仰角。

---

## 2. 状态机与采集节奏（极限轻量设计）

| 状态 | 何时进入 | 屏幕采集 | 采样率 |
|---|---|---|---|
| IDLE | 启动后 / 未输入距离 | **零采集**（无定时器、无 BitBlt） | — |
| 距离输入 | F8 | 零采集（热键输入） | — |
| F9 校准 / F5 自动定位 / Shift+F5 补强 | F9 / F5 / Shift+F5 | F5/F8 只做一次有界左上角扫描；Shift+F5 只抓当前 ROI 一次；其余过程零采集 | — |
| ARMED | F8 确认距离后 | 只抓校准好的小 ROI | 远处 **5Hz**；距目标 ≤8° 内 **~12Hz**（83ms） |
| READY | 连续两次窗内样本（时间跨度 120–500ms）后 | 同上 | **3Hz**（333ms），仅进入边缘响一声 |
| PAUSED | F6 | **零采集** | — |
| OCR 丢失 | 连续识别失败 | 同上 | 指数退避 4Hz→2Hz→1Hz |

实现要点（与源码一致）：

- 只有 `armed && 非暂停 && 正常模式` 才会 `SetTimer` 并执行 `BitBlt`（见 `src/apex_arc_assist.cpp` 的 `CanCapture()` / `ScheduleCapture()`）。
- 全进程只复用 **1 个屏幕 DC + 1 个内存 DC + 1 张 top-down 32 位 DIB**。自动定位仅在 F5/F8 时临时抓每块显示器左上角最多 `720×280`，随后立刻释放大 DIB；ARMED 状态回到自动锁定的紧凑数字 ROI（通常几十像素，默认最小 `72×18`）。
- ROI 二进制哈希（FNV-1a 64 位）：像素不变则**复用上次解析结果**，不做模板匹配（悬浮窗显示 `hash-reuse` 计数）。
- 不使用 `timeBeginPeriod` 等定时器分辨率修改，不排队采集任务；`WM_TIMER` 每次只做一件小工作。
- F5/F8 对每块显示器只做 **一次** `BitBlt`，随后从同一张内存 DIB 生成阈值 `150 / 160 / 170` 的二值候选；若你曾在 F9 使用 PgUp/PgDn 调到其他阈值，也会把该值加入同一帧候选。多个成功候选必须指向同一 `ANG:` token/区域，否则直接 `OCR LOST`，不会按最高分盲选。色差上限沿用当前 F9 配置（默认 64）；提取条件为 `high>=threshold && ((max-min)<=chroma_delta || (high>=248 && low>=190))`。

---

## 3. 识别原理（无需系统 OCR）

1. F5/F8 触发一次本地、有界的左上角扫描：匹配 `ANG` 字样，再严格取其后的**第一个完整数字 token**。
2. 找到后自动把 ROI 缩到该 token；ARMED 状态只用 GDI `BitBlt` 抓取这个小框并阈值化。
3. 8 连通分割字形 → 归一化到 12×18 网格 → 自动定位的 ROI **只接受**由你的 `pic.png` 离线提取的 Apex/Source HUD 模板；F9/Shift+F5 的人工兜底路径才允许通用 Windows 字体、5×7 和已学习模板。
4. 数字语法严格为 `[+-]?dd.dd`，|pitch|>90° 直接拒绝。自动 ROI 必须包含完整 token 边界和可见 `+/-`（只有精确 `0.00` 可无符号）；缺失/粘连符号、ROI 边缘截断、token 内部后缀（例如把 `-52.25` 错切为 `2.25`）或多阈值互相冲突都会变成 `OCR LOST`，而不是静默翻转方向。

自动定位已用提供截图中的 `ANG: -52.25` 做回归测试，覆盖原尺寸、约 1920×1080 的 75% HUD 缩放、2× HUD 缩放、真实截图中出现的数字字形、减号缺失和减号粘连变异。运行时不读取或保存该截图。

**极少数失败时的 Shift+F5 补强：**

- 按 `Shift+F5` 才会截取当前 ROI 一次；
- 原样输入屏幕数字（含 `-`/`+`/`.`）并按 Enter；
- 模板保存到 exe 同目录 `ApexArcAssist.templates`（自描述格式，12×18 网格，≤512 条），之后自动加载。

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

- `clang++` 编译并运行纯核心测试（`tests/test_arc_core.cpp`）：公式/容差/符号/语法/字形/哈希/ROI/READY 全部原有检查，以及截图派生的 `ANG: -52.25` 自动定位与读数回归（原尺寸、75%、2× HUD 缩放、空图、减号缺失与粘连拒绝、符号连续性）。
- `tests/baseline_test.cpp` 单独以未修改的 `original/arc_core.h` 编译 30m→75.0000° 守护，防止高抛公式意外改变。
- 静态扫描：`rg` 检查 `src/ resources/` 不含任何禁用 API 名（与 SECURITY_BOUNDARY.md 第 4 节同一正则）。

---

## 6. 使用注意事项与已知限制

- **必须开启 `+cl_showpos 1`**（Steam/EA 启动参数；若用控制台则执行不带 `+` 的 `cl_showpos 1`），否则没有可识别的 pitch 读数。
- **全屏注意**：此程序只使用 GDI 桌面像素。你已经确认当前机器可读到数字，说明可先测试新自动路径；但真正独占全屏在某些驱动/游戏组合下仍可能抓黑屏或让悬浮窗被覆盖。此时 F5/F8 会提示找不到 `ANG:`，不是要求你手输数字；请先在你的全屏环境实际验证，若 GDI 不可见则现有技术路线无法可靠抓取该画面。
- **通常无需 F9**：先按 F8（或 F5）让程序自动找到 `ANG:` 后第一个数。只有出现“未找到 ANG:”时才进入 F9；红框只框完整的第一项 pitch（包括 `+/-` 与两位小数），可贴紧 token 供人工恢复，但不能截掉任一笔画；按 `Home` 后用 `-` 收紧、方向键微移。不要框到 `ANG:` 文本或第二、第三个角度值。
- 识别依赖 HUD 文字**亮且近中性**；若 HUD 被染色（如红色血雾），按 F9 用 PgUp/PgDn 调阈值。
- 识别集固定为 `-+.0123456789`；`OCR LOST` 时先按 F5 自动重定位。若状态提示“符号不可信”，请继续平稳抬枪直到 HUD 再出现完整带 `+/-` 的 token；不要依赖无符号非零值（即使显示为 `2.25`，也可能是被裁切的高角度 token）。仍失败才用 F9 收紧 ROI 或 Shift+F5 补强。丢失期间采样按 4→2→1Hz 退避。
- READY 只是**提示**：瞄准与投掷始终由你手动完成；程序不模拟任何键鼠操作。
- 显示模式/分辨率改变后，程序停止旧 ROI 采集并要求按 F5 重定位；也可用 F9 或 Shift+F5 明确恢复手动 ROI。全局热键 F5–F10 / Ctrl+Shift+Q 可能与其它软件冲突；注册失败时悬浮窗会提示。
- 距离输入支持 1–60m 的整数或小数；容差仍按 ping 常见的整数取整误差、60m 端导数放大原则处理（0.35°–2.0° 钳位）。
- App 已接入 `ElevationStabilizer`：使用 3 样本中位数显示/判定，单帧跳变 >15° 会被拒绝；若下一帧在 600ms 内与跳变候选相差 ≤1.5°，则视为真实快速转向并重新锚定。显示值因此最多滞后一个采样周期。
- 每次启动位置/阈值/色差上限/距离/符号约定保存在 exe 同目录 `ApexArcAssist.ini`（明文，可手改；小数固定使用 `.`）。

---

## 7. 文件清单

| 文件 | 说明 |
|---|---|
| `src/apex_arc_assist.cpp` | Windows 应用：一次式 ANG 自动定位、GDI 采集、模板 OCR、调度器、悬浮窗、热键 |
| `src/apex_hud_ocr.h` | 可移植 Apex/Source HUD 自动定位与截图派生字形模板（运行时不读取图片） |
| `src/arc_core.h` | 可移植核心（`original/arc_core.h` 的副本 + 扩展：公式、容差、归一化、语法、字形、哈希、READY 门） |
| `src/roi_geometry.h` | 可移植 ROI 几何：居中缩放、72×18 紧凑复位、尺寸/虚拟桌面边界夹取 |
| `original/arc_core.h` | 原始公式头文件，保持原样不动 |
| `resources/app.manifest` / `ApexArcAssist.rc` | 清单与资源 |
| `build-windows.cmd` / `.github/workflows/windows-build.yml` | MSVC 构建与 GitHub Actions EXE artifact |
| `tests/test_arc_core.cpp` / `tests/fixtures/` | 纯核心跨平台测试；脱敏 `ANG: -52.25` 扫描掩码和截图派生的数字字形夹具 |
| `README.md` / `SECURITY_BOUNDARY.md` / `PERFORMANCE_TEST.md` | 使用说明 / 可审计边界 / 性能测试方法 |
| `GITHUB_ACTIONS_GUIDE.md` | 从上传源码到手动 Run workflow、下载 artifact、核验 SHA-256 的完整指南 |

更多细节：安全与 API 边界见 `SECURITY_BOUNDARY.md`；性能预算与 A/B 测量协议见 `PERFORMANCE_TEST.md`。
