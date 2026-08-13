# 当前状态

更新：2026-08-13（Rust 全重构开发中，待首次 CI）

## 已完成并本地验证（macOS）

- 公式反解：锚点 −75°→30m、−45°→60m；无效值拒绝（`cargo test --lib` 3 项全过）。
- OCR 管线：从 pic.png 离线 fixture（240×55，1409 前景像素）识别出 `-52.25`；
  负号擦除后拒绝；空帧拒绝（`cargo test --test ocr_test` 6 项全过）。
- 已知坑已记录于 PROGRESS（沙箱、行聚类容差、冒号组件、负号门、cfg(windows)）。

## 待验证

1. 首次 GitHub Actions：xcap 拉取、MSVC release 构建、artifact 上传。
2. Windows 实机：独占全屏 WGC 捕获可见性；−75°→30.0m 锚点；60Hz 灵敏度；
   CPU 占用。

## 发布状态

- 根目录已切换为 Rust crate；旧 C++ 移入 `legacy-cpp/`。
- 待 push：本事务将同步 `build/publish-repo/` 并附回滚事务
  `artifacts/rust-rewrite/`。
