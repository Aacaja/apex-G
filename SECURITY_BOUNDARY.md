# SECURITY_BOUNDARY — 可审计的 API 边界

本文档逐条声明 Apex Arc Assist 的**能力边界**。审计原则：
**「只算本地、只听热键、只显示自己」**——本版本**不做任何屏幕采集**。

## 1. 允许的能力（完整清单，无隐含项）

| # | 能力 | 实现与源码入口 | 边界说明 |
|---|---|---|---|
| A1 | 注册/注销全局热键 | `RegisterHotKey` / `UnregisterHotKey`；见 `RegisterStaticHotkeys`、`RegisterTextHotkeys` | 只监听，绝不发送；均使用 `MOD_NOREPEAT`；输入/退出时全部注销 |
| A2 | 本地计算 | `HighArcElevationDegrees`、`ElevationToleranceDegrees` | 纯 C++ 数学；公式锚点 30m→75°、60m→45° 有测试强制 |
| A3 | 自己的置顶悬浮窗 | `CreateWindowExW(WS_EX_TOPMOST\|TOOLWINDOW\|NOACTIVATE\|LAYERED\|TRANSPARENT)`；`WM_NCHITTEST→HTTRANSPARENT` | 不抢焦点、点击穿透；`LWA_ALPHA` 半透明；纯 `TextOutW` 自绘；默认隐藏，仅 F9 显示 |
| A4 | 本地配置文件 | `WritePrivateProfileStringW` / `GetPrivateProfileStringW`；见 `LoadConfig`、`SaveConfig` | 只读写 exe 同目录 `ApexArcAssist.ini`；不写注册表 |

**无屏幕捕获**：源码中不存在 `GetDC`、`BitBlt`、`CreateDIBSection`、
`EnumDisplayMonitors` 或任何视频/截图 API。IDLE 时进程阻塞在 `GetMessageW`，
CPU 占用为零。

## 2. 禁止的能力（源码中必须不存在）

`GetDC`、`BitBlt`、`CreateDIBSection`、`StretchBlt`、`OpenProcess`、
`ReadProcessMemory`、`WriteProcessMemory`、`VirtualAllocEx`、
`CreateRemoteThread`、`SetWindowsHookEx`、`SendInput`、`keybd_event`、
`mouse_event`、`SetCursorPos`、对游戏窗口发送的任意消息、
`Windows.Media.Ocr`、Direct3D/D3D11、DXGI、Windows Graphics Capture、
Media Foundation（`MFCreate*`）、OBS（`obs-websocket`）、Winsock/网络 API。

含义：

- **不读任何像素**：没有屏幕 DC，没有任何捕获路径。
- **不读/不写任何进程内存**：不需要游戏数据。
- **不注入、不挂钩、无驱动**：`asInvoker` 普通权限 manifest。
- **不产生合成输入**：瞄准/投掷永远由你手动完成。
- **不联网**：源码无 Winsock/网络导入。
- **不用 GPU/视频管线**：无相关导入。

## 3. 导入边界（PE 审计目标）

`build-windows.cmd` 只链接：

```
user32.lib gdi32.lib   (/MT 静态 CRT，/SUBSYSTEM:WINDOWS)
```

预期 PE 导入：`KERNEL32`、`USER32`、`GDI32`、UCRT。**不得出现** `d3d11.dll`、
`dxgi.dll`、`d2d1.dll`、`Windows.Media.Ocr`、`ws2_32.dll`、`obs-*`。

审计命令（x64 Native Tools 环境）：

```bat
dumpbin /imports dist\ApexArcAssist.exe
dumpbin /headers dist\ApexArcAssist.exe | findstr /i subsystem
dumpbin /dependents dist\ApexArcAssist.exe
```

## 4. 自动验证（与 CI 一致）

`test.sh` 静态扫描正则（`src/` 与 `resources/`，命中即失败）：

```
GetDC|BitBlt|CreateDIBSection|StretchBlt|OpenProcess|ReadProcessMemory|
WriteProcessMemory|VirtualAllocEx|CreateRemoteThread|SetWindowsHookEx|
SendInput|keybd_event|mouse_event|SetCursorPos|PostMessage|SendMessage|
FindWindow|GetWindowThreadProcessId|CreateToolhelp32Snapshot|EnumProcesses|
Windows[.]Media[.]Ocr|D3D11|Direct3D|DXGI|Windows[.]Graphics[.]Capture|
MediaFoundation|MFCreate|WSAStartup|WinHttp|InternetOpen|obs-websocket
```

当前仓库状态：**扫描通过**。`.github/workflows/windows-build.yml` 会在 Windows
Runner 上重复 MSVC 核心测试、GUI 构建和 `dumpbin /imports` 断言，并上传 EXE、
SHA-256 与导入证据。

## 5. 数据流与数据保留

```
热键 F8 → 输入 1–60m → 本地公式 → 显示当前目标角度
热键 F9 → 显示/隐藏半透明标尺（10m..60m 角度表）
```

- 无任何像素数据进入进程。
- 落盘仅 `ApexArcAssist.ini`（上次距离，明文）。无截图、无日志、无网络报文。
- 悬浮窗 `WS_EX_TRANSPARENT` + `HTTRANSPARENT`：不拦截鼠标，不影响游戏输入焦点；
  `WS_EX_NOACTIVATE`：从不抢焦点。

## 6. 威胁模型与残留风险（诚实声明）

| 风险面 | 评估 |
|---|---|
| 全局热键 | 常驻监听面为 F8/F9/Ctrl+Shift+Q；距离输入临时注册数字/小数点/Backspace/Enter/Esc，模式退出即注销；注册冲突会提示 |
| 屏幕捕获 | **无**。程序不读取任何屏幕像素，不采集、不轮询 |
| 本地文件 | INI 与 exe 同目录，写入需要该目录权限；仅本进程写入 |
| 恶意对抗 | 无网络、无插件、无外部输入代码路径；唯一外部输入是热键 |

## 7. 复核清单（Reviewer 用）

1. `rg -n "<第4节正则>" src resources` → 必须无输出。
2. `dumpbin /imports dist\ApexArcAssist.exe` → 仅 KERNEL32/USER32/GDI32/UCRT。
3. `./test.sh` → 核心测试全绿 + `FORBIDDEN_API_SCAN_OK`。
4. 运行 60s：任务管理器确认 IDLE（未按任何键）CPU 为 0.0%，GPU 引擎 0%。
5. 对照第 1 节表格逐条核对具名函数和数据流。
