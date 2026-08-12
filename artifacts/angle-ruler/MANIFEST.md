# Angle-ruler transaction manifest

- Baseline: ecfcc7e (OCR 版，含 `apex_hud_ocr.h`、`roi_geometry.h`、`tests/fixtures/`、`artifacts/auto-hud-ocr/`）。
- Candidate: 纯角度计算器（零屏幕采集）。`PRODUCT_FILES.txt` 列出 22 个产品文件；
  本事务删除了全部 OCR 相关文件并新增半透明标尺 UI。
- `MODIFIED_FILES.tar.gz` / `BASELINE_FILES.tar.gz`：候选/基线快照。
- `DIFF.patch`：相对路径统一 diff；`*_SHA256SUMS.txt`：逐文件哈希。
- `ROLLBACK.sh`：要求显式目标副本，恢复完整基线树并移除新增文件。
