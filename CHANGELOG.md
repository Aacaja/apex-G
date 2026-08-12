# Changelog

本文件仅记录已落地的源码与文档变更；Windows 实机行为、性能和 CI 结果以各次
实际运行记录为准，不以本文替代验证证据。

## Unreleased — automatic `ANG:` pitch OCR hardening

### Added

- F5 与 F8 的单次、每显示器左上角（最大 `720×280`）`ANG:` 自动定位。
- `src/apex_hud_ocr.h`：基于离线脱敏二值模板的 `ANG:` 空间校验、首个完整
  pitch token 定位、缩放字形变体、严格 token 识别与多阈值候选仲裁。
- 脱敏 fixture：目标行、数字字形及保留相邻 HUD 行的多行二值掩码；不包含 RGB
  场景、玩家昵称或原始截图。
- 可移植回归：原始、75%、125%、150%、200% 及非零栅格相位；严格 ARMED ROI
  路径、缺失/粘连减号、右偏截断 token、多阈值一致/冲突结果。
- F5/F8 在**同一次 GDI 抓图**上派生 `150/160/170` 二值候选，并附加当前 F9
  阈值（若不同）；不增加第二次抓图。
- `BitBlt` 后的 `GdiFlush()`、ROI 窗口销毁、F9 红框点击穿透，以及 PerMonitorV2
  DPI 变化时的 overlay 重定位处理。
- 显示模式改变后停止旧 ROI 采集，直到 F5 自动重定位或用户明确 F9/Shift+F5 恢复。

### Changed

- 自动路径仅接受完整、已留边的 token；非零角度必须有可见 `+/-`。无符号、截断、
  token 内部后缀（如从 `-52.25` 误切出的 `2.25`）、符号粘连或阈值候选冲突全部
  fail-closed 为 `OCR LOST`。
- `PitchSignContinuity` 将无符号非零近零读数视为不可信，防止裁切高角度 token
  被错误当作接近水平。
- ARMED 持续采集仍是单个小 ROI 和一个胜出的阈值；多阈值只发生在用户显式 F5/F8。
- README、边界文档、性能测试协议和手动 CI 指南已同步自动定位与严格失败语义。

### Verification recorded locally

- Portable core: `arc_core: 750 checks, 0 failures`。
- Baseline formula guard: `BASELINE_OK distance=30.0000 high_arc=75.0000`。
- Clang ASan/UBSan（`detect_leaks=0`）与 GCC `-Wall -Wextra -Werror -pedantic`
  核心测试通过。
- 本地离线原始像素分析文件：`150/160/170` 同帧候选一致定位 `-52.25`，选择
  threshold `150`（分析文件受 `build/` 忽略且不会被运行时访问）。
- 禁用 API 静态扫描、workflow YAML 与 manifest XML 解析通过。

### Not yet verified

- 本变更尚未在 Windows MSVC、GitHub Actions 或用户实际游戏环境运行。
- 全屏 GDI 可见性、真实 HUD 缩放/色彩范围、CPU/FPS/GPU、READY 体验需按
  `PERFORMANCE_TEST.md` 在用户 Windows 环境测量。

## Historical releases

- `b2abeb3` — F9 可缩放紧凑 ROI；这是当前发布树在自动 OCR 同步前的基线。
- `e5f8eed` — Windows workflow 改为 `vswhere` 发现 MSVC；历史手动 run
  `31500665808` 在该提交成功。
- `9392ea2` — 初始 Windows 本地像素高抛辅助实现。
