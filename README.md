# Apex Arc Assist — 高抛角度标尺（纯计算器）

Windows 原生、**零屏幕采集**的高抛角度计算器。不读取游戏画面、不做 OCR、
不访问游戏进程：你按距离查角度，自己瞄准投掷。

核心公式（高抛分支，不可替换为低抛分支）：

```
x = 90 - degrees(asin(y / 60)) / 2
```

- `x`：向上仰角（度）
- `y`：ping 距离（米），1–60
- 锚点：30m → 75°；60m → 45°

## 用法

| 按键 | 功能 |
|---|---|
| `F9` | 显示/隐藏半透明角度标尺（默认隐藏） |
| `F8` | 输入当前距离（1–60m，支持小数）；空 Enter 复用上次距离 |
| `Ctrl+Shift+Q` | 退出 |

标尺面板（右上角、半透明、点击穿透、不抢焦点、不挡游戏数据）：

| 距离 | 角度 |
|---|---|
| 10m | 85.2° |
| 20m | 80.3° |
| 30m | 75.0° |
| 40m | 69.1° |
| 50m | 61.8° |
| 60m | 45.0° |

输入距离后，对应行高亮并显示“当前目标 xxm → xx.x°”；你可以同时用标尺
估读附近距离的角度。面板只在 F9 时出现，其余时间零窗口干扰。

## 构建

Windows（MSVC x64 Native Tools）：

```bat
build-windows.cmd
```

产物 `dist\ApexArcAssist.exe`，仅链接 `user32.lib gdi32.lib`，无任何运行时依赖。

macOS/Linux 可跑核心测试：

```bash
./test.sh
```

GitHub Actions（`workflow_dispatch` 手动触发）会执行核心测试、MSVC 构建、
`dumpbin` PE 导入审计并上传 artifact，详见 `GITHUB_ACTIONS_GUIDE.md`。

## 配置

`exe 同目录\ApexArcAssist.ini` 保存上次距离（明文，可手改）。无截图、无日志、
无网络、无注册表写入。
