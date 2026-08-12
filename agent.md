# Agent Handoff Entry

完整项目约束、文件地图和验收规则以 [`AGENTS.md`](AGENTS.md) 为准，开始工作前必须先读。

- 当前进度：[`docs/PROGRESS.md`](docs/PROGRESS.md)
- 实现契约：[`TASK_SPEC.md`](TASK_SPEC.md)
- 安全/API 边界：[`SECURITY_BOUNDARY.md`](SECURITY_BOUNDARY.md)
- Windows 手动构建：[`GITHUB_ACTIONS_GUIDE.md`](GITHUB_ACTIONS_GUIDE.md)

当前项目为**纯高抛角度计算器**：不采集屏幕、不 OCR、不访问游戏。F8 输入距离，
F9 显示/隐藏半透明标尺（10–60m 每 10m 一档）。发布经 `build/publish-repo/`，
发布前必须创建 `artifacts/angle-ruler/` 回滚事务。
