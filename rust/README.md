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
crate this links against, so `setup` installs it where it is needed. It also warms the crate cache,
which turns the first build from a long silence into a step that reports itself.

**Prerequisites — you install these.** Rust via [rustup](https://rustup.rs), at
least 1.85; anything older cannot parse the 2024-edition manifests in this
dependency tree and fails with `feature edition2024 is required` a long way from
anything informative. On Linux, the webkit2gtk development packages. macOS uses
WKWebView and Windows uses WebView2, both part of the OS — and on Windows the
installer downloads Microsoft's bootstrapper if the runtime turns out to be
missing, which it will not be on Windows 11.

**Dependencies — `setup` installs these.** The Tauri CLI, via `cargo install`,
on macOS and Linux, where it bundles the `.dmg`, `.deb` and `.rpm`. On Windows
it is reported but not installed: packaging there is Inno Setup's job now, and
the CLI takes minutes to build from source for nothing.

## Run

```bash
packaging/run_win ../shared/spec/sample/index.md      # run_win.bat in cmd
packaging/run_mac ../shared/spec/sample/index.md
packaging/run_linux ../shared/spec/sample/index.md
```

This is the tool for iterating on the code. It always runs the working tree,
rebuilding whatever changed via `cargo run`. A debug build, because that
is what iterating wants; **Package** produces the optimised artefact.
Read the script if you want the details; there is nothing in it you could not
type yourself, which is rather the point.

It never runs anything under `dist/` — that is what **Package** is for.

## Test

```bash
packaging/test_win         # Git Bash or Cygwin
packaging\test_win.bat     # cmd
packaging/test_mac
packaging/test_linux
```

One per platform, the same shape as `run_*` and `build_*`. Arguments go
straight to cargo, so `packaging/test_linux link_cases` runs one test. By hand
it is `cd src-tauri && cargo test` — core: the comrak renderer and link
resolver against the shared fixtures. The wrapper exists so the verb matches
the other two ports; the `cd` is the only thing it saves you.

`tests/fixtures.rs` loads `../../shared/spec/fixtures/*.json` — the same
contract the Python and C++ ports satisfy. comrak matched it with no per-engine
fixups (strikethrough is `<del>` natively).

## Package

```bash
packaging/build_win        # Git Bash or Cygwin -> installer/Marklens_Rust_V<ver>.exe
packaging\build_win.bat    # cmd, same output
packaging/build_mac        # -> Marklens_Rust_V<ver>.dmg  (or `build_mac app`)
packaging/build_linux      # -> dist/Marklens-Rust-<ver>-{AppImage,deb}
```

Windows uses **Inno Setup**, the same as the other two ports and through the
same two shared includes, so the existing-install prompt and the "Open with"
registration behave identically across all three. macOS and Linux keep Tauri's
bundler, which writes the `.dmg`, `.deb` and `.rpm` itself.

Tauri's bundler used to build the Windows installer too, because it installs the
WebView2 runtime. It was dropped because its version must be three-part semver:
the build number never reached the installer, so every build looked like the
same version to it and it could never offer to replace an existing install.
`packaging/marklens-rust.iss` now checks for the runtime and downloads
Microsoft's bootstrapper when it is missing — a fallback for machines older than
Windows 11, which ships it. See
[../packaging/README.md](../packaging/README.md) for the details and for
signing.

## Notes

- **No GUI test.** The Qt ports drive a real window and assert that a document
  rendered; this port has only the shared fixtures. Tauri has no offscreen mode,
  so such a test would have to use a real window — but that is not what stops
  it. The obstacle is that `lib.rs` holds only the renderer and link resolver,
  while the window, menus and commands live in the binary, where no test can
  link them. C++ keeps its window code in a library, `marklens_gui`, for exactly
  this reason. See the Testing section of `../shared/spec/SPEC.md`.
- No sandbox, but the webview runs from `tauri://localhost`, so relative images
  can't load as `file://`. The frontend rewrites relative `<img src>` to the
  asset protocol via `convertFileSrc` (enabled + scoped in `tauri.conf.json`).
- `frontend/` needs the shared web assets and toolbar icons inside it, because
  that is what Tauri bundles as `frontendDist`. They are **copied in by
  `build.rs` on every build** and gitignored, rather than committed a second
  time: the originals live in `shared/`, where the other two ports read them,
  and deriving them is what stops the two drifting. Only `index.html`,
  `chrome.css` and `main.js` are this port's own.
