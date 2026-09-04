# Marklens — Python / PySide6 port

A Markdown viewer: `QWebEngineView` renders the same HTML/CSS/JS the Swift app
uses; the Python side does Markdown→HTML, link routing, and file watching.

## Setup

```bash
packaging/setup            # check the environment, install what it can
packaging/setup --check    # report only, change nothing
packaging\setup.bat        # cmd, same checks
```

`pyproject.toml` is this port's manifest and covers every library dependency,
which is why there is far less here than for the C++ port. What a manifest
cannot cover is the environment around it, and that is what `setup` checks: the
virtual environment, the interpreter inside it, and — only when you come to
package — `appimagetool`, `create-dmg` or Inno Setup.

**One virtual environment, at the repository root**, `marklens-ports/.venv` —
not `python/.venv`. One environment for the project rather than one per port;
`setup` reports a `python/.venv` as an error, because once two exist you install
into one and run the other, and the difference surfaces as an import failure
somewhere unrelated.

Creating and activating it are yours to do — VS Code does both if you open
`marklens-ports` as the workspace folder (*Command Palette > Python: Create
Environment > Venv*), and activates it in every new terminal. By hand, from the
repository root:

```bash
python -m venv .venv
source .venv/bin/activate      # .venv\Scripts\activate in cmd
```

Then `packaging/setup` installs `-e ".[dev]"` into whatever is active. `setup`
is the only script that looks at any of this: `run_*` and `build_*` use the
interpreter they are given, so the venv you activate is the venv everything
uses.

## Run

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
```

This is the tool for iterating on the code. It always runs the working tree,
exactly as `python -m marklens` does, through whichever venv you have active.
There is no build step — that is the whole point of this port.
Read the script if you want the details; there is nothing in it you could not
type yourself, which is rather the point.

It never runs anything under `dist/` — that is what **Package** is for.

## Check

```bash
packaging/test_win         # Git Bash or Cygwin - the test suite
packaging\test_win.bat     # cmd
packaging/test_mac
packaging/test_linux

ruff check src tests       # lint and types have no per-platform wrapper
mypy src
```

The `test_*` scripts are the same shape as `run_*` and `build_*`: one per
platform, each running the suite against the working tree. Arguments go
straight to pytest, so `packaging/test_linux -k renderer` selects a subset. By
hand it is `PYTHONPATH=tests python -m pytest -q`, with the venv active — this
port needs no build step, so the wrapper only sets `PYTHONPATH` and checks the
venv, and exists mostly so the verb matches the other two ports.

`pytest` runs everything: the shared-fixture logic tests (`test_renderer.py`,
`test_links.py`, loading `../shared/spec/fixtures/*.json`) plus three GUI checks
(`test_gui.py`), launched as offscreen subprocesses — `smoke_gui.py` (render —
image, mermaid, table, highlight), `nav_smoke.py` (clicks a link in the real
MainWindow and verifies it navigates without trapping), and `back_smoke.py`
(follows an in-page anchor, presses Back, and checks it returns to where the
link was read rather than reopening the previous document).

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
`assets._shared_dir()` looks when frozen. Needs `packaging/setup` to have run;
see [../packaging/README.md](../packaging/README.md) for the per-platform tools
and for signing.

## Notes

- No sandbox → no folder-grant machinery. The page's base URL is the
  document's folder, so relative images and links resolve natively.
- `renderer.py` aligns strikethrough to `<del>` (GFM), matching the other ports.
- File watching re-adds the path after a change, since atomic saves
  (write-temp-then-rename) drop the watch — the cross-platform analog of the
  Swift kqueue re-arm.