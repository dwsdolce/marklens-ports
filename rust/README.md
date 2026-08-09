# Marklens — Rust / Tauri port

A Markdown viewer. The **Rust backend** (in `src-tauri/`) renders Markdown→HTML
with **comrak**, resolves links, and watches the file; the **frontend**
(`frontend/`) is HTML/CSS/JS reusing the shared web assets, running in the
platform's native webview (WKWebView on macOS — no bundled Chromium).

## Setup

```bash
cd rust
packaging/setup            # checks the environment, installs what it can
packaging/setup --check    # report only, change nothing
```

`Cargo.toml` already is this port's manifest — `cargo build` fetches and builds
every library dependency by itself — so there is far less here than for the C++
port. What Cargo has no slot for is *tools*: the Tauri CLI is a binary, not a
crate this links against, so `setup` installs it. It also warms the crate cache,
which turns the first build from a long silence into a step that reports itself.

**Prerequisites — you install these.** Rust via [rustup](https://rustup.rs), at
least 1.85; anything older cannot parse the 2024-edition manifests in this
dependency tree and fails with `feature edition2024 is required` a long way from
anything informative. On Linux, the webkit2gtk development packages. macOS uses
WKWebView and Windows uses WebView2, both part of the OS — and the Windows
installer carries the WebView2 bootstrapper for machines without it.

**Dependencies — `setup` installs these.** The Tauri CLI, via `cargo install`.

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

## Run

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
```

Same command shape as the other two ports. It prefers a release build and falls
back to `cargo run --release`, building if there is nothing there yet.

## Package

```bash
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Rust_V<ver>.exe
packaging\build_win.bat    # cmd, same output
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
