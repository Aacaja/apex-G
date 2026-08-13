# Agent Handoff Entry

完整项目约束、文件地图和验收规则以 [`AGENTS.md`](AGENTS.md) 为准，开始工作前必须先读。

- 当前进度：[`docs/PROGRESS.md`](docs/PROGRESS.md)
- 使用说明：[`README.md`](README.md)
- 实现契约：[`TASK_SPEC.md`](TASK_SPEC.md)
- 安全/API 边界：[`SECURITY_BOUNDARY.md`](SECURITY_BOUNDARY.md)
- Windows 构建：GitHub Actions（`.github/workflows/build.yml`，push main 自动触发）

当前项目为 **Rust 版高抛距离实时显示工具**：xcap 捕获 → 纯像素 OCR 读 `ang:` pitch
→ 反解公式 → 半透明悬浮窗显示"当前角度可抛出的距离"。无网络、无 OCR 库、无 C++。
macOS 只跑 `cargo test --lib` 与 `cargo test --test ocr_test`；Windows 编译全部
交给 GitHub Actions。发布经 `build/publish-repo/`，发布前必须创建回滚事务。
