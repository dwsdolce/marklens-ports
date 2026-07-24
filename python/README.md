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
.venv/bin/python -m marklens ../shared/spec/sample/index.md
# or via the installed console script:
.venv/bin/marklens ../shared/spec/sample/index.md
```

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

## Notes

- No sandbox → no folder-grant machinery. The page's base URL is the
  document's folder, so relative images and links resolve natively.
- `renderer.py` aligns strikethrough to `<del>` (GFM), matching the other ports.
- File watching re-adds the path after a change, since atomic saves
  (write-temp-then-rename) drop the watch — the cross-platform analog of the
  Swift kqueue re-arm.