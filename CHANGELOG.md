# Changelog

本文件仅记录已落地的源码与文档变更；Windows 实机行为、性能和 CI 结果以各次
实际运行记录为准，不以本文替代验证证据。

## 2026-08-12 — 重构：纯高抛角度计算器（不再 OCR）

用户实测表明 HUD OCR 提取效果不佳，已整体弃用截图/识别路径：

- 删除全部屏幕采集与 OCR：`apex_hud_ocr.h`、`roi_geometry.h`、`tests/fixtures/`、
  多阈值定位、模板识别、符号连续性、稳定器/READY 状态机。
- 新增半透明角度标尺：右上角、点击穿透、不抢焦点、默认隐藏；F9 显示/隐藏。
- 标尺内容：10/20/30/40/50/60m → 85.2/80.3/75.0/69.1/61.8/45.0°（大字号角度）。
- F8 输入距离（1–60m，小数可用；空 Enter 复用上次）；输入后高亮对应行并显示
  “当前目标 xxm → xx.x°”。
- 进程零采集：IDLE 阻塞于 `GetMessageW`，无定时器、无 BitBlt。
- `ApexArcAssist.ini` 仅保存上次距离。
- 核心测试重写：公式锚点/无效/单调/容差/标尺表（`arc_core: 287 checks, 0 failures`）。

## 2026-08-12 — automatic `ANG:` pitch OCR hardening（`f133a1c`，已被重构取代）

Windows CI run `31610004758` 曾成功（MSVC 750 checks、EXE 构建、PE 导入审计）。
该 OCR 版本现已废弃。

## Historical releases

- `ecfcc7e` — OCR 版发布记录。
- `b2abeb3` — F9 可缩放紧凑 ROI。
- `e5f8eed` — Windows workflow 改为 `vswhere` 发现 MSVC。
- `9392ea2` — 初始 Windows 本地像素高抛辅助实现。
