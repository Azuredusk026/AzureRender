"""Run a pinned upstream Python entry point with explicit catalog compatibility.

Example: python run_upstream.py --source-root EXTRACTOR/src
    --dependency-root RUN/python-deps --script ENTRY.py -- ENTRY_ARGUMENTS
This command modifies only its own Python process, never upstream source files.
"""
import argparse
import json
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
    parser.add_argument("--vfs-layer-root",type=Path,action="append",default=[],
                        help="Explicit VFS roots ordered from lowest to highest priority")
    parser.add_argument("--vfs-layer-receipt",type=Path,
                        help="New receipt file outside the source layers; required with layered reads")
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
    view=None;receipt=None
    if args.vfs_layer_root:
        if args.vfs_layer_receipt is None:
            parser.error("--vfs-layer-receipt is required with --vfs-layer-root")
        from extractor_core import resources,vfs
        from endfield_native.vfs_layers import LayeredVfs
        view=LayeredVfs(args.vfs_layer_root,vfs)
        receipt=view.output_path(args.vfs_layer_receipt)
        if receipt.exists():raise FileExistsError(receipt)
        receipt.parent.mkdir(parents=True,exist_ok=True)
        view.install(resources)
    elif args.vfs_layer_receipt is not None:
        parser.error("--vfs-layer-receipt requires --vfs-layer-root")
    forwarded = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    sys.argv = [str(script), *forwarded]
    try:
        runpy.run_path(str(script), run_name="__main__")
    finally:
        if view is not None:
            with receipt.open('x',encoding='utf-8') as stream:
                json.dump(view.receipt(),stream,indent=2,allow_nan=False)


if __name__ == "__main__":
    main()
