# 当前状态

更新：2026-08-12（已发布，Windows CI 通过）

## 已完成并本地验证

- 零屏幕采集：源码无 `GetDC`/`BitBlt`/`CreateDIBSection`；IDLE 无定时器。
- F8 距离输入（1–60m，小数、复用上次、Esc 取消）；F9 标尺显示/隐藏。
- 半透明、点击穿透、右上角标尺：10–60m 角度表，当前距离行高亮。
- 核心测试 `arc_core: 287 checks, 0 failures`；`BASELINE_OK 30m→75.0000°`；
  禁用 API 扫描通过（含捕获 API 正则）。

## 待验证

1. Windows MSVC 构建 + PE 导入审计：push 后手动 CI。
2. 用户 Windows 实机：F8/F9 交互、半透明可读性、不遮挡游戏数据、CPU 零占用。

## 发布状态

经 `build/publish-repo/` 提交推送，并附带 `artifacts/angle-ruler/` 回滚事务。
