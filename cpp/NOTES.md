# C++/Qt port — working notes (resume from here)

Durable state for Phase 2 so it can be resumed cold. Update as you go.

## Decisions

- **Markdown engine: md4c** (0.5.3, `brew`, already installed). Headers
  `md4c.h` + `md4c-html.h`, libs `libmd4c` + `libmd4c-html` under
  `/opt/homebrew/opt/md4c`. Use `md_html()` with `MD_DIALECT_GITHUB`
  (tables + strikethrough + tasklists + permissive autolinks). md4c renders
  strikethrough as `<del>` and fenced code as `<pre><code class="language-X">`
  — already matches the shared fixtures, so no per-engine fixups (unlike the
  Python `<s>`→`<del>` tweak). Raw HTML passes through (no `MD_FLAG_NOHTML`),
  so `<img>`/`<p align>` in source survive.
- **Core uses Qt types** (QString/QUrl) — it's a Qt app throughout; QUrl gives
  scheme parsing + percent-decoding for free. Path normalization via
  `std::filesystem::path::lexically_normal` (collapses `..` without touching
  disk, matching Swift `standardizedFileURL` / Python `os.path.normpath`).
- **Tests: QtTest + QJsonDocument** — no gtest/nlohmann; load the shared
  `../shared/spec/fixtures/*.json` and assert tolerant substrings, same
  contract as the Python port.
- **Build: CMake** targeting Qt6 (Widgets, WebEngineWidgets, WebEngineCore,
  Test). Toolchain confirmed: Apple clang 21, cmake 3.26, Qt 6.11.1 at
  `/opt/homebrew/opt/qt`.

## Off-sandbox simplifications (same as Python)

No security-scoped bookmarks, no folder-grant UI, no custom URL scheme. The
webview base URL is the document's folder → relative images/links resolve
natively. Link interception only routes: external→system browser,
relative .md→viewer, `#frag`→native scroll.

## Layout

```
cpp/
  CMakeLists.txt
  src/  renderer.{h,cpp} links.{h,cpp} assets.{h,cpp}
        page.{h,cpp} mainwindow.{h,cpp} main.cpp
  tests/ test_core.cpp   (QtTest, loads shared fixtures)
  build/ (gitignored)
```

## Qt API map (verify each before relying on it)

- WKWebView → `QWebEngineView` / `QWebEnginePage`
- link interception → override `QWebEnginePage::acceptNavigationRequest`
  (fires for link clicks off-sandbox, unlike WKWebView)
- setHtml(html, baseUrl) → `QWebEnginePage::setHtml`
- find → `findText`; zoom → `setZoomFactor`; PDF → `printToPdf`
- file watch → `QFileSystemWatcher` (re-add path after atomic-save rename)
- local asset access → `QWebEngineSettings::LocalContentCanAccessFileUrls`

## Status

