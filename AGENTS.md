# Apex Arc Assist — Agent Guide

> **先读本文件，再修改任何源码、测试、构建、文档或发布内容。**
>
> 简短入口也见 [`agent.md`](agent.md)；当前可验证进度见
> [`docs/PROGRESS.md`](docs/PROGRESS.md)。

## 目标与不可变约束

- 项目是 Windows 原生外部高抛角度提示工具，不控制游戏。
- 高抛公式必须保持：`x = 90 - degrees(asin(y / 60)) / 2`。
  必须保留锚点：30m → 75°，60m → 45°。
- 运行时只允许：用户显式 F5/F8 的受限 GDI 屏幕读取、本地 OCR/计算、
  热键、自己的悬浮窗与提示音。
- 绝不使用游戏进程/内存访问、注入、Hook、驱动、网络、合成输入、游戏窗口
  消息、系统 OCR、GPU/视频捕获 API。完整边界见
  [`SECURITY_BOUNDARY.md`](SECURITY_BOUNDARY.md)。
- 用户手动瞄准和投掷；工具只能显示和提醒。
- IDLE、PAUSED、距离输入、F9 校准、训练输入状态没有周期截图。F5/F8 的
  一次性有界扫描是明确例外。

## 当前 OCR 方向

- 用户提供的本地 `pic.png` 是离线开发证据，**不得提交、运行时不得打开或保存**。
- 优先把 `cl_showpos` 的 `ANG:` 第一项 pitch 做成自动、直接、本地识别；
  `Shift+F5` 仅保留为最后的人工作为兜底。
- 用户接受约 50 MB 以内的本地依赖/资源，但增加体积不能突破上述 API 或数据边界。
- 自动路径必须 fail-closed：不确定 token、缺失/粘连符号、无符号非零、ROI 边缘截断
  或同帧阈值候选冲突都应为 `OCR LOST`，不得给出可能反向的角度。
- 任何“实际游戏中准确率、CPU、FPS、READY 表现”的结论均须标为待 Windows
  实机验证；不可臆测。

## 工作方式

1. 阅读相关源码、调用路径、此文件、`TASK_SPEC.md` 和当前进度日志。
2. 用最小改动修复根因；优先复用 `src/` 的可移植实现。
3. 修改 OCR 时，必须提供脱敏二值 fixture 和能捕获该失败模式的跨平台回归测试。
4. 运行最窄且有意义的验证；至少 `./test.sh` 与 `python3 scripts/verify_docs.py`，并在
   改动影响 Windows 代码时检查 workflow YAML / manifest。无法本地运行 MSVC 时明确写明。
5. 更新 `docs/PROGRESS.md`：记录已验证事实、未解决项、下一步和测试命令。
6. 同步实际发布树 `build/publish-repo/` 前先比较文件；根目录不是可写 Git 工作树。
7. 发布前创建可复现的 diff、验证记录和回滚脚本；不要覆盖用户已有文件。

## 关键路径

| 路径 | 责任 |
|---|---|
| `src/arc_core.h` | 公式、语法、二值字形、稳定/READY 可移植核心 |
| `src/apex_hud_ocr.h` | Apex/Source `ANG:` 自动定位与严格 token OCR |
| `src/apex_arc_assist.cpp` | Win32 GDI、热键、状态机、悬浮窗、配置 |
| `src/roi_geometry.h` | ROI 位置、缩放与虚拟桌面夹取 |
| `tests/test_arc_core.cpp` | macOS/Linux/Windows 可移植回归 |
| `tests/fixtures/` | 仅脱敏、二值、可提交的 OCR fixture |
| `test.sh` | 本地核心 / 原始公式 / 禁止 API 检查 |
| `.github/workflows/windows-build.yml` | **只允许 `workflow_dispatch`** 的 Windows 构建与导入审计 |
| `build/publish-repo/` | 唯一可提交/push 的 Git 工作树 |

## 验收底线

```bash
./test.sh
```

还应按改动比例补做：严格编译/ASan/UBSan、工作流 YAML 与 manifest XML 解析、
Windows CI 手动构建与 PE 导入审计。提交前在 `docs/PROGRESS.md` 中记录实际结果，
不要把未执行的步骤写成已通过。
