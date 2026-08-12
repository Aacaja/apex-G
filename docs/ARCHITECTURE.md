# 架构与数据流

## 组件

| 组件 | 路径 | 职责 |
|---|---|---|
| 可移植核心 | `src/arc_core.h` | 高抛公式、pitch 归一化/符号连续性、二值网格、哈希、稳定器、READY 门 |
| HUD 自动 OCR | `src/apex_hud_ocr.h` | `ANG:` 空间定位、截图派生模板、缩放变体、完整 token/符号/ROI 边界校验、多阈值一致性 |
| Win32 应用 | `src/apex_arc_assist.cpp` | GDI DIB、热键、状态机、配置、悬浮窗、调度、F5/F8 一次扫描 |
| ROI 几何 | `src/roi_geometry.h` | 最小/最大尺寸、居中缩放、虚拟桌面夹取 |
| 资源 | `resources/` | `asInvoker`、PerMonitorV2 manifest、版本资源 |
| 回归 | `tests/` | 纯 C++ 核心、公式基线、脱敏二值 HUD fixture |

## F5/F8 自动定位数据流

```text
用户按 F5/F8
  └─ 每个显示器：左上 min(720×280, 显示器大小)
      └─ 一次 BitBlt → top-down 32-bit DIB → GdiFlush
          ├─ 阈值 150 / 160 / 170
          └─ 当前 F9 阈值（若不同）
              └─ 同一帧内生成多个 BinaryGrid
                  └─ 严格 ANG: 定位
                      ├─ 必须有 A→N→G、同一基线、左列、冒号
                      ├─ 必须是首个完整 `[+-]?dd.dd` token
                      ├─ 非零必须有可见、未粘连 +/-
                      └─ 成功阈值必须 token/ROI 互相兼容
                          └─ 选择最高置信的同意候选
                              └─ 保存小 ROI 和胜出阈值；释放大 DIB
```

任一 token、符号、ROI 或阈值候选发生歧义时，不尝试猜测方向，返回 `OCR LOST`。

## ARMED 数据流

```text
CanCapture() = normal mode && armed && !paused
  └─ WM_TIMER（仅 5Hz / ~12Hz / 3Hz / OCR backoff）
      └─ 一次小 ROI BitBlt + 一张二值网格
          └─ FNV-1a 相同？
              ├─ 是：复用上次严格识别
              └─ 否：Apex token 模板识别
                  ├─ token 必须完整、留边、非零有 +/-
                  ├─ PitchSignContinuity / 稳定器
                  └─ pitch → 当前仰角 → READY gate → overlay / 可选 beep
```

多阈值大扫描不在 ARMED 定时路径中。IDLE、PAUSED、F9 校准、距离/训练输入均不启用
周期性 `BitBlt`。

## 失败语义

- 大扫描失败：保留旧 ROI；若旧 ROI 是自动 ROI，仍保持严格 Apex 模板路径。
- 缺失、粘连、裁切、无符号非零或冲突 token：`OCR LOST`。
- `OCR LOST` 立即撤销 READY，并按 4 → 2 → 1Hz 退避；不会发送游戏输入。
- F9/Shift+F5 是显式人工恢复路径，但仍不能把无符号非零 token 当作方向可信。

## 生命周期

`CaptureSurface` 复用一个屏幕 DC、内存 DC 和 DIB；尺寸变化或自动扫描结束时 `Reset()`
先恢复选入对象、删除 DIB/DC，再释放屏幕 DC。`Shutdown()` 是幂等的：停止 timer、注销
热键、释放捕获资源、销毁 ROI/overlay 窗口，再释放 GDI 字体/画笔/刷子。
