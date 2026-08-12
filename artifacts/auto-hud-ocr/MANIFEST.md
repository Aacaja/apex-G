# Automatic HUD OCR transaction manifest

- Published baseline: `b2abeb3de86c0c2991416a801388936a65c55459` (`main` before this transaction).
- Candidate scope: `PRODUCT_FILES.txt`; it excludes `pic.png`, all `build/` content, INI/template state,
  binaries, and the artifact directory itself.
- `MODIFIED_FILES.tar.gz`: exactly the candidate product files.
- `BASELINE_FILES.tar.gz`: only baseline versions of files that existed at `b2abeb3`.
- `NEW_FILES.txt`: candidate files absent from the baseline and removed by `ROLLBACK.sh`.
- `ROLLBACK.sh`: requires an explicit target copy, verifies every restored baseline SHA-256, and
  intentionally keeps this audit directory. It never defaults to overwriting the current source tree.
- The runtime never opens `pic.png`; fixture headers are binary foreground only.
