# Rust/Tauri port — working notes (resume from here)

Durable state for Phase 3. Update as you go.

## Decisions

- **Markdown engine: comrak** (CommonMark + GFM). Enable extensions table,
  strikethrough, tasklist, autolink, and `render.unsafe_ = true` so raw
  `<img>` / `<p align>` pass through (like md4c's no-NOHTML). comrak renders
  strikethrough as `<del>` and fenced code as `<pre><code class="language-X">`
  — expected to match the shared fixtures with no per-engine fixups (as md4c
  did; unlike Python markdown-it-py which needed a `<s>`→`<del>` override).
- **Tauri 2.x.** Native OS webview (WKWebView on macOS) — no bundled Chromium.
  Frontend is HTML/CSS/JS reusing `shared/web/` assets. Rust backend does
  markdown→HTML, link resolution, file watching.
- **Toolchain:** rustc/cargo 1.97 via rustup. Tauri CLI via
  `cargo install tauri-cli` (compiles from source).

## Off-sandbox simplifications (same as the Qt ports)

No folder grants / bookmarks / custom sandbox scheme. BUT Tauri's webview runs
from `tauri://localhost`, so relative `file://` images do NOT resolve on their
own — the render step must rewrite relative `<img src>` to Tauri's asset
protocol via `convertFileSrc(absolutePath)` (the asset protocol must be enabled
+ scoped in tauri.conf.json). This is the Tauri analog of the Swift
`marklens-doc:` scheme / the Qt file:// base URL.

## Gotcha carried over (see shared/spec/SPEC.md)

Don't re-navigate from inside a navigation handler — it trapped in both Qt
ports. In Tauri, handle link clicks by posting to Rust and re-rendering via an
event/command on the next tick, not synchronously mid-click.

## Plan / status

- [x] Rust toolchain installed (rustc/cargo 1.97)
- [x] Tauri CLI installed (`cargo install tauri-cli` ^2.0)
- [x] Core: renderer.rs (comrak + mermaid rewrite + page shell) + links.rs
- [x] Core tests pass shared fixtures — **2/2** (render_cases, link_cases).
      comrak matched with NO per-engine fixups (like md4c).
- [x] Tauri app: index.html shell + main.js (reuse shared/web assets)
- [x] Commands: initial_document, render_document, follow_link, watch_document
- [x] Relative images via convertFileSrc / asset protocol (protocol-asset
      feature + assetProtocol scope in tauri.conf.json)
- [x] Compiles clean; **launches without crashing** (runtime + webview +
      embedded frontend load).
- [ ] VISUAL verification (image/mermaid/table/highlight render, links
      navigate): needs a human run — Tauri uses the macOS system webview, which
      has NO offscreen/headless mode (unlike Qt's QWebEngineView), so it can't
      be driven+screenshotted from a script. Run: `cargo run -- <file.md>`.

## Feature parity (added after first pass was too thin)

The first pass was just a renderer (open-via-CLI only). Now at parity with the
Qt ports:
- **Native menu bar** (Tauri menu API, built in Rust `build_menu`): App
  (About/Quit), File (Open ⌘O, Open Recent, Reload ⌘R, Auto-Reload check,
  Export as PDF ⌘⇧E, Show in Finder, Close ⌘W), Edit (Copy/Select All, Find ⌘F,
  Find Next ⌘G, Find Prev ⇧⌘G), View (Back ⌘[, Zoom In/Out/Actual), Window
  (Minimize/Zoom), Help (Marklens Help).
- **Frontend toolbar + find bar + help overlay** (chrome.css/index.html/main.js).
- **Open dialog** via tauri-plugin-dialog; **Reveal** via
  tauri-plugin-opener::reveal_item_in_dir; **Recent files** persisted as JSON in
  app_config_dir (menu rebuilt on change); **Zoom** via native
  `WebviewWindow::set_zoom`; **PDF export** via `window.print()` (system
  print→save-as-PDF — WKWebView has no direct printToPdf exposed by Tauri);
  **Find** via `window.find()` (no Qt-style findText in a system webview);
  **Help** overlay loads `help_html` command (include_str! of shared/help.html +
  OS snippet, so it's bundle-safe).
- Menu events emit to the frontend for webview actions (reload/find/back/print);
  backend handles open/reveal/recent/zoom directly.

Compiles clean; launches clean. GUI still needs a human visual check.

Design notes:
- index.html is the static shell (in frontendDist) that loads the shared web
  assets; `render_document` returns just the BODY, injected via innerHTML. So
  `renderer::page()` is unused by this port (only render_body).
- Uses global Tauri API (`withGlobalTauri`) → no npm/bundler; main.js reads
  `window.__TAURI__.{core,event}`.
- Link clicks await `follow_link` (async) → the re-render is naturally deferred
  past the click handler, sidestepping the nav-reentrancy trap the Qt ports hit.
- File watch is on the parent dir (atomic saves), filtered to the file path.

## Layout (intended)

```
rust/
  Cargo.toml               (or workspace)
  src-tauri/
    Cargo.toml  tauri.conf.json  build.rs
    src/ main.rs renderer.rs links.rs
  frontend/  index.html  main.js   (+ symlink/copy of shared/web)
```

## Commands

```bash
source "$HOME/.cargo/env"
cargo test                       # core fixtures (from the crate dir)
cargo tauri dev                  # run the app
```
