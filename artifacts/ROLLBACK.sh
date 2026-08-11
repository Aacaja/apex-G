#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-$ROOT/src/arc_core.h}"
case "$TARGET" in
  /*) ;;
  *) TARGET="$ROOT/$TARGET" ;;
esac
mkdir -p "$(dirname "$TARGET")"
cp "$ROOT/original/arc_core.h" "$TARGET"
EXPECTED="$(shasum -a 256 "$ROOT/original/arc_core.h" | awk '{print $1}')"
ACTUAL="$(shasum -a 256 "$TARGET" | awk '{print $1}')"
if [[ "$EXPECTED" != "$ACTUAL" ]]; then
  printf 'ROLLBACK_FAIL target=%s expected=%s actual=%s\n' "$TARGET" "$EXPECTED" "$ACTUAL" >&2
  exit 1
fi
printf 'ROLLBACK_OK target=%s sha256=%s restored=original/arc_core.h\n' "$TARGET" "$ACTUAL"
