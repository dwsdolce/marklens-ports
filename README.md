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
  icons/            Toolbar glyphs, identical across all ports — open
                    look-alikes for the original's SF Symbols, which are
                    Apple-platform-only; see icons/ICONS.md
  icon*.{svg,png,ico,icns}
                    The application icon, and the per-port badged variants
                    tools/make_icons.py generates from it
  licenses/         Licence texts the Linux packages have to carry
  spec/
    SPEC.md         The behavior contract
    fixtures/       Executable contract — each port loads these into its own
                    test harness:
                      render_cases.json   markdown -> HTML substring assertions
                      link_cases.json     (href, doc) -> resolved path
    sample/         A document exercising every path, for manual GUI testing
python/             The PySide6 port
cpp/                The Qt Widgets port
rust/               The Tauri port
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

## The three ports

All three are complete and behave the same. Where they differ is underneath,
which is the point of the exercise:

| | [python/](python/) | [cpp/](cpp/) | [rust/](rust/) |
|---|---|---|---|
| Toolkit | PySide6 (Qt 6) | Qt 6 Widgets | Tauri 2 |
| Needs | Python ≥ 3.12 | CMake ≥ 3.19, Qt 6 + WebEngine | Rust ≥ 1.85 |
| Markdown engine | markdown-it-py | md4c | comrak |
| Webview | QtWebEngine (bundled Chromium) | QtWebEngine (bundled Chromium) | the OS webview — WKWebView, WebView2, WebKitGTK |
| Installer built by | PyInstaller | CMake + deploy tools | Cargo (Inno on Windows, Tauri's bundler elsewhere) |
| Tests | fixtures, GUI smoke, navigation | fixtures, GUI smoke, navigation | fixtures only ([why](#known-issues)) |

The webview row is the interesting one: the Qt ports carry their own Chromium
and so render identically everywhere, while the Rust port uses whatever the
platform provides and is therefore the smallest download and the least
predictable — it renders a different set of image formats on each OS.

Each port has its own README covering setup, running, testing and packaging;
they share the same four verbs and the same script names.

## Known issues

Nothing here is a regression. All three ports build, install and run on
Windows, Linux and macOS, and have been eyeballed side by side on each; the
macOS builds are signed, notarised and stapled. What remains is two deliberate
divergences and one gap in the tests.

- **The Rust port has no GUI test.** The Qt ports each drive a real window and
  assert that a document rendered — that the image loaded, mermaid drew, code
  highlighted, a link navigated. Rust has only the shared fixtures. The
  obstacle is structural: its window code lives in the binary, where nothing
  can link it, rather than in a library as the C++ port's does. That layer is
  what catches "the window opened but nothing rendered", which is how the worst
  bugs here have presented. See the Testing section of
  [shared/spec/SPEC.md](shared/spec/SPEC.md).

- **Recent files are shared between the Qt ports but not with Rust.** The C++
  and Python ports both use `QSettings` under `Marklens/Marklens`, so they
  share one list; the Rust port keeps its own JSON under Tauri's
  `app_config_dir`. Deliberate for now — unifying them means pinning one path
  in `shared/spec/SPEC.md`, because `dirs`, `platformdirs` and `QStandardPaths`
  disagree about where config belongs on Windows and macOS.

- **The ports render fewer image formats than the macOS original.** They render
  PNG, JPEG, GIF, WebP, BMP, ICO and SVG — the set `shared/spec/SPEC.md`
  requires — and a `.tif` shows a broken-image icon. Images are decoded by the
  webview rather than by the port, and the original inherits macOS ImageIO
  through WKWebView, so it displays TIFF, HEIC and anything else the system
  knows. Qt WebEngine and WebView2, both Chromium, and WebKitGTK each carry a
  smaller web-oriented set. Deliberate: closing it would mean enumerating every
  format ImageIO decodes and writing a converter in three languages, for
  formats a Markdown document almost never carries. The Rust port is not even
  consistent with itself here — its macOS build is WKWebView, so it shows what
  the original shows.

## License

MIT — see [LICENSE](LICENSE), with attributions in
[NOTICES.md](NOTICES.md).

These are ground-up reimplementations rather than a translation: the original is
Swift and no code was carried across. What did come from it is the **application
icon**, which is MIT and remains © Donald Jackson;
[NOTICES.md](NOTICES.md) reproduces that notice, as MIT requires.

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
| [Lucide](https://lucide.dev) (toolbar icons) | all three | ISC |

**Qt is LGPL-3.0, not GPL**, which is why these ports need not adopt its
license: LGPL requires only that a user be able to relink the application
against a modified Qt. The packaged builds ship Qt as separate shared libraries
rather than statically linked, which satisfies that. Qt's source is available
from <https://www.qt.io/download>.
