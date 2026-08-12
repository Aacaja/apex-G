# Apex Arc Assist — Agent Guide

> **先读本文件，再修改任何源码、测试、构建、文档或发布内容。**

## 目标与不可变约束

- Windows 原生高抛角度**计算器**；**不采集屏幕、不 OCR、不访问游戏**。
- 高抛公式必须保持：`x = 90 - degrees(asin(y / 60)) / 2`；30m → 75°，60m → 45°。
- 运行时只允许：热键（F8 距离、F9 标尺、Ctrl+Shift+Q 退出）、本地计算、
  自己的半透明置顶悬浮窗、exe 同目录 INI。
- 禁止：任何屏幕捕获（`GetDC`/`BitBlt`/`CreateDIBSection` 等）、系统 OCR、
  GPU/视频管线、进程/内存访问、注入、Hook、驱动、网络、合成输入、游戏窗口消息。
- 用户手动瞄准和投掷；工具只显示角度参照。
- 默认隐藏，仅 F9 显示；面板半透明、点击穿透、不抢焦点、不挡游戏数据。

## 工作方式

1. 修改前阅读 `TASK_SPEC.md`、`SECURITY_BOUNDARY.md`、本文件和 `docs/PROGRESS.md`。
2. 修改公式/核心时，`tests/baseline_test.cpp`（编译 `original/arc_core.h` 原件）
   必须继续通过。
3. 至少运行 `./test.sh` 与 `python3 scripts/verify_docs.py`；改动影响 Windows
   代码时检查 workflow YAML / manifest。
4. 无法本地运行 MSVC 时明确写明；Windows 构建/PE 审计经手动 CI 验证。
5. 更新 `docs/PROGRESS.md` 记录已验证事实与未验证项。
6. 发布经 `build/publish-repo/`（唯一可提交 Git 工作树），先创建回滚事务
   `artifacts/<transaction>/`（快照 + diff + SHA + ROLLBACK.sh，并在独立副本实测）。

## 关键路径

| 路径 | 责任 |
|---|---|
| `src/arc_core.h` | 高抛公式与容差（可移植，`original/arc_core.h` 的副本 + 扩展） |
| `src/apex_arc_assist.cpp` | Win32 窗口、热键、INI、标尺绘制 |
| `resources/` | manifest / 资源 |
| `tests/` | 可移植核心回归 + 原始公式守护 |
| `test.sh` | 核心测试 + 原始公式 + 禁止 API 扫描 |
| `.github/workflows/windows-build.yml` | **只允许 `workflow_dispatch`** |
| `build/publish-repo/` | 唯一可提交/push 的 Git 工作树 |

## 验收底线

```bash
./test.sh
python3 scripts/verify_docs.py
```

Windows CI 手动运行通过后，再按 `PERFORMANCE_TEST.md` 让用户实机验收。
