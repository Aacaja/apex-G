# 开发进度日志

> 本日志只记录已观察到的证据；不把尚未在 Windows CI 或用户实际游戏中执行的结果写成结论。
> 最近更新：2026-08-12（自动 HUD OCR 发布事务准备中）。

## 当前目标

使用用户提供的本地 `pic.png` 作为**仅离线开发**参考，自动识别 `cl_showpos` 的
`ANG:` 后首个 pitch，减少对 F5 人工模板训练的依赖，同时保持：

- 精确高抛公式 `x = 90 - degrees(asin(y / 60)) / 2`；30m → 75°，60m → 45°。
- 只读像素、本地 OCR/数学、热键、自己的 overlay/提示音；不访问或控制游戏。
- 自动路径在 token、符号、ROI 边界或阈值候选不可信时 **fail-closed** 为 `OCR LOST`。

`pic.png` 为 2560×1440，SHA-256：
`eb0056aea5990a5695a8c2078a4cbddf60f44d8fa9087d9edf460459fdb13de0`。
它被 `.gitignore` 排除；运行时代码不打开、保存或传输该文件。

## 本次已完成

### 自动定位与同帧多阈值

- F5 强制自动定位，F8 在距离输入前自动定位；Shift+F5 保留为人工模板学习兜底。
- 每块显示器只在用户显式 F5/F8 时捕获一次左上最大 `720×280` 区域。`BitBlt` 后调用
  `GdiFlush()`，同一张内存 DIB 派生 threshold `150 / 160 / 170`，并在当前 F9 阈值不同
  时附加该值；没有第二次抓屏。
- 成功阈值候选必须识别到相同 token 且 ROI 大幅重叠；不同成功候选直接拒绝，避免按
  单个最高分盲选。胜出的阈值供后续 ARMED 小 ROI 使用。
- 大 DIB/二值掩码在每块显示器扫描后释放；ARMED 仍只以一个小 ROI、一个阈值周期采集。

### 严格 OCR 与方向安全

- 自动定位要求同一行内、左列起始的 `A → N → G`、紧邻冒号及其后的首个完整两位小数
  token，避免 POS/VEL 的相似组件伪装 `ANG:`。
- 使用截图派生二值模板和缩放/相位变体；75% 缩放导致的数字碎片仅在已确认的文本基线内
  合并，避免跨 VEL 行合并。
- 非零 pitch 必须有可见、未粘连的 `+/-`；自动 ROI 还必须在 token 两侧留足边缘。缺失/
  粘连符号、裁切 ROI、无符号非零、内部后缀（例如 `-52.25` 被错切为 `2.25`）、不完整
  token 和多阈值冲突都返回 `OCR LOST`。
- `PitchSignContinuity` 不再接受无符号非零近零读数，防止截断的高角度后缀成为“接近
  水平”的方向提示。
- F9 ROI 红框现在处理 `WM_NCHITTEST → HTTRANSPARENT`；F9/Shift+F5 的显式人工恢复
  允许紧贴的完整带符号 token，但自动 ARMED 路径仍要求 token 留边。显示模式变化后停止
  旧 ROI 采集，直到 F5 自动重定位或用户确认 F9/Shift+F5 恢复。`Shutdown()` 幂等释放
  timer、热键、CaptureSurface、ROI/overlay 窗口和 GDI 对象。manifest 的 PerMonitorV2
  DPI 声明保留，源码不再调用 `SetProcessDPIAware()` 覆盖它。

### Fixture 与回归

- `tests/fixtures/apex_hud_pic_2560x1440_mask.h`：只含目标 `ANG: -52.25` 二值前景。
- `tests/fixtures/apex_hud_pic_multiline_mask.h`：240×55、13,200 像素、1,409 前景像素；
  只保留 NAME 行以下的二值 HUD 前景，包含相邻行干扰，但无 RGB 场景、玩家昵称或原图。
- `tests/fixtures/apex_hud_glyphs_pic_mask.h`：离线派生的脱敏数字字形 fixture。
- 可移植测试覆盖原尺寸、75%/125%/150%/200% 缩放、非零栅格相位、空图、缺失/粘连减号、
  右偏 72×18 ROI 后缀、多阈值一致/冲突、稳定器、ROI 几何、READY 等。

## 已执行验证

| 检查 | 实际结果 |
|---|---|
| `./test.sh` | `arc_core: 750 checks, 0 failures`；`BASELINE_OK distance=30.0000 high_arc=75.0000`；`FORBIDDEN_API_SCAN_OK runtime_source=src,resources` |
| Clang ASan/UBSan | `ASAN_OPTIONS=detect_leaks=0` 下 `arc_core: 750 checks, 0 failures`；macOS 不支持 AddressSanitizer leak detector，因此显式关闭该选项 |
| GCC 严格核心编译 | `-std=c++17 -Wall -Wextra -Werror -pedantic -O2`，`arc_core: 750 checks, 0 failures` |
| workflow / manifest | Ruby YAML 解析 `WORKFLOW_YAML_PARSE_OK`；Python XML 解析 `MANIFEST_XML_PARSE_OK` |
| 原始像素离线 probe | 同一张已忽略 `720×280` RGB 分析输入在 150/160/170 三个掩码均能互相佐证；`MULTI_THRESHOLD_REAL_OK selected_threshold=150 token=-52.25 sign=1 confidence=0.994 roi=48,54,53x13` |
| 75% 严格 ARMED 模型 | 16 个相位：8 个识别 `-52.25`，8 个 `OCR LOST`，0 个其他 token 成功 |
| 125% / 150% 严格 ARMED 模型 | 125% 16/16、150% 4/4 识别 `-52.25`，0 个其他 token 成功 |
| 0.5× 模型 | 无定位，按设计 fail-closed |

本地分析程序与 RGB 文件位于 `build/`（已忽略），不是运行时依赖，也不随发布事务打包。

## 未验证 / 不能声明

- 本次源码尚未在 Windows MSVC、GitHub Actions 或 PE 导入审计上实际运行。
- 用户 Windows 上的独占全屏 GDI 可见性、F5/F8 成功率、正/负/零 pitch、HUD 色彩/字体、
  1080p/1440p/4K/多显示器表现仍需实测。
- CPU、工作集、GPU 引擎、FPS、帧时间和 READY 使用体验均未测量；按
  [`../PERFORMANCE_TEST.md`](../PERFORMANCE_TEST.md) 进行。

## 发布状态与下一步

- 开发根目录不是有效 Git worktree；唯一可提交树是 `build/publish-repo/`，当前仍在
  `b2abeb3 Add resizable compact calibration ROI`，尚未包含自动 OCR 事务。
- 发布前：创建 diff/快照/验证/rollback artifact，将经过比较的文件同步到发布树，在该树
  重跑核心与文档检查，提交并 push。
- push 后：仅手动触发 `workflow_dispatch` Windows CI；获得 run 日志前不声明 MSVC、PE
  导入或 EXE artifact 成功。
- 最后由用户按 Windows 验收清单验证真实画面与性能。
