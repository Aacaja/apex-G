# 开发与发布工作流

## 1. 修改前

1. 阅读根目录 [`../AGENTS.md`](../AGENTS.md)、`TASK_SPEC.md`、当前进度。
2. 确认根目录只作为开发工作区；不要在其中提交。
3. OCR 改动必须有脱敏二值 fixture 和跨平台回归；不要把 `pic.png` 或原始像素分析
   文件加入 Git。

## 2. 本地验证

在根目录运行：

```bash
./test.sh
clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -O1 -g \
  -fsanitize=address,undefined tests/test_arc_core.cpp \
  -o build/core/test_arc_core_asan
ASAN_OPTIONS=detect_leaks=0 ./build/core/test_arc_core_asan

g++ -std=c++17 -Wall -Wextra -Werror -pedantic -O2 \
  tests/test_arc_core.cpp -o build/core/test_arc_core_gcc
./build/core/test_arc_core_gcc

ruby -e 'require "yaml"; YAML.load_file(ARGV[0]); puts "WORKFLOW_YAML_PARSE_OK"' \
  .github/workflows/windows-build.yml
python3 -c 'import xml.etree.ElementTree as e; e.parse("resources/app.manifest"); print("MANIFEST_XML_PARSE_OK")'
```

macOS 不能构建 Windows GUI；不要把核心编译结果描述为 MSVC/游戏实机成功。

## 3. 创建发布事务证据

为每次发布事务创建独立目录（例如 `artifacts/auto-hud-ocr/`），至少包含：

- `MODIFIED_FILES.tar.gz`：要同步文件的可复现快照；
- `DIFF.patch`：相对已发布基线的差异；
- `VERIFICATION.txt`：命令、实际输出、哈希和未验证项；
- `ROLLBACK.sh`：在**目标副本**上恢复基线的脚本，且实际测试过。

artifact 中不得打包 `pic.png`、`build/` 分析图/像素、INI、模板或生成 EXE。

## 4. 同步、提交与推送

1. 用 `git -C build/publish-repo status --short --branch` 确认发布树状态。
2. 比较每一个要同步的文件；不要用整目录复制覆盖未知用户变更。
3. 同步后，在 `build/publish-repo/` 重跑 `./test.sh`、YAML/XML 解析和文档检查。
4. `git add` 仅本次清单，提交一个描述性 commit，push `main`。
5. 用 `git ls-remote origin refs/heads/main` 和本地 `HEAD` 核对推送结果。

## 5. Windows CI 与实机验证

工作流只允许 `workflow_dispatch`。push 不会自动运行；在 GitHub Actions 手动选择
**Build Windows x64 → Run workflow**。检查核心测试、MSVC GUI 构建、PE 导入审计、
SHA-256 和 artifact。之后按 `PERFORMANCE_TEST.md` 在用户 Windows 测试。

Windows CI、全屏可见性、性能或 READY 结论在未实际获得日志/测量前均标为未验证。
