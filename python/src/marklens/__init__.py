"""Marklens - a native Markdown viewer (PySide6 port)."""

from __future__ import annotations

#: The base version. pyproject.toml must agree.
__version__ = "0.1.0"

#: Shown in Help > About. pyproject.toml's Homepage must agree.
PROJECT_URL = "https://github.com/dwsdolce/marklens-ports"

#: The macOS/iOS app these are ports of, credited in Help > About. Its icon
#: is reused here under MIT; see LICENSE.
UPSTREAM_URL = "https://github.com/donald-jackson/marklens"


def _read_build() -> str:
    """The build number: the git commit count for HEAD.

    Two sources, in order:

    1. A frozen build, where ``tools/gen_version_build.py`` wrote
       ``version_build`` and the PyInstaller spec bundled it beside the package.
    2. A source checkout, where git can simply be asked.

    Neither is fatal - an empty build number just means the version renders as
    ``0.1.0`` rather than ``0.1.0 (4)``.
    """
    import os
    import sys

    here = os.path.dirname(os.path.abspath(__file__))
    bundled = getattr(sys, "_MEIPASS", None)
    if bundled:
        try:
            with open(os.path.join(bundled, "marklens", "version_build"),
                      encoding="utf-8") as handle:
                return handle.read().strip()
        except OSError:
            return ""

    import subprocess

    try:
        result = subprocess.run(
            ["git", "rev-list", "--count", "HEAD"],
            # Anchored to the package, not the process cwd: the app may well
            # have been launched from the directory of the document opened.
            cwd=here,
            capture_output=True, text=True, timeout=2, check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return ""
    return result.stdout.strip() if result.returncode == 0 else ""


#: Git commit count, or "" when neither a bundle nor a checkout is available.
__build__ = _read_build()

#: What the About box shows, e.g. "0.1.0 (4)".
__version_string__ = f"{__version__} ({__build__})" if __build__ else __version__
