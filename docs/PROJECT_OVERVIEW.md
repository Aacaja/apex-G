# 项目概览

## 目的

Apex Arc Assist 是 Windows 原生、像素级的高抛辅助显示工具。用户手动瞄准和投掷；
程序只读取桌面可见像素、做本地数学/OCR，并显示自己的悬浮窗与可选提示音。

高抛公式是固定契约：

```text
x = 90 - degrees(asin(y / 60)) / 2
```

其中 `y` 是距离（米），`x` 是向上仰角；关键锚点为 **30m → 75°**、
**60m → 45°**。不得替换为低抛分支。

## 用户流程

1. 在游戏启动参数中启用 `+cl_showpos 1`。
2. 按 F8：程序在每块显示器左上最多 `720×280` 范围内做一次 `ANG:` 自动定位，
   再输入 1–60m 距离并按 Enter。
3. ARMED 时只采集已定位的极小 pitch ROI；悬浮窗给出当前 pitch、仰角、误差和
   READY 状态。
4. F5 强制重定位；仅在自动定位失败时用 F9 校准或 Shift+F5 人工模板补强。
5. F6 暂停时没有周期性屏幕采集。

## 运行时边界

允许：Win32 GDI 桌面读取、热键、自己的窗口、`MessageBeep`、本地文件配置和
本地二值字形处理。

禁止：进程/内存访问、注入、Hook、驱动、合成输入、游戏窗口消息、网络、系统 OCR、
Direct3D/DXGI/Windows Graphics Capture/Media Foundation/OBS 集成。完整清单见
[`../SECURITY_BOUNDARY.md`](../SECURITY_BOUNDARY.md)。

`pic.png` 仅为离线开发参考，已被 `.gitignore` 排除；运行时代码不打开、保存或传输它。
可提交测试数据仅为脱敏二值掩码。

## 已验证与未验证

已验证的是可移植核心、fixture 回归、静态边界、YAML/XML 解析；详情见
[`PROGRESS.md`](PROGRESS.md)。尚未验证 Windows MSVC 构建、真实游戏全屏行为、OCR
覆盖率、CPU/FPS/GPU 与 READY 体验。请不要把本地 fixture 结果解释成实机结论。

## 入口文档

- 使用/构建：[README](../README.md)
- 架构：[ARCHITECTURE.md](ARCHITECTURE.md)
- 当前状态：[CURRENT_STATUS.md](CURRENT_STATUS.md)
- 安全/API 边界：[SECURITY_BOUNDARY.md](../SECURITY_BOUNDARY.md)
- 性能实测协议：[PERFORMANCE_TEST.md](../PERFORMANCE_TEST.md)
- 手动 Windows CI：[GITHUB_ACTIONS_GUIDE.md](../GITHUB_ACTIONS_GUIDE.md)
