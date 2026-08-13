# SECURITY_BOUNDARY — Rust 版 API 边界

审计原则：**「只读像素、只算本地、只显示自己」**。

## 1. 允许的能力（完整清单）

| # | 能力 | 实现 | 边界说明 |
|---|---|---|---|
| A1 | 屏幕像素读取 | `xcap`（Windows Graphics Capture）捕获左上 ROI（默认 320×120） | 只读显示器输出像素，不写任何东西 |
| A2 | 本地计算 | `src/ocr.rs`、`src/formula.rs` | 纯 Rust 像素二值化/模板匹配与反解公式 |
| A3 | 自己的悬浮窗 | `src/ui.rs` 手写 Win32：`WS_EX_LAYERED\|NOACTIVATE\|TRANSPARENT`，`WM_NCHITTEST→HTTRANSPARENT` | 半透明、点击穿透、不抢焦点 |
| A4 | 热键 | `RegisterHotKey`：F9 显示/隐藏，Ctrl+Shift+Q 退出 | 只监听，绝不发送 |

## 2. 禁止（源码/依赖中不存在）

- 网络：无 WebSocket、无 HTTP、无任何 socket 依赖
- 进程/内存：无 `OpenProcess`/`ReadProcessMemory` 等
- 注入/Hook/驱动：无
- 合成输入：无 `SendInput`/`keybd_event`/`mouse_event`
- 游戏窗口消息：不向任何外部窗口发送消息
- 系统 OCR/GPU 渲染管线：无 Windows.Media.Ocr；xcap 使用系统 WGC 只读捕获
- 运行时文件：不打开/保存任何图片；不写日志文件

## 3. 数据流

```
WGC 捕获 ROI → 灰度+RGB → 二值化(150/近中性) → 连通域 → 模板匹配 → ANG: 行
→ 负号 pitch → 反解距离 → 悬浮窗绘制
```

像素数据仅在进程内存中每帧覆写；不落盘、不传输。

## 4. 依赖树说明

- `xcap`：Windows Graphics Capture 封装（只读屏幕）
- `image`：仅作为 xcap 返回的 RgbaImage 类型载体
- `anyhow`：错误处理

## 5. 复核清单

1. `cargo tree` 无网络/进程库；`cargo audit`（如可用）无已知漏洞。
2. 源码 grep 无 `OpenProcess|ReadProcessMemory|SendInput|keybd_event|WSA|TcpStream|WebSocket`。
3. 实机：IDLE 时任务管理器 CPU ≈ 0%（60Hz 捕获下应 <2%）。
4. 全屏捕获失败时 UI 显示 `--` 而非错误数据。
