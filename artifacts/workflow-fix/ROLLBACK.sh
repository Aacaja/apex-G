#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET="${1:-$ROOT/.github/workflows/windows-build.yml}"
case "$TARGET" in
  /*) ;;
  *) TARGET="$ROOT/$TARGET" ;;
esac
mkdir -p "$(dirname "$TARGET")"
cp "$ROOT/original/windows-build.failure.yml" "$TARGET"
EXPECTED="$(shasum -a 256 "$ROOT/original/windows-build.failure.yml" | awk '{print $1}')"
ACTUAL="$(shasum -a 256 "$TARGET" | awk '{print $1}')"
if [[ "$EXPECTED" != "$ACTUAL" ]]; then
  printf 'WORKFLOW_ROLLBACK_FAIL target=%s expected=%s actual=%s\n' "$TARGET" "$EXPECTED" "$ACTUAL" >&2
  exit 1
fi
printf 'WORKFLOW_ROLLBACK_OK target=%s sha256=%s restored=original/windows-build.failure.yml\n' "$TARGET" "$ACTUAL"
