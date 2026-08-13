#!/usr/bin/env python3
"""Small, dependency-free consistency check for active project documentation."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
CORE_CHECKS = 287

REQUIRED_FILES = (
    "AGENTS.md",
    "agent.md",
    "CHANGELOG.md",
    "README.md",
    "TASK_SPEC.md",
    "SECURITY_BOUNDARY.md",
    "PERFORMANCE_TEST.md",
    "GITHUB_ACTIONS_GUIDE.md",
    "docs/PROGRESS.md",
    "docs/PROJECT_OVERVIEW.md",
    "docs/ARCHITECTURE.md",
    "docs/CURRENT_STATUS.md",
    "docs/DEVELOPMENT_HISTORY.md",
    "docs/DEVELOPMENT_WORKFLOW.md",
)

ACTIVE_DOCS = (
    "AGENTS.md",
    "agent.md",
    "CHANGELOG.md",
    "README.md",
    "TASK_SPEC.md",
    "SECURITY_BOUNDARY.md",
    "PERFORMANCE_TEST.md",
    "GITHUB_ACTIONS_GUIDE.md",
    "docs/PROGRESS.md",
    "docs/PROJECT_OVERVIEW.md",
    "docs/ARCHITECTURE.md",
    "docs/CURRENT_STATUS.md",
    "docs/DEVELOPMENT_HISTORY.md",
    "docs/DEVELOPMENT_WORKFLOW.md",
)

LINK_RE = re.compile(r"(?<!\!)\[[^]]*\]\(([^)]+)\)")


def fail(message: str, failures: list[str]) -> None:
    failures.append(message)


def local_link_target(source: Path, raw_target: str) -> Path | None:
    target = raw_target.strip()
    if not target or target.startswith(("#", "http://", "https://", "mailto:")):
        return None
    target = target.split("#", 1)[0].strip()
    if not target:
        return None
    # Markdown allows an optional title after a whitespace separator.
    target = target.split(maxsplit=1)[0].strip("<>")
    return (source.parent / unquote(target)).resolve()


def main() -> int:
    failures: list[str] = []
    for rel in REQUIRED_FILES:
        if not (ROOT / rel).is_file():
            fail(f"missing required document: {rel}", failures)

    for rel in ACTIVE_DOCS:
        path = ROOT / rel
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for target_raw in LINK_RE.findall(text):
            target = local_link_target(path, target_raw)
            if target is not None and not target.exists():
                fail(f"broken local link in {rel}: {target_raw}", failures)

    for rel in ("docs/PROGRESS.md", "docs/CURRENT_STATUS.md", "CHANGELOG.md"):
        path = ROOT / rel
        if path.is_file() and f"{CORE_CHECKS} checks, 0 failures" not in path.read_text(
            encoding="utf-8"
        ):
            fail(f"{rel} does not record the current {CORE_CHECKS}-check result", failures)

    stale = "arc_core: 610 checks, 0 failures"
    for rel in ACTIVE_DOCS:
        path = ROOT / rel
        if path.is_file() and stale in path.read_text(encoding="utf-8"):
            fail(f"stale core-check count in active document: {rel}", failures)

    gitignore = ROOT / ".gitignore"
    if not gitignore.is_file() or "pic.png" not in gitignore.read_text(encoding="utf-8"):
        fail(".gitignore must retain pic.png", failures)

    workflow = ROOT / ".github/workflows/windows-build.yml"
    if not workflow.is_file():
        fail("missing manual Windows workflow", failures)
    else:
        workflow_text = workflow.read_text(encoding="utf-8")
        if "workflow_dispatch:" not in workflow_text:
            fail("workflow must retain workflow_dispatch", failures)
        if re.search(r"^\s*(push|pull_request):", workflow_text, re.MULTILINE):
            fail("workflow must remain manual-only", failures)
        if "scripts\\verify_docs.py" not in workflow_text:
            fail("Windows workflow must run scripts\\verify_docs.py", failures)

    if failures:
        for message in failures:
            print(f"DOC_VERIFY_FAIL {message}", file=sys.stderr)
        return 1
    print(
        "DOC_VERIFY_OK "
        f"documents={len(REQUIRED_FILES)} core_checks={CORE_CHECKS} "
        "workflow=manual_only"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
