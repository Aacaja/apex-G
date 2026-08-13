# Changelog

## 2026-08-13 — Rust 全重构：像素读取 → 实时"角度→距离"悬浮窗

- 完全弃用 C++ 与 OCR 方案（旧代码存档 `legacy-cpp/`）。
- 新 Rust crate：xcap 捕获左上 ROI → 纯像素模板匹配读 `ang:` pitch →
  反解高抛公式 → 半透明悬浮窗直接显示"当前角度可抛出的距离"。
- 无网络/WebSocket；无 OCR 库；fail-closed 负号门；模板离线嵌入。
- macOS 只跑跨平台测试；Windows 编译由 GitHub Actions（push 自动触发）完成。

## 历史（已存档 legacy-cpp/）

- C++ 版（角度计算器/OCR 版）的开发与发布记录见 `legacy-cpp/docs/` 与
  `artifacts/` 下各事务目录。
