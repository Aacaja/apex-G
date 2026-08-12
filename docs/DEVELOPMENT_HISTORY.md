# 开发历史

- `9392ea2` — 初始像素级高抛辅助（截图+模板识别）。
- `e5f8eed` — Windows workflow 改用 `vswhere`；历史手动 run `31500665808` 成功。
- `b2abeb3` — F9 可缩放紧凑 ROI。
- `f133a1c` / `ecfcc7e` — 自动 ANG OCR 版（同帧多阈值、fail-closed 符号门）；
  手动 CI run `31610004758` 通过。
- 本次 — **重构为纯角度计算器**：删除全部采集/OCR，新增半透明标尺 UI。

用户实测 OCR 提取效果不佳，是本次整体推翻策略的直接原因。
