# 架构与数据流

## 组件

| 组件 | 路径 | 职责 |
|---|---|---|
| 可移植核心 | `src/arc_core.h` | 高抛公式、容差（30m→75°、60m→45°） |
| Win32 应用 | `src/apex_arc_assist.cpp` | 热键、INI、半透明标尺绘制 |
| 资源 | `resources/` | `asInvoker`、PerMonitorV2 manifest |
| 回归 | `tests/` | 公式锚点/无效/单调/容差/标尺表 + 原始公式守护 |

## 数据流

```text
F8 → 数字热键输入 1–60m → 本地公式 → 高亮标尺行 + “当前目标 xxm → xx.x°”
F9 → ShowWindow 切换半透明标尺（10m..60m 角度表）
Ctrl+Shift+Q → 退出
```

无采集、无定时器、无轮询：IDLE 时进程阻塞于 `GetMessageW`，CPU 为零。

## 标尺 UI

- 右上角，`WS_EX_TOPMOST|TOOLWINDOW|NOACTIVATE|LAYERED|TRANSPARENT`，
  `WM_NCHITTEST → HTTRANSPARENT`：点击穿透、不抢焦点。
- `LWA_ALPHA 208`：半透明，不挡游戏数据。
- 每行：左侧距离（小字）+ 右侧角度（24px 粗体大字号）；当前距离行琥珀色高亮。
- 默认隐藏；F9 切换；输入模式自动显示。
