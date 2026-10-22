#!/usr/bin/env python3
"""Validate material assets with the production material interpreter."""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('asset', type=Path)
    parser.add_argument('--validator', type=Path, default=Path(__file__).resolve().parents[1] / 'build/ninja-msvc-release/AzureMaterialValidator.exe')
    parser.add_argument('--texture-paths', type=Path, help='UUID to image path JSON for full document interpretation')
    args = parser.parse_args()
    if not args.validator.is_file():
        parser.error('Build AzureMaterialValidator or pass --validator')
    command = [str(args.validator.resolve()), str(args.asset.resolve())]
    if args.texture_paths:
        command.append(str(args.texture_paths.resolve()))
    raise SystemExit(subprocess.run(command).returncode)


if __name__ == '__main__':
    main()
