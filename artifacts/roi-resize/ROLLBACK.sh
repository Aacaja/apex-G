#!/usr/bin/env bash
set -euo pipefail
SOURCE_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TARGET_ROOT="${1:-$SOURCE_ROOT}"
case "$TARGET_ROOT" in
  /*) ;;
  *) TARGET_ROOT="$SOURCE_ROOT/$TARGET_ROOT" ;;
esac
mkdir -p "$TARGET_ROOT/src" "$TARGET_ROOT/tests"
cp "$SOURCE_ROOT/original/roi-resize/src/apex_arc_assist.cpp" \
   "$TARGET_ROOT/src/apex_arc_assist.cpp"
cp "$SOURCE_ROOT/original/roi-resize/src/arc_core.h" \
   "$TARGET_ROOT/src/arc_core.h"
cp "$SOURCE_ROOT/original/roi-resize/tests/test_arc_core.cpp" \
   "$TARGET_ROOT/tests/test_arc_core.cpp"
cp "$SOURCE_ROOT/original/roi-resize/docs/README.md" "$TARGET_ROOT/README.md"
cp "$SOURCE_ROOT/original/roi-resize/docs/TASK_SPEC.md" "$TARGET_ROOT/TASK_SPEC.md"
cp "$SOURCE_ROOT/original/roi-resize/docs/GITHUB_ACTIONS_GUIDE.md" \
   "$TARGET_ROOT/GITHUB_ACTIONS_GUIDE.md"
cp "$SOURCE_ROOT/original/roi-resize/docs/PERFORMANCE_TEST.md" \
   "$TARGET_ROOT/PERFORMANCE_TEST.md"
cp "$SOURCE_ROOT/original/roi-resize/docs/SECURITY_BOUNDARY.md" \
   "$TARGET_ROOT/SECURITY_BOUNDARY.md"
rm -f "$TARGET_ROOT/src/roi_geometry.h"

check_pair() {
  local expected="$1" actual="$2"
  local expected_hash actual_hash
  expected_hash="$(shasum -a 256 "$expected" | awk '{print $1}')"
  actual_hash="$(shasum -a 256 "$actual" | awk '{print $1}')"
  [[ "$expected_hash" == "$actual_hash" ]]
}
check_pair "$SOURCE_ROOT/original/roi-resize/src/apex_arc_assist.cpp" \
           "$TARGET_ROOT/src/apex_arc_assist.cpp"
check_pair "$SOURCE_ROOT/original/roi-resize/tests/test_arc_core.cpp" \
           "$TARGET_ROOT/tests/test_arc_core.cpp"
if [[ -e "$TARGET_ROOT/src/roi_geometry.h" ]]; then
  echo "ROI_RESIZE_ROLLBACK_FAIL new header still exists" >&2
  exit 1
fi
printf 'ROI_RESIZE_ROLLBACK_OK target=%s restored=pre-resize removed=src/roi_geometry.h\n' \
  "$TARGET_ROOT"
