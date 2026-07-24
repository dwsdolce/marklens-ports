# Marklens — C++ / Qt port

A Markdown viewer: `QWebEngineView` renders the same HTML/CSS/JS the other ports
use; C++ does Markdown→HTML (via **md4c**), link routing, and file watching.

## Requirements

- Qt 6 (`brew install qt`) and md4c (`brew install md4c`)
- CMake ≥ 3.19, a C++17 compiler

## Build

```bash
cd cpp
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
```

## Run

```bash
./build/marklens ../shared/spec/sample/index.md
```

## Test

```bash
ctest --test-dir build --output-on-failure   # all three, headless (offscreen)
```

Three tests: **core** (shared fixtures), **smoke** (render — image, mermaid,
table, highlight), **navigation** (clicks a link in the real MainWindow and
verifies it navigates without trapping). ctest sets the offscreen env for the
GUI ones. Run one directly with, e.g.,
`QT_QPA_PLATFORM=offscreen ./build/nav_smoke`.

## Notes

- `test_core` loads `../shared/spec/fixtures/*.json` — the same contract the
  Python port satisfies. 25/25 rows pass.
- **md4c matched the fixtures with no per-engine fixups** — strikethrough is
  `<del>` natively (the Python markdown-it-py port needed an override).
- No sandbox → no folder-grant machinery. Base URL is the document's folder, so
  relative images/links resolve natively.
- `MARKLENS_SHARED_DIR` is baked at configure time (dev build, not a bundle).
