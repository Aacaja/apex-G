# Agent Handoff Entry

完整项目约束、文件地图和验收规则以 [`AGENTS.md`](AGENTS.md) 为准，开始工作前必须先读。

- 当前进度：[`docs/PROGRESS.md`](docs/PROGRESS.md)
- 实现契约：[`TASK_SPEC.md`](TASK_SPEC.md)
- 安全/API 边界：[`SECURITY_BOUNDARY.md`](SECURITY_BOUNDARY.md)
- Windows 手动构建：[`GITHUB_ACTIONS_GUIDE.md`](GITHUB_ACTIONS_GUIDE.md)

当前本地 OCR 工作已完成严格回归（以 `docs/PROGRESS.md` 的实际计数为准），正在通过
`build/publish-repo/` 准备发布。F5/F8 必须保持同帧多阈值、token/ROI 互证和非零符号
fail-closed；`pic.png` 不得进入 Git，也不得被运行时读取。Windows CI、实机全屏与性能
仍未验证，不能提前宣称成功。用户接受约 50 MB 以内的本地实现，但不允许突破只读像素、
本地处理、无游戏控制的安全边界。
