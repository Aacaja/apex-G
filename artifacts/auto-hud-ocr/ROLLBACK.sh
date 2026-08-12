#!/usr/bin/env bash
# Restore the b2abeb3 published-file baseline into a target copy.
# This script deliberately retains artifacts/auto-hud-ocr/ as audit evidence.
set -euo pipefail
ARTIFACT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(cd "$ARTIFACT_DIR/../.." && pwd)"
if [[ "$#" -ne 1 ]]; then
  printf 'Usage: %s TARGET_COPY\n' "${BASH_SOURCE[0]}" >&2
  exit 2
fi
TARGET="$1"
case "$TARGET" in
  /*) ;;
  *) TARGET="$PWD/$TARGET" ;;
esac
if [[ ! -d "$TARGET" ]]; then
  printf 'AUTO_HUD_OCR_ROLLBACK_FAIL missing_target=%s\n' "$TARGET" >&2
  exit 1
fi
ARCHIVE="$ARTIFACT_DIR/BASELINE_FILES.tar.gz"
SUMS="$ARTIFACT_DIR/BASELINE_SHA256SUMS.txt"
NEW_FILES="$ARTIFACT_DIR/NEW_FILES.txt"
[[ -f "$ARCHIVE" && -f "$SUMS" && -f "$NEW_FILES" ]] || {
  printf 'AUTO_HUD_OCR_ROLLBACK_FAIL incomplete_artifact=%s\n' "$ARTIFACT_DIR" >&2
  exit 1
}
tar -xzf "$ARCHIVE" -C "$TARGET"
while IFS= read -r rel || [[ -n "$rel" ]]; do
  [[ -z "$rel" ]] && continue
  rm -f "$TARGET/$rel"
done < "$NEW_FILES"
while IFS='  ' read -r expected rel || [[ -n "${expected:-}" ]]; do
  [[ -z "${expected:-}" ]] && continue
  actual="$(shasum -a 256 "$TARGET/$rel" | awk '{print $1}')"
  if [[ "$actual" != "$expected" ]]; then
    printf 'AUTO_HUD_OCR_ROLLBACK_FAIL file=%s expected=%s actual=%s\n' \
      "$rel" "$expected" "$actual" >&2
    exit 1
  fi
done < "$SUMS"
printf 'AUTO_HUD_OCR_ROLLBACK_OK target=%s restored=%s removed=%s\n' \
  "$TARGET" "$(wc -l < "$SUMS" | tr -d ' ')" \
  "$(grep -cve '^$' "$NEW_FILES" || true)"
