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

## 识别优化（2026-08-13，用户提供 8 张游戏截图校准）

- 视觉 agent（gpt-5.6-luna，经代理）逐图读出 ANG 真值：1:-43.73 2:-10.07
  3:-45.63 4/5/6:-89.00 7:-34.12 8:-4.08（首轮误读 -41.08，放大复核修正）。
- 用 8 张截图离线校准：
  - 从截图提取 12 个真实字形模板（0-9、-、.，多数投票）替换旧理想模板；
  - ANG 定位从"全图行聚类"改为"A-N-G 锚定 + 同行收集"（行聚类会被背景噪声
    合并/拆散）；
  - 二值化增加染色亮字规则 `hi>=155 && chroma<=100`（红背景会压暗白字）；
  - 允许 1×1 组件（拉伸字体的 `.` 只有 1 像素）并加分类分数门槛(0.55)；
  - 数字区过滤改为"冒号窗口之后的所有组件"（标签组件曾混入 token）。
- 验证结果（`cargo run --example analyze_screenshots`）：
  7 个触发场景（|pitch|>40）全部精确识别（diff=0.00）；pic8(-4.08) 低于触发
  阈值正确 fail-closed。fixture 测试与公式测试全部通过。
- 触发阈值：|pitch|>40 才更新显示（否则距离恒近 60m 无意义）。
- F9 隐藏面板时零捕获/零 OCR/零 CPU（消息泵 50ms 轮询，热键即时响应）。

## UI 优化（2026-08-13）

- 位置：默认屏幕正中偏下（视线焦点附近）；**F10 进入拖动模式**（点击穿透
  关闭、HTCAPTION 可拖），再按 F10 锁定并保存到 `apex-angle-reader.ini`，
  下次启动沿用；`--left`/`--top` 可覆盖。
- 简化：去掉边框/标题/提示行，只保留两行——大字距离（xx.x m）+ 小字
  pitch（xx.x°）；半透明 alpha 200。
- 字体：Bahnschrift（Win10 自带 DIN 工业字体，最接近 Apex 主题的 Rajdhani）。

## 待验证（Windows 实机）

- 独占全屏下 WGC 能否抓到帧（若黑屏 → 无边框窗口化或换 DXGI 后端）；
  −75°→30.0m 锚点核对；60Hz 灵敏度；CPU 占用（按 PERFORMANCE_TEST.md）。

## 发布状态

- 已发布：根目录已切换为 Rust crate（旧 C++ 在 `legacy-cpp/`），回滚事务
  `artifacts/rust-rewrite/`（基线 47fe09e，独立副本实测恢复）。
