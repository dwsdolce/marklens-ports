# marklens-ports

The Marklens Markdown viewer, reimplemented three ways for fun and comparison:
**Python/PySide6**, **C++/Qt**, and **Rust/Tauri**. A Rosetta Stone — one
behavior, three stacks.

The original is [Marklens](https://github.com/donald-jackson/marklens) by Donald
Jackson, a native macOS/iOS app (SwiftUI + WebKit). These target Windows and
Linux (and macOS) as ordinary desktop apps, so everything the original did to
satisfy the App Sandbox is gone — see `shared/spec/SPEC.md`.

## Layout

```
shared/
  web/              Frontend assets, identical across all ports (copied from
                    the Swift app): styles.css, highlight.min.js,
                    mermaid.min.js, hljs-{light,dark}.css
  spec/
    SPEC.md         The behavior contract
    fixtures/       Executable contract — each port loads these into its own
                    test harness:
                      render_cases.json   markdown -> HTML substring assertions
                      link_cases.json     (href, doc) -> resolved path
    sample/         A document exercising every path, for manual GUI testing
python/             Phase 1 — PySide6
cpp/                Phase 2 — Qt Widgets
rust/               Phase 3 — Tauri
packaging/          Bits the three ports' installers share, plus the how-to
tools/              Version stamping and icon generation
```

The point of `shared/spec/fixtures` is that all three implementations prove
themselves against the *same* cases. Different Markdown engines emit slightly
different HTML, so the render assertions are tolerant substrings (assert a
`<table>` exists, not its exact attributes).

## Installers

Each port builds its own, from its own directory, with the same three script
names:

```bash
python/packaging/build_win        # or build_win.bat, build_mac, build_linux
cpp/packaging/build_mac dmg
rust/packaging/build_linux
```

They install side by side on purpose — `Marklens Python`, `Marklens C++` and
`Marklens Rust` each have their own name, install location and identifiers, so
you can run all three at once and compare. See
[packaging/README.md](packaging/README.md).

## Status

- [x] Phase 0 — shared foundation (assets, spec, fixtures, sample)
- [x] Phase 1 — Python/PySide6 (ruff+mypy clean, 34 tests, smoke passes)
- [x] Phase 2 — C++/Qt (25/25 fixture rows, smoke passes; md4c engine)
- [x] Phase 3 — Rust/Tauri (core fixtures pass; comrak engine; app launches —
      visual GUI check is manual, Tauri has no headless webview)
- [x] Phase 4 — packaging and installers for all three
      (see [packaging/README.md](packaging/README.md))

## Known issues

Nothing here is a regression; the last three are behaviours the three ports
were never made to agree on.

- **The packaging has only ever been run on Windows.** The three ports
  themselves were developed and run on macOS — that is what the initial commit
  is — but everything added since (`packaging/setup`, `build_mac`,
  `build_linux`, `run_mac`, `run_linux`, and the `macdeployqt`, `create-dmg`,
  `linuxdeploy` and AppImage paths they drive) was written and exercised on
  Windows alone. The C++ port's md4c handling also differs off Windows: a
  system md4c from Homebrew or apt should be found first, taking a branch
  Windows never reaches. Linux has seen neither the apps nor the packaging.

- **Same-document `#fragment` links probably misbehave on Windows.** Both Qt
  ports compare `url.toLocalFile()` (forward slashes) against the stored
  document path (native separators) to decide whether a link is an in-page
  anchor — `cpp/src/page.cpp` and the equivalent in `python/src/marklens/app.py`.
  On Windows that comparison should never match, so an anchor click likely
  re-renders the document instead of scrolling to it. Read from the code, not
  reproduced. Same family as the separator bugs already fixed in the link
  fixtures and the recent-files list.

- **Recent files are shared between the Qt ports but not with Rust.** The C++
  and Python ports both use `QSettings` under `Marklens/Marklens`, so they
  share one list; the Rust port keeps its own JSON under Tauri's
  `app_config_dir`. Deliberate for now — unifying them means pinning one path
  in `shared/spec/SPEC.md`, because `dirs`, `platformdirs` and `QStandardPaths`
  disagree about where config belongs on Windows and macOS.

- **The window title differs.** The Qt ports show `index.md — Marklens`; the
  Rust port shows only `Marklens`.

- **The Rust toolbar's `font: menu` rule is unverified visually.** It has an
  explicit `system-ui`/12px fallback, so an unsupporting webview degrades
  rather than regressing to the 16px document font, but nobody has looked at
  the final build.

## License

MIT — see [LICENSE](LICENSE).

These are ground-up reimplementations rather than a translation: the original is
Swift and no code was carried across. What did come from it is the **application
icon**, which is MIT and remains © Donald Jackson; `LICENSE` reproduces that
notice, as MIT requires.

### Third-party components

Everything below is permissive and compatible with distributing this under MIT.
Qt is the only one carrying an obligation worth knowing about.

| Component | Used by | License |
|-----------|---------|---------|
| [Qt 6](https://www.qt.io) / PySide6 | Python, C++ | **LGPL-3.0** |
| [md4c](https://github.com/mity/md4c) | C++ | MIT |
| [markdown-it-py](https://github.com/executablebooks/markdown-it-py), mdit-py-plugins | Python | MIT |
| [platformdirs](https://github.com/tox-dev/platformdirs) | Python | MIT |
| [Tauri](https://tauri.app) | Rust | Apache-2.0 OR MIT |
| [comrak](https://github.com/kivikakk/comrak) | Rust | BSD-2-Clause |
| [serde](https://serde.rs), regex, url, percent-encoding, dirs | Rust | MIT OR Apache-2.0 |
| [notify](https://github.com/notify-rs/notify) | Rust | CC0-1.0 |
| [highlight.js](https://highlightjs.org) | all three | BSD-3-Clause |
| [Mermaid](https://mermaid.js.org) | all three | MIT |

**Qt is LGPL-3.0, not GPL**, which is why these ports need not adopt its
license: LGPL requires only that a user be able to relink the application
against a modified Qt. The packaged builds ship Qt as separate shared libraries
rather than statically linked, which satisfies that. Qt's source is available
from <https://www.qt.io/download>.
