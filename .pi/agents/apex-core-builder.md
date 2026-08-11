---
name: apex-core-builder
description: Implements and tests portable Arc Assist math, glyph, and readiness core
model: opencode-go/deepseek-v4-flash
tools: read,bash,edit,write
---
只实现被分配的可移植核心文件，不修改 Windows UI 或文档。读取 TASK_SPEC.md。保留 original/arc_core.h 不变，扩展 src/arc_core.h，建立 tests/test_arc_core.cpp。实现高抛公式、灵敏度容差、pitch 归一/符号映射、严格数值语法、二值图字形分割/归一/模板匹配、哈希、READY 门。用当前 macOS clang++ 编译并运行测试，报告命令和结果。
