# Marklens — Python / PySide6 port

A Markdown viewer: `QWebEngineView` renders the same HTML/CSS/JS the Swift app
uses; the Python side does Markdown→HTML, link routing, and file watching.

## Setup

```bash
cd python
python3 -m venv .venv
.venv/bin/pip install -e '.[dev]'
```

## Run

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
```

This is the tool for iterating on the code. It always runs the working tree,
through the venv, exactly as `python -m marklens` does. There is no
build step — that is the whole point of this port.
Read the script if you want the details; there is nothing in it you could not
type yourself, which is rather the point.

It never runs anything under `dist/` — that is what **Package** is for.

## Check

```bash
PYTHONPATH=tests .venv/bin/python -m pytest -q      # whole suite
.venv/bin/ruff check src tests
.venv/bin/mypy src
```

`pytest` runs everything: the shared-fixture logic tests (`test_renderer.py`,
`test_links.py`, loading `../shared/spec/fixtures/*.json`) plus the two GUI
checks (`test_gui.py`), which launch `smoke_gui.py` (render — image, mermaid,
table, highlight) and `nav_smoke.py` (clicks a link in the real MainWindow and
verifies it navigates without trapping) as offscreen subprocesses.

## Package

```bash
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Python_V<ver>.exe
packaging\build_win.bat    # cmd, same output
packaging/build_mac        # -> Marklens_Python_V<ver>.dmg  (or `build_mac pkg`)
packaging/build_linux      # -> dist/Marklens-Python-<ver>-<arch>.AppImage
```

PyInstaller does the freezing (`packaging/marklens.spec`), collecting PySide6
wholesale so QtWebEngine's Chromium, its `.pak` resources and its ICU data all
come along. The shared assets are bundled under `shared/`, which is where
`assets._shared_dir()` looks when frozen. Needs `pip install -e ".[packaging]"`;
see [../packaging/README.md](../packaging/README.md) for the per-platform tools
and for signing.

## Notes

- No sandbox → no folder-grant machinery. The page's base URL is the
  document's folder, so relative images and links resolve natively.
- `renderer.py` aligns strikethrough to `<del>` (GFM), matching the other ports.
- File watching re-adds the path after a change, since atomic saves
  (write-temp-then-rename) drop the watch — the cross-platform analog of the
  Swift kqueue re-arm.