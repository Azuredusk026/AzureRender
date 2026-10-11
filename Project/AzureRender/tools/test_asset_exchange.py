"""Exercise the independent exchange workflow and its publication boundaries."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable = Path(sys.argv[1]).resolve()
    fixture = Path(sys.argv[2]).resolve()
    assert executable.is_file(), "Independent asset exchange executable is required"
    with tempfile.TemporaryDirectory(prefix="azure-exchange-") as directory:
        root = Path(directory)
        source = root / "🎨 中文.PNG"
        source.write_bytes(fixture.read_bytes())
        request = root / "请求.json"
        body = {"source": source.name, "format": "png", "destination": "输出", "settings": {}}

        def run(mode, expected=0, *options):
            request.write_text(json.dumps(body, ensure_ascii=False), encoding="utf-8")
            result = subprocess.run([str(executable), mode, str(request), *options],
                                    capture_output=True, encoding="utf-8", timeout=30)
            assert result.returncode == expected, (result.stdout, result.stderr)
            return json.loads(result.stdout)

        planned = run("plan")
        assert planned["entry"] == "🎨 中文.png"
        assert not (root / "输出").exists()
        exported = run("export")
        output = Path(exported["output"])
        assert output.read_bytes() == source.read_bytes()
        original = output.read_bytes()
        run("export", 1)
        assert output.read_bytes() == original
        run("export", 0, "--replace")
        body["destination"] = "取消输出"
        run("export", 1, "--deadline-ms", "0")
        assert not (root / "取消输出").exists()
        body["settings"] = {"unsupported": True}
        run("plan", 1)
        body["settings"] = {}
        body["settingsVersion"] = 999
        run("plan", 1)
        body["settingsVersion"] = 1
        body["format"] = "wav"
        run("export", 1)
        assert not list(root.glob(".azure-export-*")), "Owned staging must be released"
    print("Independent exchange: Unicode, plan, publication, replacement and deadline passed")


if __name__ == "__main__":
    main()
