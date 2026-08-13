# 开发进度日志（角度标尺版）

> 本日志只记录已观察到的证据。最近更新：2026-08-12（重构为纯角度计算器）。

## 当前目标

放弃 HUD OCR（实机识别效果不佳），改为**纯高抛角度计算器**：

- 不读取任何屏幕像素、不做 OCR、不访问游戏。
- F8 输入距离（1–60m）；F9 显示/隐藏半透明角度标尺。
- 标尺：10/20/30/40/50/60m → 85.2/80.3/75.0/69.1/61.8/45.0°。
- 用户自己目视角度判断投掷时机；工具只提供参照。

## 已完成

- `src/arc_core.h` 精简为公式 + 容差（30m→75°、60m→45° 守护保留）。
- `src/apex_arc_assist.cpp` 重写：无任何采集代码；F8 输入、F9 切换、右上角
  半透明（LWA_ALPHA 208）点击穿透面板；输入后高亮对应行并显示当前目标角度；
  默认隐藏；INI 只存上次距离。
- 删除 `apex_hud_ocr.h`、`roi_geometry.h`、`tests/fixtures/` 及全部 OCR 相关。
- `tests/test_arc_core.cpp` 重写：公式锚点/无效/单调/容差/标尺表。

## 已执行验证

| 检查 | 实际结果 |
|---|---|
| `./test.sh` | `arc_core: 287 checks, 0 failures`；`BASELINE_OK distance=30.0000 high_arc=75.0000`；`FORBIDDEN_API_SCAN_OK` |
| 静态扫描 | 含 `GetDC\|BitBlt\|CreateDIBSection` 在内的禁用 API 扫描通过 |

## 待验证

- Windows MSVC 构建、PE 导入审计：push 后手动 CI（`workflow_dispatch`）。
- 用户 Windows 实机：F8/F9 交互、半透明可读性、不遮挡游戏数据、CPU 零占用。

## 发布状态

开发根目录不是 Git worktree；发布经 `build/publish-repo/` 提交推送，并附带
`artifacts/angle-ruler/` 回滚事务。

## 发布状态

- 已发布 `7281ce9`（纯角度计算器）；Windows CI run `31625003274` 全部成功：
  - `arc_core: 287 checks, 0 failures`、`BASELINE_OK 30m→75.0000°`
  - `DOC_VERIFY_OK documents=14 core_checks=287 workflow=manual_only`
  - `BUILT: ...dist\ApexArcAssist.exe`；`PE_IMPORT_SCAN_OK`
  - EXE SHA-256 `f9a1c9ca…`；artifact `ApexArcAssist-windows-x64`（161386 字节）
- 回滚事务 `artifacts/angle-ruler/`（已在独立副本实测恢复 ecfcc7e）。
- 剩余：用户 Windows 实机验收（F8/F9 交互、半透明可读性、CPU 零占用）。

## UI 修复（2026-08-12，提交 7d526a7）

- 位置：左上角 `(work.left+12, work.top+96)`。以用户 2560×1440 截图为参照实测：
  HUD 文本块 y≈36..76（NAME 行 36–42，POS/ANG/VEL 行 46–76），面板顶部 y=96，
  与 HUD 数字保持 20px 间距。
- 布局：行高 30→38，每行距离/角度同基线排版；24px 粗体角度不再与下一行重叠。
- 半透明：LWA_ALPHA 208→185，明显半透明。
- Windows CI run `31625764845` 通过：EXE SHA-256 `d5b5a73d…`，artifact 161850 字节。
