#!/usr/bin/env python3
"""Stamp the build number onto a port, ready for packaging.

The build number is ``git rev-list --count HEAD`` -- monotonic, needs nothing
typed by hand, and identifies the exact commit an installer was cut from.
Combined with the port's own base version it gives the four-part version
Windows wants: 0.1.0.37.

Each port keeps its base version in its own native place, which stays the
single source of truth:

    python   python/pyproject.toml      project.version
    cpp      cpp/CMakeLists.txt         project(... VERSION ...)
    rust     rust/src-tauri/tauri.conf.json   version

Usage:

    python tools/gen_version_build.py python           # write the files
    python tools/gen_version_build.py cpp --print      # print the full version
    python tools/gen_version_build.py all              # every port

Writes ``<port>/build/installer_version`` holding the full version. The
packaging scripts and the Inno Setup scripts both read that file rather than
taking a command-line define, because Git Bash rewrites any argument that looks
like a Unix path (so ``/DMyAppVersion=`` arrives mangled) while the ``//``
escape that fixes it is passed through literally by Cygwin. Neither shell
touches a file.

One Python script rather than a .sh and a .bat, because the builds run from
both Git Bash and cmd and there is no reason for the two to drift.
"""

import argparse
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

PORTS = ("python", "cpp", "rust")


def commit_count() -> str:
    result = subprocess.run(
        ["git", "rev-list", "--count", "HEAD"],
        cwd=ROOT, capture_output=True, text=True,
        # Explicitly not check=True: a missing .git deserves the message below,
        # not a CalledProcessError traceback.
        check=False,
    )
    if result.returncode != 0:
        sys.exit(f"gen_version_build: git rev-list failed - is this a git checkout?\n"
                 f"{result.stderr.strip()}")
    count = result.stdout.strip()
    if not count.isdigit():
        sys.exit(f"gen_version_build: unexpected git output {count!r}")
    return count


def _search(path: pathlib.Path, pattern: str) -> str:
    if not path.is_file():
        sys.exit(f"gen_version_build: {path} is missing")
    match = re.search(pattern, path.read_text(encoding="utf-8"), re.MULTILINE)
    if not match:
        sys.exit(f"gen_version_build: no version found in {path}")
    return match.group(1)


def base_version(port: str) -> str:
    """The port's own declared version, normalised to three components.

    Windows wants four numbers and the build number supplies the fourth, so a
    two-part version like 0.1 is padded rather than rejected.
    """
    if port == "python":
        version = _search(ROOT / "python" / "pyproject.toml", r'^version = "([^"]+)"')
    elif port == "cpp":
        version = _search(ROOT / "cpp" / "CMakeLists.txt",
                          r"^project\([^)]*?VERSION\s+([0-9][0-9.]*)")
    elif port == "rust":
        config = ROOT / "rust" / "src-tauri" / "tauri.conf.json"
        if not config.is_file():
            sys.exit(f"gen_version_build: {config} is missing")
        version = json.loads(config.read_text(encoding="utf-8")).get("version", "")
        if not version:
            sys.exit(f"gen_version_build: no version in {config}")
    else:
        sys.exit(f"gen_version_build: unknown port {port!r}")

    parts = version.split(".")
    if not all(part.isdigit() for part in parts) or not 1 <= len(parts) <= 3:
        sys.exit(f"gen_version_build: {port} version {version!r} is not 1-3 numbers")
    return ".".join(parts + ["0"] * (3 - len(parts)))


def stamp(port: str) -> str:
    """Write ``<port>/build/installer_version`` and return the full version."""
    full = f"{base_version(port)}.{commit_count()}"
    target = ROOT / port / "build" / "installer_version"
    target.parent.mkdir(parents=True, exist_ok=True)
    # No trailing newline: the .iss scripts read this raw.
    target.write_text(full, encoding="utf-8")
    return full


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("port", choices=[*PORTS, "all"])
    parser.add_argument("--print", dest="show", action="store_true",
                        help="print the full version to stdout, nothing else")
    args = parser.parse_args()

    ports = PORTS if args.port == "all" else (args.port,)
    for port in ports:
        full = stamp(port)
        if args.show:
            print(full)
        else:
            print(f"gen_version_build: {port} version {full}")


if __name__ == "__main__":
    main()
