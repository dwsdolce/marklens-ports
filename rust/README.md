# Marklens — Rust / Tauri port

A Markdown viewer. The **Rust backend** (in `src-tauri/`) renders Markdown→HTML
with **comrak**, resolves links, and watches the file; the **frontend**
(`frontend/`) is HTML/CSS/JS reusing the shared web assets, running in the
platform's native webview (WKWebView on macOS — no bundled Chromium).

## Requirements

- Rust (rustup) and the Tauri CLI: `cargo install tauri-cli --version "^2.0"`
- System webview deps (macOS: none extra; Linux: webkit2gtk)

## Run

```bash
cd src-tauri
cargo run -- ../../shared/spec/sample/index.md   # simplest: static frontend is embedded
# or the full dev experience:
cargo tauri dev
```

## Test

```bash
cd src-tauri
cargo test            # core: comrak renderer + link resolver vs shared fixtures
```

`tests/fixtures.rs` loads `../../shared/spec/fixtures/*.json` — the same
contract the Python and C++ ports satisfy. comrak matched it with no per-engine
fixups (strikethrough is `<del>` natively).

## Package

```bash
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Rust_V<ver>.exe
packaginguild_win.bat    # cmd, same output
packaging/build_mac        # -> Marklens_Rust_V<ver>.dmg  (or `build_mac app`)
packaging/build_linux      # -> dist/Marklens-Rust-<ver>-{AppImage,deb}
```

Tauri's own bundler does the packaging here rather than Inno Setup and
appimagetool, as the other two ports use: it is the only one that installs the
WebView2 runtime Windows needs, and the frontend is already embedded in the
binary so there is nothing else to collect. The scripts stamp the version, and
rename the output to the naming convention the other ports use. See
[../packaging/README.md](../packaging/README.md) for the details and for
signing.

## Notes

- **No headless GUI test.** Tauri uses the OS webview, which has no offscreen
  mode, so the render/navigation smoke tests the Qt ports have can't be
  replicated here — visual checks are manual.
- No sandbox, but the webview runs from `tauri://localhost`, so relative images
  can't load as `file://`. The frontend rewrites relative `<img src>` to the
  asset protocol via `convertFileSrc` (enabled + scoped in `tauri.conf.json`).
- `frontend/` has copies of the shared web assets (styles, highlight.js,
  mermaid.js) so Tauri can bundle `frontendDist`.
