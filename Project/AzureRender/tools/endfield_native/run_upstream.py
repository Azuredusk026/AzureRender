"""Run a pinned upstream Python entry point with explicit catalog compatibility.

Example: python run_upstream.py --source-root EXTRACTOR/src
    --dependency-root RUN/python-deps --script ENTRY.py -- ENTRY_ARGUMENTS
This command modifies only its own Python process, never upstream source files.
"""
import argparse
from pathlib import Path
import runpy
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--dependency-root", type=Path, required=True)
    parser.add_argument("--script", type=Path, required=True)
    parser.add_argument("--catalog-mode", choices=("legacy", "upstream"),
                        default="legacy",
                        help="legacy installs the isolated adapter; upstream preserves the pinned reader")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    script = args.script.resolve(strict=True)
    dependencies = args.dependency_root.resolve(strict=True)
    sys.path[:0] = [str(source), str(dependencies), str(Path(__file__).resolve().parents[1])]
    if args.catalog_mode == "legacy":
        from extractor_core import vfs
        from endfield_native.compat import install_legacy_adapter
        install_legacy_adapter(vfs)
    forwarded = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    sys.argv = [str(script), *forwarded]
    runpy.run_path(str(script), run_name="__main__")


if __name__ == "__main__":
    main()
