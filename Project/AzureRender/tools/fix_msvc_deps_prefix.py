#!/usr/bin/env python3
"""Repair Ninja header-dependency tracking on localized MSVC toolchains.

Ninja learns header dependencies by matching each line of cl's /showIncludes
output against a prefix string that CMake probes at compiler-detection time
and stores in CMakeFiles/<version>/CMakeCXXCompiler.cmake. On a non-English
MSVC the probe output can be transcoded through the wrong code page, leaving a
prefix that never matches the bytes cl actually emits. Every header edit then
silently fails to trigger rebuilds, and stale objects crash against new
struct layouts at runtime.

This tool compiles a one-line probe with the compiler recorded in the build
directory, extracts the exact prefix bytes from its output, and rewrites the
recorded prefix in both CMakeFiles/<version>/CMakeCXXCompiler.cmake (feeds
future reconfigures) and CMakeFiles/rules.ninja (the file Ninja actually
reads). No reconfigure is needed after running it.

Usage: python tools/fix_msvc_deps_prefix.py <build-dir> [<build-dir> ...]
Exit code is 0 when every given tree matches (or was repaired), 1 otherwise.
"""

import pathlib
import re
import subprocess
import sys
import tempfile


def probe_prefix(cxx_compiler: str, work_dir: pathlib.Path) -> bytes:
    """Compile a stub; return the exact bytes cl prepends to include lines."""
    # A quoted local include needs no INCLUDE environment, so the probe works
    # without VsDevCmd while still producing a /showIncludes line.
    (work_dir / "foo.h").write_bytes(b"\n")
    source = work_dir / "main.c"
    source.write_bytes(b'#include "foo.h"\n')
    result = subprocess.run(
        [cxx_compiler, "/nologo", "/showIncludes", "/c", str(source)],
        cwd=work_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "probe compile failed: " + repr(result.stdout[:200]))
    # cl prints the include line as <prefix><path-to-foo.h>; the path may be
    # absolute or bare, so strip whichever form is present.
    absolute = str((work_dir / "foo.h").resolve()).encode()
    for line in result.stdout.split(b"\r\n"):
        if line.endswith(absolute):
            return line[: len(line) - len(absolute)]
        if line.endswith(b"foo.h"):
            return line[: len(line) - len(b"foo.h")]
    raise RuntimeError("could not locate a /showIncludes line in probe output")


def repair(build_dir: pathlib.Path) -> bool:
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        print(f"skip {build_dir}: no CMakeCache.txt", file=sys.stderr)
        return False
    compiler_files = sorted(
        (build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake")
    )
    if not compiler_files:
        print(f"skip {build_dir}: no CMakeCXXCompiler.cmake", file=sys.stderr)
        return False

    compiler = None
    for line in cache.read_bytes().splitlines():
        if line.startswith(b"CMAKE_CXX_COMPILER:FILEPATH="):
            compiler = line.split(b"=", 1)[1].decode("utf-8").strip()
            break
    if compiler is None:
        print(f"skip {build_dir}: no compiler in cache", file=sys.stderr)
        return False

    with tempfile.TemporaryDirectory(dir=build_dir) as temp:
        prefix = probe_prefix(compiler, pathlib.Path(temp))

    repaired = False
    cmake_pattern = re.compile(
        rb'set\(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "(.*?)"\)'
    )
    for compiler_file in compiler_files:
        data = compiler_file.read_bytes()
        match = cmake_pattern.search(data)
        if match is None or match.group(1) == prefix:
            continue
        patched = cmake_pattern.sub(
            b'set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "'
            + prefix.replace(b"\\", b"\\\\")
            + b'")',
            data,
        )
        compiler_file.write_bytes(patched)
        repaired = True

    rules_ninja = build_dir / "CMakeFiles" / "rules.ninja"
    if rules_ninja.is_file():
        rules = rules_ninja.read_bytes()
        rules_pattern = re.compile(rb"msvc_deps_prefix = (.*)")
        if rules_pattern.search(rules):
            patched_rules = rules_pattern.sub(
                b"msvc_deps_prefix = " + prefix.replace(b"\\", rb"\\"), rules
            )
            if patched_rules != rules:
                rules_ninja.write_bytes(patched_rules)
                repaired = True
        else:
            # CMake 4.4 omits the line when the probe found no prefix; Ninja
            # then falls back to the English default and silently stops
            # tracking. Insert the probed prefix next to the first msvc rule.
            anchor = rules.find(b"  deps = msvc")
            if anchor >= 0:
                rules_ninja.write_bytes(
                    rules[:anchor]
                    + b"  msvc_deps_prefix = "
                    + prefix
                    + b"\n"
                    + rules[anchor:]
                )
                repaired = True
    print(
        f"{build_dir}: prefix "
        + ("repaired" if repaired else "already matches compiler output")
    )
    return True


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    ok = True
    for argument in sys.argv[1:]:
        ok = repair(pathlib.Path(argument)) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
