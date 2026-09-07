r"""The settings file all three ports share.

Until now the Qt ports kept their state in ``QSettings`` - the Windows
registry, a macOS plist, an INI file on Linux - and the Rust port kept its own
``recent.json`` under Tauri's config directory. Two stores meant two Open Recent
lists and, once a starting folder was remembered, two of those as well. None of
``QSettings``' backends can be reached from Rust, so the common ground is a
plain JSON file that all three read and write.

Location, identical in every port::

    Windows   %LOCALAPPDATA%\Marklens\settings.json
    macOS     ~/Library/Preferences/Marklens/settings.json
    Linux     ${XDG_CONFIG_HOME:-~/.config}/Marklens/settings.json

which is what Qt calls ``GenericConfigLocation``. The Rust port derives the same
three by hand, because its own config directory is keyed by bundle identifier
and would not agree.

Writes are read-modify-write: the file holds keys this port does not own -
``toolBarStyle`` means nothing to the Rust port - and replacing the whole
document would quietly discard them. Three applications can hold the file at
once and the last writer wins, which is why a change made in one is seen by
another only when that other next reads.

``MARKLENS_SETTINGS`` overrides the path, which is what the tests use.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

from PySide6.QtCore import QStandardPaths

RECENT_KEY = "recentFiles"
LAST_OPEN_DIR_KEY = "lastOpenDir"
TOOLBAR_STYLE_KEY = "toolBarStyle"
RECENT_MAX = 10


def settings_path() -> Path:
    override = os.environ.get("MARKLENS_SETTINGS")
    if override:
        return Path(override)
    # Qt's GenericConfigLocation and Rust's dirs::preference_dir() return the
    # same directory on all three platforms - measured, not assumed, because an
    # honest-looking guess that they differed on Windows sent this to
    # AppData/Roaming for a while and silently unshared the file.
    root = QStandardPaths.writableLocation(
        QStandardPaths.StandardLocation.GenericConfigLocation
    )
    return Path(root) / "Marklens" / "settings.json"


def load() -> dict[str, Any]:
    """The whole document, or an empty one if it is missing or unreadable.

    A corrupt file is treated as absent rather than fatal: settings are a
    convenience, and refusing to start because a recent-files list will not
    parse would be a poor trade.
    """
    try:
        raw = settings_path().read_bytes().decode("utf-8")
    except OSError:
        return {}
    try:
        data = json.loads(raw)
    except ValueError:
        return {}
    return data if isinstance(data, dict) else {}


def save(data: dict[str, Any]) -> None:
    path = settings_path()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        # write_bytes, not write_text: the latter translates newlines on
        # Windows, and this file is read by three ports on three platforms.
        path.write_bytes((json.dumps(data, indent=2) + "\n").encode("utf-8"))
    except OSError:
        pass


def update(**values: Any) -> None:
    """Merge keys into the file, leaving the ones this port does not own."""
    data = load()
    data.update(values)
    save(data)


def canonical(path: str | Path) -> str:
    """The one spelling every port agrees to store.

    Forward slashes, because that is what Qt hands back (QFileDialog,
    QUrl::toLocalFile) and what the C++ port works with internally. Without a
    single spelling the same document lands in the list twice on Windows.
    """
    return Path(path).as_posix()


def dedup_key(path: str) -> str:
    """Comparison key: ``normcase`` folds case on Windows, where filenames are
    case-insensitive, and is the identity elsewhere."""
    return os.path.normcase(path)


def recent_files() -> list[str]:
    """The stored list, canonicalised and de-duplicated on read, so a list
    written by an older build or by another port is cleaned up on sight."""
    raw = load().get(RECENT_KEY)
    if isinstance(raw, str):
        raw = [raw]
    if not isinstance(raw, list):
        return []
    seen: set[str] = set()
    out: list[str] = []
    for entry in raw:
        if not isinstance(entry, str):
            continue
        spelled = canonical(entry)
        key = dedup_key(spelled)
        if key not in seen:
            seen.add(key)
            out.append(spelled)
    return out


def add_recent(path: str | Path) -> list[str]:
    spelled = canonical(path)
    key = dedup_key(spelled)
    recent = [p for p in recent_files() if dedup_key(p) != key]
    recent.insert(0, spelled)
    recent = recent[:RECENT_MAX]
    update(**{RECENT_KEY: recent})
    return recent


def clear_recent() -> None:
    update(**{RECENT_KEY: []})


def last_open_dir() -> str:
    """The folder the Open dialog should start in, or "" for the platform's
    own default."""
    value = load().get(LAST_OPEN_DIR_KEY)
    return canonical(value) if isinstance(value, str) and value else ""


def set_last_open_dir(path: str | Path) -> None:
    update(**{LAST_OPEN_DIR_KEY: canonical(path)})


def toolbar_style(default: int) -> int:
    value = load().get(TOOLBAR_STYLE_KEY, default)
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def set_toolbar_style(style: int) -> None:
    update(**{TOOLBAR_STYLE_KEY: int(style)})
