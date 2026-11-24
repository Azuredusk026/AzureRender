"""Execute the production C++ scene-import workflow and preserve its diagnostics."""
import argparse
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--fixture', type=Path, default=Path(__file__).resolve().parents[1]/'assets_public/resource_validation/mechanical_panel.gltf')
    parser.add_argument('--exchange', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    command = [str(args.executable.resolve()), str(args.fixture.resolve())]
    if args.exchange: command.append(str(args.exchange.resolve()))
    result = subprocess.run(command, text=True, capture_output=True, timeout=180)
    text = result.stdout + result.stderr
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding='utf-8')
    print(text, end='')
    if result.returncode or 'Scene exchange parser, transactions' not in text:
        raise SystemExit(result.returncode or 1)

if __name__ == '__main__': main()
