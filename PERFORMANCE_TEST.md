# PERFORMANCE_TEST — 实机验收协议（Rust 版）

工具：GitHub Actions 产出的 `apex-angle-reader.exe`。

## 1. 验收项

| 项 | 标准 |
|---|---|
| 全屏捕获 | 游戏中持续有数字更新；若 30 秒一直 `--`，记录游戏显示模式（独占全屏/无边框） |
| 锚点核对 | 抬头对准 −75°（游戏内 pitch 显示）→ 悬浮窗稳定 30.0 m；−45° → 60.0 m |
| 灵敏度 | 快速转动视角，数字逐帧平滑跟随，无跳变/回退；240fps 慢录帧差 ≤4 帧 |
| 丢帧率 | 60 秒内有效帧/总帧 ≥95%（其余为看地面/菜单等无 pitch 帧） |
| CPU | 任务管理器 60Hz 下 <2% |
| UI | 半透明可读、不挡 HUD、点击穿透（游戏内点击不受影响） |

## 2. 测量命令

```powershell
Get-Counter '\Process(apex-angle-reader*)\% Processor Time' -SampleInterval 1 -MaxSamples 60
Get-Process apex-angle-reader | Select-Object WorkingSet64
```

## 3. 结果记录模板

机器/分辨率/游戏显示模式/缩放设置：____

| 项 | 结果 |
|---|---|
| 全屏捕获可见 | 是/否（模式） |
| −75° 锚点 | ____ m |
| −45° 锚点 | ____ m |
| 慢录帧差 | ____ 帧 |
| 60s 有效帧率 | ____% |
| CPU 均值 | ____% |
