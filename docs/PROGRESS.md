# 开发进度日志（Rust 版）

> 本日志只记录已观察到的证据。最近更新：2026-08-13（Rust 全重构）。

## 项目目的（用户原话要点）

- 不需要用户输入距离；**实时读取 pitch → 反解公式 → UI 直接显示"当前角度可抛出的距离"**。
- 用户自己在游戏内核对距离，决定是否投掷。
- 只用 Rust；编译与使用全部交给 GitHub Actions（macOS 只做开发与跨平台测试）。
- 用户配置：显示器 2560×1440，游戏 2560×1920 拉伸无黑边；性能富余（9800X3D+5080），可牺牲轻量化换取准确/灵敏。
- 决策 D1–D4：无 WebSocket/网络；抬头为负且必须带负号（fail-closed）；默认显示+F9 隐藏；ROI 默认 320×120。

## 架构

| 模块 | 职责 |
|---|---|
| `src/capture.rs` | xcap(WGC) 捕获 ROI → 灰度+RGB |
| `src/ocr.rs` | 二值化(阈值150+近中性) → 8邻域连通域 → 行聚类 → 归一化模板匹配 → ANG: 行校验 → 负号 pitch token |
| `src/templates.rs` | 离线嵌入 7×7 精确字形(A/N/G/0-9/-/.)+5×7 兜底，12×18 归一化匹配 |
| `src/formula.rs` | 反解 `y=60*sin(radians((90+pitch)*2))`，锚点 −75°→30m、−45°→60m |
| `src/ui.rs` | 手写 Win32 FFI：半透明(alpha205)点击穿透窗口，左上(12,96)，大字距离+小字pitch |
| `src/main.rs` | 参数(--monitor/--roi/--hz/--top)，PeekMessage 循环 + Sleep 定频，F9 切换/Ctrl+Shift+Q 退出 |
| `tests/ocr_test.rs` | 离线 fixture（pic.png 的 ANG 区 240×55 二值掩码，1409 前景像素）验证识别 −52.25、负号门、空帧 |
| `.github/workflows/build.yml` | push main 自动构建 + 上传 exe |

## 关键坑（记录供后续接手）

1. **沙箱**：本开发环境禁止写入任何 `.vscode/extensions.json`，导致 xcap 0.4.1
   无法解压。解决：xcap/image 放在 `[target.'cfg(windows)'.dependencies]`，
   macOS 本地只编译跨平台 lib（不下载 xcap）；Windows 编译全部交给 CI。
2. **行聚类容差**：小数点低于数字基线约 5px，最初 `ref_h/2` 容差把它分到别的行；
   改为 `ref_h.max(4)`（HUD 行间距约 20px，不会误合并）。
3. **冒号识别**：ANG 冒号在 1440p 下是"2×1 短横 + 2×2 点"两个组件，前者会被分类为
   `-`；改用"G 右缘 2×h 窗口内存在 '.' 或 '-' 标点"判定，并只返回标点之后的数字区。
4. **负号门**：pitch token 必须以 `-` 开头，否则该帧不更新 UI（绝不把 +75° 当 30m）。
5. **Cargo 平台**：`ui.rs`/`capture.rs` 仅 `#[cfg(windows)]` 编译；非 Windows 的
   `main()` 是占位（提示 Windows only），保证 `cargo test` 全 target 可在 macOS 编译。

## 本地验证（macOS）

- `cargo test --lib`：公式 3 项全过（锚点/单调/无效）。
- `cargo test --test ocr_test`：6 项全过，包括从 pic.png fixture 识别 `-52.25`、
  负号擦除后拒绝、空帧拒绝。
- macOS 不构建 Windows 二进制（用户明确：编译与使用全部走 GitHub Actions）。

## Windows CI（已验证）

- 提交 `49b8ebe`（Rust 全重构）+ `9f91e1c`（cargo fmt）+ `17c79bc`
  （修复 Windows-only 编译：import ptr、binarize 新签名）。
- run `31677369569` 成功：fmt check ✓、`cargo test --lib` 3 passed ✓、
  `cargo test --test ocr_test` 5 passed ✓、`cargo build --release` ✓。
- artifact `apex-angle-reader`（144309 字节 ≈141KB）。

## 待验证（Windows 实机）

- 独占全屏下 WGC 能否抓到帧（若黑屏 → 无边框窗口化或换 DXGI 后端）；
  −75°→30.0m 锚点核对；60Hz 灵敏度；CPU 占用（按 PERFORMANCE_TEST.md）。

## 发布状态

- 已发布：根目录已切换为 Rust crate（旧 C++ 在 `legacy-cpp/`），回滚事务
  `artifacts/rust-rewrite/`（基线 47fe09e，独立副本实测恢复）。
