# Apex Angle Reader — Agent Guide

> **先读本文件，再修改任何源码、测试、构建、文档或发布内容。**
> 当前进度见 `docs/PROGRESS.md`；使用说明见 `README.md`。

## 项目目的（不可变）

- 纯 Rust Windows 工具：实时读取 Apex `cl_showpos` 左上 HUD 的 `ang:` 行
  （像素模板匹配，无 OCR 库），反解高抛公式后，在**半透明悬浮窗直接显示当前
  角度可抛出的距离**；用户自己核对距离、自己决定投掷。
- 公式固定：`x = 90 - degrees(asin(y/60)) / 2`；锚点 30m→75°、60m→45°。
  反解：`y = 60 * sin(radians((90 + pitch) * 2))`（pitch 抬头为负）。
- 用户不需要输入任何距离。

## 技术约束

- 语言：Rust（Stable，edition 2021）；无 OpenCV/C++ 绑定。
- 捕获：xcap（Windows Graphics Capture）；ROI 默认 320×120，默认 60Hz。
- UI：手写 Win32 FFI（`src/ui.rs`），半透明、点击穿透、左上角 HUD 下方。
- 无网络、无 WebSocket、无进程/内存访问、无注入、无 Hook、无合成输入。
- macOS 只做开发与跨平台测试；**Windows 编译/使用全部交给 GitHub Actions**
  （`cargo test` + `cargo build --release`，push main 自动触发）。

## 安全/正确性规则

- fail-closed：pitch 必须带显式负号、ANG 行校验、值域 [-90,0)；任何歧义 → 该帧
  不更新 UI（显示 `--`），绝不把无符号/错误值当距离显示。
- 模板来自用户 2560×1920 拉伸画面的离线证据（`pic.png` 不入库）；运行时永不
  打开图片文件。测试用脱敏二值 fixture（`tests/fixtures/ang_roi.rs`）。

## 工作方式

1. 修改前阅读本文件、`TASK_SPEC.md`、`docs/PROGRESS.md`。
2. 修改后至少运行：`cargo fmt --check`、`cargo test --lib`、
   `cargo test --test ocr_test`（本机只跑这些，不 build Windows）。
3. Windows 构建验证依赖 CI（push 后查看 Actions run）。
4. 更新 `docs/PROGRESS.md` 记录已验证事实与未验证项。
5. 发布经 `build/publish-repo/`（唯一可提交 Git 工作树），先创建回滚事务
   `artifacts/<transaction>/`（快照+diff+SHA+ROLLBACK.sh，并在独立副本实测）。

## 关键路径

| 路径 | 责任 |
|---|---|
| `src/capture.rs` | xcap 捕获（仅 Windows） |
| `src/ocr.rs` | 二值化/连通域/行聚类/模板匹配/ANG 定位/负号门 |
| `src/templates.rs` | 离线字形模板（12×18 归一化） |
| `src/formula.rs` | 反解公式 + 单元测试 |
| `src/ui.rs` | Win32 悬浮窗（仅 Windows） |
| `src/main.rs` | 参数/主循环/热键 |
| `tests/ocr_test.rs` + `tests/fixtures/` | 跨平台回归 |
| `.github/workflows/build.yml` | push 自动构建 + artifact |
| `build/publish-repo/` | 唯一可提交/push 的 Git 工作树 |
| `legacy-cpp/` | 旧 C++ 版本存档（不参与构建） |
