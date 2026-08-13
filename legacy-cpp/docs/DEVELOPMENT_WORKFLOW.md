# 开发与发布工作流

1. 修改前阅读 `AGENTS.md`、`TASK_SPEC.md`、`docs/PROGRESS.md`。
2. 本地验证：

   ```bash
   ./test.sh
   python3 scripts/verify_docs.py
   ruby -e 'require "yaml"; YAML.load_file(ARGV[0]); puts "WORKFLOW_YAML_PARSE_OK"' .github/workflows/windows-build.yml
   ```

   macOS 不能构建 Windows GUI；不要把核心编译结果描述为 MSVC 成功。
3. 发布事务：在 `artifacts/<transaction>/` 创建快照、diff、SHA 清单、MANIFEST
   和 `ROLLBACK.sh`（要求显式目标副本并在独立副本实测）。
4. 同步 `build/publish-repo/`：只复制清单内文件，删除本事务废弃文件，
   在发布树重跑测试后提交并 push。
5. 手动触发 `workflow_dispatch` Windows CI；未拿到 run 日志前不声明 MSVC/PE 成功。
6. 用户按 `PERFORMANCE_TEST.md` 实机验收。
