"""Locate the shared web assets (styles.css, highlight.js, mermaid.js, themes).

Shared verbatim across all three ports; they live in ``shared/web`` at the
repo root, two levels above this package's project dir.
"""

from __future__ import annotations

import sys
from pathlib import Path


def _shared_dir() -> Path:
    # Frozen (PyInstaller): shared/ is bundled next to the app's data.
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS) / "shared"  # type: ignore[attr-defined]
    # Dev tree: src/marklens/assets.py -> python/ -> marklens-ports/
    return Path(__file__).resolve().parents[3] / "shared"


def web_dir() -> Path:
    """Absolute path to ``shared/web``."""
    return _shared_dir() / "web"


def asset_base_url() -> str:
    """``file://`` URL of the shared web dir, for the page shell's asset links."""
    return web_dir().as_uri()


def icon_path() -> Path:
    """The app icon, badged for this port.

    The three ports install side by side, so each carries the shared Marklens
    plate with its own language badge. tools/make_icons.py generates them.
    """
    return _shared_dir() / "icon-py.png"


def icons_dir() -> Path:
    """Toolbar icons (shared, SF-Symbols-style SVGs)."""
    return _shared_dir() / "icons"


def help_html() -> str:
    """The shared help document with the OS-specific 'set as default' steps
    substituted in for the current platform."""
    shared = _shared_dir()
    if sys.platform == "darwin":
        os_name = "macos"
    elif sys.platform.startswith("win"):
        os_name = "windows"
    else:
        os_name = "linux"
    steps = (shared / f"help_default_{os_name}.html").read_text()
    return (shared / "help.html").read_text().replace("<!--DEFAULT_APP_STEPS-->", steps)