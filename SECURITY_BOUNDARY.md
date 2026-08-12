# SECURITY_BOUNDARY — 可审计的 API 边界

本文档逐条声明 Apex Arc Assist 的**能力边界**，每条均可对照源码中的具名函数与 PE 导入表审计。这里引用函数名而非易漂移的行号。

审计原则：**「只读像素、只听热键、只算本地、只显示自己」**。不碰游戏进程、不注入、不挂钩、不模拟输入、不联网、不用 GPU/系统 OCR/OBS。

---

## 1. 允许的能力（完整清单，无隐含项）

| # | 能力 | 实现与源码入口 | 边界说明 |
|---|---|---|---|
| A1 | 注册/注销全局热键（监听输入） | `RegisterHotKey` / `UnregisterHotKey`；见 `RegisterStaticHotkeys`、`RegisterTextHotkeys`、`RegisterCalibrationHotkeys` | 只**监听**，绝不**发送**；静态/文本热键使用 `MOD_NOREPEAT`，F9 校准移动/缩放键刻意允许按住连发；退出/模式切换时全部注销 |
| A2 | 抓取桌面像素 | `GetDC(nullptr)` + `CreateCompatibleDC` + `CreateDIBSection` + `BitBlt(SRCCOPY)`；见 `CaptureSurface`、`AutoLocateApexHud` | ARMED 时仅周期抓自动定位/手调的小 ROI；F5/F8 是用户显式触发的一次扫描，每块显示器仅左上角最多 `720×280`，同一 DIB 在内存中派生多个阈值掩码，随后释放大 DIB；F9 可居中缩放并自动限制在虚拟桌面内；无连续全屏轮询 |
| A3 | 本地字形识别 | `apex_hud_ocr.h`、`SegmentGlyphs`、`NormalizeGlyph`、`OverlapScore`、`MatchScore`、`RecognizePitch` | 固定字形集 `-+.0123456789`；自动定位 ROI 仅接受用户截图离线派生的 Apex HUD `ANG`/数字模板。非零值要求可见未粘连符号、完整 token/ROI 留边，阈值候选不一致或任何方向不可信时 fail-closed；系统字体、5×7 与 Shift+F5 精确模板仅用于手动路径；运行时不打开图片文件 |
| A4 | 本地计算 | `HighArcElevationDegrees`、`ElevationToleranceDegrees`、`NormalizePitchDegrees`、`PitchSignContinuity`、`ElevationStabilizer`、`ReadyGate` | 纯 C++ 数学；符号连续性保护拒绝无符号非零/反号，3 样本中值稳定，单帧 >15° 跳变拒绝并需第二帧确认 |
| A5 | 自己的置顶悬浮窗 | `CreateWindowExW(WS_EX_TOPMOST\|TOOLWINDOW\|NOACTIVATE\|LAYERED\|TRANSPARENT)`；`WM_NCHITTEST→HTTRANSPARENT` | 不抢焦点、点击穿透；纯 `TextOutW` 自绘 |
| A6 | 提示音 | `MessageBeep(MB_OK)` | 仅 READY 进入边缘一次；F10 可关；无媒体管线 |
| A7 | 本地配置/模板文件 | Profile 文件 API 与 `_wfopen_s/_fread/_fwrite`；见 `LoadConfig`、`SaveConfig`、`LearnedTemplateFile` | 只读写 exe 同目录的 INI 与 `ApexArcAssist.templates`；不写注册表 |
| A8 | 消息循环与定时采样 | `GetMessageW` + `SetTimer/KillTimer`；见 `Run`、`ScheduleCapture`、`KillCaptureTimer` | 普通消息定时器；无定时器精度修改、无队列任务、无轮询线程 |

## 2. 禁止的能力（源码中必须不存在）

运行时源码/导入表**不得包含**：`OpenProcess`、`ReadProcessMemory`、`WriteProcessMemory`、`VirtualAllocEx`、`CreateRemoteThread`、`SetWindowsHookEx`、`SendInput`、`keybd_event`、`mouse_event`、`SetCursorPos`、对游戏窗口发送的任意消息、`Windows.Media.Ocr`、Direct3D/D3D11、DXGI、Windows Graphics Capture、Media Foundation（`MFCreate*`）、OBS（`obs-websocket`）等。

含义：

- **不读/不写任何进程内存**：看不到游戏数据，也不需要（一切数据来自屏幕像素与你的键盘输入）。
- **不注入、不挂钩、无驱动**：`asInvoker` 普通权限 manifest，无管理员要求（`resources/app.manifest`）。
- **不产生合成输入**：瞄准/投掷永远由你手动完成；热键只监听。
- **不联网**：源码无 Winsock/网络导入；不发送任何遥测。
- **不用 GPU/视频管线**：渲染与识别全部走 GDI 软件路径；GPU 引擎占用为零（见 PERFORMANCE_TEST.md）。

## 3. 导入边界（PE 审计目标）

`build-windows.cmd` 的链接行只给出：

