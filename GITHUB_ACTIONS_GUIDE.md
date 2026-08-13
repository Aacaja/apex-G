# GitHub Actions 构建指南（Rust 版）

工作流：`.github/workflows/build.yml`，**push 到 main 自动触发**（也可在
Actions 页面手动 Run workflow）。

构建内容（`windows-latest`）：
1. `cargo fmt --check`
2. `cargo test --lib` + `cargo test --test ocr_test`（公式 + OCR fixture 回归）
3. `cargo build --release`
4. 上传 artifact `apex-angle-reader`（`apex-angle-reader.exe` + README.md，保留 30 天）

重点日志：

```text
test result: ok. 3 passed   (lib)
test result: ok. 5 passed   (ocr_test)
Finished `release` profile
```

下载：Actions → 最新 run → Artifacts → `apex-angle-reader`。

实机验收：按 `PERFORMANCE_TEST.md` 执行（锚点/灵敏度/CPU/全屏捕获）。
