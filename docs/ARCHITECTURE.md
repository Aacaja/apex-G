# 架构

```
┌────────────────────────────────────────────────────────┐
│ main.rs（Windows）                                      │
│  PeekMessage 循环（F9/Ctrl+Shift+Q）                    │
│  └─ 每帧（默认 60Hz）：                                  │
│     capture::grab_roi_gray()  xcap/WGC ROI 320×120      │
│        → ocr::binarize  (阈值150 + 近中性)               │
│        → ocr::segment   8邻域连通域                     │
│        → ocr::cluster_lines  行聚类(ref_h.max(4))       │
│        → templates::match  12×18 归一化 SAD/重叠         │
│        → ocr::find_ang_line  A-N-G + 冒号窗口校验       │
│        → ocr::recognize_pitch  负号门 → f64             │
│        → formula::distance_for_pitch  反解              │
│        → ui::Window::present  半透明悬浮窗               │
└────────────────────────────────────────────────────────┘
```

- `templates.rs`：离线嵌入 7×7 精确字形 + 5×7 兜底，启动时归一化为 12×18。
- `ui.rs`：LayeredWindow(alpha 205) + HTTRANSPARENT，左上 (12,96)，大字距离。
- 平台：`capture.rs`/`ui.rs`/Windows `main` 均 `#[cfg(windows)]`；macOS 只编译
  lib（formula/ocr/templates）跑测试。
