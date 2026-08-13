#\!/usr/bin/env bash
# Restore the ecfcc7e (C++ era) baseline into an explicit target copy.
set -euo pipefail
ARTIFACT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "$#" -ne 1 ]]; then
  printf 'Usage: %s TARGET_COPY\n' "${BASH_SOURCE[0]}" >&2
  exit 2
fi
TARGET="$1"
case "$TARGET" in
  /*) ;;
  *) TARGET="$PWD/$TARGET" ;;
esac
if [[ -z "$TARGET" || -d "$TARGET" ]]; then :; else
  printf 'RUST_REWRITE_ROLLBACK_FAIL missing_target=%s\n' "$TARGET" >&2
  exit 1
fi
tar -xzf "$ARTIFACT_DIR/BASELINE_FILES.tar.gz" -C "$TARGET"
while IFS= read -r rel || [[ -n "$rel" ]]; do
  [[ -z "$rel" ]] && continue
  rm -f "$TARGET/$rel"
done < "$ARTIFACT_DIR/NEW_FILES.txt"
printf 'RUST_REWRITE_ROLLBACK_OK target=%s\n' "$TARGET"
