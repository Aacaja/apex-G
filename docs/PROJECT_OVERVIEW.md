# 项目概览

Apex Angle Reader 是 Windows 原生 Rust 工具：实时读取 Apex `cl_showpos` 左上角
HUD 的 `ang:` 行（纯像素模板匹配），反解高抛公式后，在半透明悬浮窗直接显示
"当前角度可抛出的距离"。用户自己核对距离、自己决定投掷。

公式：`x = 90 - degrees(asin(y/60)) / 2`；反解 `y = 60 * sin(radians((90 + pitch) * 2))`。
锚点：−75°→30m、−45°→60m。

边界：只读屏幕像素（WGC）+ 本地计算 + 自己的悬浮窗；无网络、无进程访问、
无注入、无合成输入。详见 `SECURITY_BOUNDARY.md`。

开发在 macOS（只跑跨平台测试），Windows 编译/使用全部交给 GitHub Actions。
