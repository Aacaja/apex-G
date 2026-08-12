# PERFORMANCE_TEST — 性能验收（角度标尺版）

本版本**没有任何屏幕采集**：进程在 IDLE 时阻塞于 `GetMessageW`，无定时器、
无轮询、无 BitBlt。性能验收因此非常简单。

## 1. 预算（验收标准）

| 项目 | 预算 | 对应实现 |
|---|---|---|
| IDLE（未按任何键） | CPU 0.0%、GPU 0% | 无定时器；阻塞式 `GetMessageW` |
| 标尺显示（F9 打开） | 打开瞬间一次重绘，之后零开销 | `WM_PAINT` 只画一次；无动画/无定时刷新 |
| 输入距离（F8） | 仅热键处理 + 一次重绘 | 无采集 |
| 内存 | 工作集 < 5MB | 无 DIB/位图缓存；/MT 静态 CRT |
| 帧时间影响 | 不参与游戏帧循环；无任何后台线程/捕获 | — |

## 2. 测量方法（Windows）

```powershell
Get-Counter '\Process(ApexArcAssist*)\% Processor Time' -SampleInterval 1 -MaxSamples 60
Get-Process ApexArcAssist | Select-Object WorkingSet64, PrivateMemorySize64
```

状态：IDLE 60s → 按 F9 显示 60s → 按 F8 输入一次 → 60s。任务管理器核对
IDLE 下 CPU 稳定为 0.0%，GPU 引擎列 0%。

## 3. 结果记录模板

机器：____  分辨率/刷新率：____  状态：____

| 指标 | 预算 | 实测 | 通过? |
|---|---|---|---|
| IDLE CPU 60s 均值 | ≈0.0% | | |
| 标尺显示 CPU 60s 均值 | ≈0.0% | | |
| GPU 引擎占用 | 0% | | |
| 工作集 | <5MB | | |
| F9 显示/隐藏延迟 | <10ms（肉眼即时） | | |
| 游戏帧时间影响 | 无（无采集/无钩子） | | |
