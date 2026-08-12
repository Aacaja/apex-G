# 当前状态

更新：2026-08-12（本地 OCR 事务完成，尚未发布）

## 已完成并本地验证

- 高抛公式守护：30m → 75°、60m → 45°。
- `ANG: -52.25` 自动定位、完整 token OCR、自动 ROI 填充后的严格识别。
- 同一张 F5/F8 GDI DIB 派生 `150/160/170` 与当前 F9 阈值候选；候选 token/ROI
  冲突时 fail-closed。
- 缺失/粘连减号、截断 ROI、token 内部后缀、非零无符号值、0.75× 小字和相邻 HUD
  行干扰不产生反向成功结果。
- F9 ROI 红框点击穿透；F9/Shift+F5 可接受完整带符号的紧贴手动 token；`BitBlt` 后
  `GdiFlush()`；显示模式变化后停止旧 ROI 采集，直到 F5 或显式手动恢复；`Shutdown()`
  幂等销毁窗口和捕获资源；manifest PerMonitorV2 不再被 `SetProcessDPIAware()` 覆盖。
- 本地验证结果：
  - `./test.sh` → `arc_core: 750 checks, 0 failures`、`BASELINE_OK`、
    `FORBIDDEN_API_SCAN_OK`。
  - Clang ASan/UBSan（`ASAN_OPTIONS=detect_leaks=0`）和 GCC 严格核心测试通过。
  - Ruby workflow YAML 解析、Python manifest XML 解析通过。
  - 离线原始像素 probe：同帧阈值 `150/160/170` 一致定位 `-52.25`，选择 150。

## 已知限制（故意 fail-closed）

- 所建模 75% HUD 栅格相位中，8/16 能完整识别 `-52.25`，另 8/16 返回无结果；
  没有替代 token 成功。小字物理丢失符号时优先 `OCR LOST`，而非猜测。
- 约 0.5×（1280×720 等效）fixture 文字不受支持，返回无定位。
- fixture 只证明已提供截图的二值模型，不代表其它 HUD 色彩、字体、缩放、分辨率或
  游戏版本。

## 未验证（发布前/用户 Windows）

1. GitHub Actions/MSVC 编译和 PE 导入审计：只能在同步、提交、push 后手动运行。
2. 用户 Windows 实机：F5/F8、F9/Shift+F5 恢复、正/负/零 pitch、1080p/1440p/4K、
   多显示器、全屏 GDI 可见性。
3. 性能与体验：CPU、工作集、GPU 引擎、FPS、帧时间、READY 提示，按
   [`../PERFORMANCE_TEST.md`](../PERFORMANCE_TEST.md) 执行。

## 发布状态

根目录是开发工作区，不是可写 Git worktree。唯一发布树是
`build/publish-repo/`，目前仍停在 `b2abeb3`，尚未同步自动 OCR 代码、fixture、文档
或本次事务证据。同步前必须比较清单、在发布树重跑测试，并保留回滚 artifact。
