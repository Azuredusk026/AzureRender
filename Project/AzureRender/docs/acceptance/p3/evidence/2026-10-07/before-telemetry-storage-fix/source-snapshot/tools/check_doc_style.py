#!/usr/bin/env python3
"""Reject long Chinese prose sentences in active Markdown documents."""

from __future__ import annotations

import re
import sys
from glob import glob
from pathlib import Path


MAX_CJK_PER_SENTENCE = 45
MAX_SENTENCES_PER_PARAGRAPH = 4
CJK_PATTERN = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")
SENTENCE_PATTERN = re.compile(r"[。！？.!?]")


def check_document(path: Path) -> list[str]:
    errors: list[str] = []
    in_fence = False
    in_math = False

    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8").splitlines(), start=1
    ):
        stripped = raw_line.strip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        if stripped == "$$":
            in_math = not in_math
            continue
        if in_fence or in_math:
            continue
        if not stripped or stripped.startswith(("|", "![", "<!--")):
            continue

        sentence_count = len(re.findall(r"[。！？]", stripped))
        if sentence_count > MAX_SENTENCES_PER_PARAGRAPH:
            errors.append(
                f"{path}:{line_number}: paragraph has {sentence_count} sentences "
                f"(limit {MAX_SENTENCES_PER_PARAGRAPH})"
            )

        for sentence in SENTENCE_PATTERN.split(stripped):
            cjk_count = len(CJK_PATTERN.findall(sentence))
            if cjk_count > MAX_CJK_PER_SENTENCE:
                preview = sentence.strip()
                errors.append(
                    f"{path}:{line_number}: sentence has {cjk_count} CJK characters "
                    f"(limit {MAX_CJK_PER_SENTENCE}): {preview}"
                )

    return errors


def main() -> int:
    arguments = sys.argv[1:]
    if not arguments:
        print("usage: check_doc_style.py <markdown> [...]", file=sys.stderr)
        return 2

    paths: list[Path] = []
    for argument in arguments:
        matches = glob(argument)
        paths.extend(Path(match) for match in (matches or [argument]))

    errors: list[str] = []
    for path in paths:
        errors.extend(check_document(path))

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1

    print(f"Documentation prose style passed for {len(paths)} files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