```
user32.lib gdi32.lib   (/MT 静态 CRT，/SUBSYSTEM:WINDOWS)
```

预期 PE 导入：`KERNEL32`、`USER32`、`GDI32`、UCRT（`UCRTBASE`/API-Set）。**不得出现** `d3d11.dll`、`dxgi.dll`、`d2d1.dll`、`Windows.Media.Ocr` 相关、`ws2_32.dll`、`obs-*`。

审计命令（x64 Native Tools 环境）：

```bat
dumpbin /imports dist\ApexArcAssist.exe
dumpbin /headers dist\ApexArcAssist.exe | findstr /i subsystem
dumpbin /dependents dist\ApexArcAssist.exe
```

## 4. 自动验证（与 CI 一致）

`test.sh` 静态扫描正则（`src/` 与 `resources/`，命中即失败）：

```
OpenProcess|ReadProcessMemory|WriteProcessMemory|VirtualAllocEx|CreateRemoteThread|
SetWindowsHookEx|SendInput|keybd_event|mouse_event|SetCursorPos|
PostMessage|SendMessage|FindWindow|GetWindowThreadProcessId|
CreateToolhelp32Snapshot|EnumProcesses|Windows[.]Media[.]Ocr|
D3D11|Direct3D|DXGI|Windows[.]Graphics[.]Capture|MediaFoundation|MFCreate|
WSAStartup|WinHttp|InternetOpen|obs-websocket
```

当前仓库状态：**扫描通过**（`FORBIDDEN_API_SCAN_OK`）。`.github/workflows/windows-build.yml` 会在 Windows Runner 上重复 MSVC 核心测试、GUI 构建和 `dumpbin /imports` 断言，并上传 EXE、SHA-256 与导入证据。

## 5. 数据流与数据保留

```
F5/F8（用户显式一次请求）：每显示器左上角≤720×280 --BitBlt + GdiFlush--> 临时DIB(~0.8MB)
--同帧阈值掩码(150/160/170/当前F9) + 本地ANG模板一致性--> 第一个完整pitch ROI --释放临时DIB
ARMED：小ROI --BitBlt--> 小DIB(通常约5KB) --阈值--> 二值网格
--FNV-1a哈希--> (哈希不变则复用上次解析, 不匹配模板) --模板匹配/符号连续性门
--> pitch 数值或OCR LOST --> 本地映射为仰角 --> READY门 --> 悬浮窗文本 + 一次MessageBeep
```

- 像素数据仅存在于进程内瞬时 DIB/二值缓冲，每次覆写或释放；不落盘、不传输。
- 落盘内容仅两类：`ApexArcAssist.ini`（ROI 坐标/阈值/色差上限/距离/符号约定/声音，明文）与 `ApexArcAssist.templates`（字形 id + 12×18 二值网格）。**无运行时截图文件、无日志、无网络报文。**
- 悬浮窗 `WS_EX_TRANSPARENT` + `HTTRANSPARENT`：不拦截鼠标，不影响游戏输入焦点；`WS_EX_NOACTIVATE`：从不抢焦点。

## 6. 威胁模型与残留风险（诚实声明）

| 风险面 | 评估 |
|---|---|
| 全局热键 | 常驻监听面为 F5/F6/F7/F8/F9/F10/Ctrl+Shift+Q；距离/训练模式临时注册数字、符号、Enter/Esc，F9 模式临时注册方向键、`-/=/+`、小键盘加减、Home、PgUp/PgDn、Enter/Esc；模式退出即注销，注册冲突会提示 |
| 屏幕捕获 | ARMED 只抓小 ROI；F5/F8 会短暂读取每块显示器左上角 `720×280` 的桌面像素来找 `ANG:`。这些像素不落盘，但该范围内的一切可见内容会进入进程；请勿把敏感窗口放在该区域 |
| 本地文件 | ini/模板与 exe 同目录，写入需要该目录权限；仅本进程写入 |
| 恶意对抗 | 程序无任何代码执行路径可被外部触发（无网络、无插件、无命令行参数处理以外的行为）；唯一外部输入是热键与屏幕像素 |
| 角度稳定 | App 已接入 `PitchSignContinuity` 与 `ElevationStabilizer`：无符号非零、可疑反号/缺失/粘连符号、ROI 截断和多阈值冲突先拒绝为 OCR LOST；其后 3 样本中值、>15° 单帧拒绝、600ms 内相邻第二帧确认真实快速转向；无效样本会立即撤销 READY |

## 7. 复核清单（Reviewer 用）

1. `rg -n "<第4节正则>" src resources` → 必须无输出。
2. `dumpbin /imports dist\ApexArcAssist.exe` → 仅 KERNEL32/USER32/GDI32/UCRT。
3. `./test.sh` → 核心测试全绿 + `FORBIDDEN_API_SCAN_OK`。
4. 运行 60s：任务管理器确认 IDLE/PAUSED 下 CPU 归零、GPU 引擎列为 0%（方法见 PERFORMANCE_TEST.md）。
5. 对照第 1 节表格逐条核对具名函数和数据流。
