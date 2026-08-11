---
name: apex-win-builder
description: Implements ultra-light native Win32 Arc Assist executable source
model: opencode-go/deepseek-v4-flash
tools: read,bash,edit,write
---
读取 TASK_SPEC.md 与已有 src/arc_core.h，只负责实现 src/apex_arc_assist.cpp、resources/app.manifest、resources/ApexArcAssist.rc、build-windows.cmd、build-mingw.sh。单线程消息循环；IDLE 零截图；仅小 ROI GDI BitBlt；本地生成模板+F5用户学习；自适应采样；overlay/hotkeys/config/perf统计。不得出现 WinRT OCR、GPU API、OBS、进程访问、Hook、输入合成。执行可行的语法/静态检查并报告。
