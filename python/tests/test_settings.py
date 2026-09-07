"""The shared settings file, which all three ports read and write.

The rules tested here are a contract between the ports, not an implementation
detail of this one: one spelling for paths, case-insensitive de-duplication on
Windows, a cap of ten, and keys another port owns left alone.
"""

from __future__ import annotations

import json
import os
from pathlib import Path

import pytest

from marklens import settings


@pytest.fixture(autouse=True)
def _isolated_settings(tmp_path, monkeypatch):
    """Never touch the real file: these tests would otherwise eat the recent
    list of whichever port ran last."""
    monkeypatch.setenv("MARKLENS_SETTINGS", str(tmp_path / "settings.json"))
    return tmp_path / "settings.json"


def test_missing_file_reads_as_empty():
    assert settings.load() == {}
    assert settings.recent_files() == []
    assert settings.last_open_dir() == ""


def test_corrupt_file_is_treated_as_absent(_isolated_settings):
    _isolated_settings.write_bytes(b"{ not json")
    assert settings.load() == {}
    assert settings.recent_files() == []


def test_paths_are_stored_with_forward_slashes(_isolated_settings):
    settings.add_recent(Path("C:/tmp/a.md") if os.name == "nt" else Path("/tmp/a.md"))
    stored = json.loads(_isolated_settings.read_bytes().decode("utf-8"))
    assert "\\" not in stored[settings.RECENT_KEY][0]


def test_recent_is_newest_first_and_deduplicated():
    settings.add_recent("/tmp/a.md")
    settings.add_recent("/tmp/b.md")
    settings.add_recent("/tmp/a.md")
    assert settings.recent_files() == ["/tmp/a.md", "/tmp/b.md"]


@pytest.mark.skipif(os.name != "nt", reason="only Windows folds filename case")
def test_case_differing_paths_are_one_entry():
    settings.add_recent(r"C:\Users\x\A.md")
    settings.add_recent(r"C:\users\x\a.md")
    assert len(settings.recent_files()) == 1


def test_recent_is_capped():
    for i in range(settings.RECENT_MAX + 5):
        settings.add_recent(f"/tmp/{i}.md")
    assert len(settings.recent_files()) == settings.RECENT_MAX
    assert settings.recent_files()[0] == f"/tmp/{settings.RECENT_MAX + 4}.md"


def test_writes_preserve_keys_this_port_does_not_own(_isolated_settings):
    """toolBarStyle means nothing to the Rust port, and its recent list means
    nothing to a port that only wants the folder. A write must not drop either."""
    _isolated_settings.parent.mkdir(parents=True, exist_ok=True)
    _isolated_settings.write_bytes(
        json.dumps({"somethingElse": 42, settings.TOOLBAR_STYLE_KEY: 2}).encode("utf-8")
    )
    settings.add_recent("/tmp/a.md")
    settings.set_last_open_dir("/tmp")
    data = settings.load()
    assert data["somethingElse"] == 42
    assert data[settings.TOOLBAR_STYLE_KEY] == 2
    assert data[settings.RECENT_KEY] == ["/tmp/a.md"]
    assert data[settings.LAST_OPEN_DIR_KEY] == "/tmp"


def test_last_open_dir_round_trips():
    settings.set_last_open_dir(Path("/tmp/docs"))
    assert settings.last_open_dir() == "/tmp/docs"


def test_toolbar_style_falls_back_when_unreadable(_isolated_settings):
    _isolated_settings.write_bytes(
        json.dumps({settings.TOOLBAR_STYLE_KEY: "not a number"}).encode("utf-8")
    )
    assert settings.toolbar_style(7) == 7


def test_default_path_is_the_one_every_port_derives(monkeypatch):
    """The whole point is that three ports open the same file, and the way that
    fails is silently. Qt's GenericConfigLocation is AppData/Local on Windows
    while Rust's dirs::preference_dir() is AppData/Roaming, so the Qt ports
    derive Roaming by hand there; this pins that down."""
    monkeypatch.delenv("MARKLENS_SETTINGS", raising=False)
    path = settings.settings_path()
    assert path.name == "settings.json"
    assert path.parent.name == "Marklens"
    if os.name == "nt":
        assert str(path).lower().startswith(os.environ["LOCALAPPDATA"].lower())


def test_reads_a_file_written_by_the_rust_port(_isolated_settings):
    """A document in the exact shape the Rust port writes: two-space indent
    and no toolBarStyle, since that key means nothing there. The sharing is
    the whole point and would fail silently, so it is pinned here rather than
    left to a live run of two applications."""
    _isolated_settings.parent.mkdir(parents=True, exist_ok=True)
    _isolated_settings.write_bytes(
        b"""{
  "recentFiles": [
    "C:/docs/OTHER.md",
    "C:/docs/index.md"
  ],
  "lastOpenDir": "C:/docs"
}
"""
    )
    assert settings.recent_files() == ["C:/docs/OTHER.md", "C:/docs/index.md"]
    assert settings.last_open_dir() == "C:/docs"
    # A style this port owns can be added without disturbing what Rust wrote.
    settings.set_toolbar_style(2)
    assert settings.recent_files() == ["C:/docs/OTHER.md", "C:/docs/index.md"]
