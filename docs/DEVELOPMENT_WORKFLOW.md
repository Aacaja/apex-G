# 开发与发布工作流

1. 修改前阅读 `AGENTS.md`、`TASK_SPEC.md`、`docs/PROGRESS.md`。
2. 本地验证（macOS）：

   ```bash
   cargo fmt --check
   cargo test --lib
   cargo test --test ocr_test
   ```

3. Windows 构建/使用一律交给 GitHub Actions（push main 自动触发
   `.github/workflows/build.yml`），不要在 macOS 上尝试 build Windows。
4. 发布事务：在 `artifacts/<transaction>/` 创建快照、diff、SHA 清单、MANIFEST
   与 `ROLLBACK.sh`（要求显式目标副本并在独立副本实测）。
5. 同步 `build/publish-repo/`（唯一可提交 Git 工作树）：复制清单文件、删除本
   事务废弃文件，在发布树重跑测试后提交并 push。
6. push 后查看 Actions run；未拿到日志前不声明 Windows 构建成功。
7. 用户按 `PERFORMANCE_TEST.md` 实机验收。