- [x] Engine chosen + toolchain confirmed
- [x] CMake skeleton builds (core + test targets)
- [x] Core (renderer+links) passes shared fixtures via QtTest — **25/25 rows**.
      md4c matched fixtures with NO per-engine fixups (strikethrough is `<del>`
      natively, unlike Python's markdown-it-py override).
- [x] GUI files written (page/mainwindow/main) — mirrors verified Python app.py
- [x] App target builds clean (no warnings)
- [x] Offscreen smoke test PASSES — image resolved, mermaid drawn (SVG), table,
      highlight. Identical result to the Python port.

**PHASE 2 DONE.** Next session: Phase 3 (Rust/Tauri), needs rustup + Tauri CLI
(neither present).

## Menu bar + Help (added in review)

Both ports were toolbar-only, so macOS showed just a bare app menu. Added a
full `QMenuBar` (`buildUi()` in mainwindow.cpp, `_build_ui()` in app.py),
sharing the same QActions as the toolbar:
- File: Open, Reload, **Auto-Reload on Change** (checkable; gates the watcher),
  Export as PDF (⌘⇧E), Show in <Finder/Explorer/File Manager>, Close (⌘W)
- Edit: Find, Find Next (⌘G), Find Previous (⇧⌘G)
- View: Back, Zoom In/Out/Actual Size
- Help: Marklens Help, About Marklens (AboutRole → app menu on macOS)

**Help** is now a proper scrollable dialog (QTextBrowser) loading
`shared/help.html`, with the "set as default app" steps substituted per-OS from
`shared/help_default_{macos,windows,linux}.html` (`assets::helpHtml()` /
`assets.help_html()`). Quick Look section from the Swift help is deliberately
omitted — the ports have no QL extension.

Also added: **Open Recent** submenu (persisted via QSettings, most-recent-first,
deduped, capped at 10, with Clear Menu) and a **Window** menu (Minimize ⌘M,
Zoom). QSettings needs org+app name — set in main.cpp / __main__.py.
PySide gotcha: `addMenu(str)` can let the QMenu's C++ object be collected out
from under the wrapper → construct `QMenu("Open Recent", self)` explicitly in
Python (C++ `addMenu(str)` is fine).

Deliberately NOT ported from the Swift File menu: Allow Access to Folder /
Allowed Folders (sandbox-only), Save / Duplicate / Rename / Move To / Revert To
(DocumentGroup-injected editor commands; Marklens is a viewer).

## App icons + packaging (added in review)

Icon: the Swift app's `design/icon.svg`, copied to `shared/icon.svg`, rasterized
to `shared/icon.png` (rsvg-convert) and `shared/icon.icns` (iconutil). Shared
across ports for now; user wants per-port badge variants later (Mc/Mp/Mr).

Two levels, both done:
- **Runtime window/Dock icon** — `setWindowIcon(QIcon(.../icon.png))` in
  main.cpp and __main__.py. Works immediately, no packaging.
- **Bundled app icon:**
  - **C++**: CMake builds a macOS `.app` (`MACOSX_BUNDLE` + icns in Resources).
    Output: `build/Marklens C++.app` on macOS, `build/marklens-cpp` elsewhere.
    This also FIXES the app-menu-name issue. Run:
    `open "build/Marklens C++.app" --args <file>`.
  - **Python**: PyInstaller. `python/packaging/marklens.spec` builds
    `dist/Marklens Python.app`. Entry point is
    `python/packaging/pyinstaller_entry.py` (NOT
    `__main__.py` — running that directly breaks its relative imports).
    `assets._shared_dir()` is frozen-aware (`sys._MEIPASS`).
  - Both are now relocatable: `assets::sharedDir()` prefers a `shared/` next to
    the executable (`Contents/Resources/shared` in a macOS bundle) and only
    falls back to the baked-in `MARKLENS_SHARED_DIR` for development builds.
    See `packaging/README.md` for the full story.
Both suites still green after all this: C++ 3/3, Python 36, ruff+mypy clean.

## Post-ship fix: link-navigation trace trap (SIGTRAP)

Clicking a relative link crashed with a trace trap. Cause: `openPath`→`setHtml`
was called synchronously from inside `MarkdownPage::acceptNavigationRequest`,
re-entering QtWebEngine's navigation machinery. Fix: connect
`openDocument`→`openPath` with **`Qt::QueuedConnection`** so the load happens
after the nav callback returns. `tests/repro_nav.cpp` reproduces it (direct →
signal 133; `QUEUED=1` → NAV OK). The Python port had the identical bug (same
Qt), fixed the same way. Neither smoke test clicked a link, which is why it
shipped — see the new "Gotchas" section in `shared/spec/SPEC.md`.

Now guarded by a real regression test in both ports: `tests/nav_smoke.cpp`
(ctest "navigation") and `tests/nav_smoke.py` (pytest via `test_gui.py`). Both
drive the actual app, click a link, and verify navigation; both were confirmed
to FAIL (SIGTRAP 133) when the connection is reverted to direct. Build gotcha:
restoring a source via `mv .bak` back-dates its mtime, so `make` skips the
recompile — `touch` the file after such a restore.

## Build / test commands

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt && cmake --build build
ctest --test-dir build --output-on-failure          # core fixture tests
./build/marklens-cpp ../shared/spec/sample/index.md  # run the app
```
