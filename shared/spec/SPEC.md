# Marklens — behavior spec (shared across all ports)

A fast, native Markdown **viewer** (not an editor). Open a `.md`, see it rendered
with syntax highlighting and Mermaid diagrams. This spec is the contract every
port (Python/PySide6, C++/Qt, Rust/Tauri) must satisfy. The JSON fixtures beside
it are executable: each port loads them into its own test harness.

## Not in scope for the ports

Everything that existed in the macOS/iOS original *only* to fight the App
Sandbox is deleted here, because Windows/Linux desktop apps aren't sandboxed:

- No security-scoped bookmarks, no folder-grant flow, no "allow this folder" UI.
- No custom `marklens-doc:` URL scheme. The page's base URL is set to the
  document's own folder (`file://…/`), so relative images and links resolve
  natively — the exact thing the sandbox forbade.

## Rendering

1. Markdown → HTML via a CommonMark + GFM engine (tables, task lists,
   strikethrough, autolinks, fenced code).
2. A fenced code block tagged `mermaid` is rewritten from
   `<pre><code class="language-mermaid">…</code></pre>` to
   `<div class="mermaid">…</div>` (unescaped), so mermaid.js renders it.
3. The body is wrapped in the shared HTML shell (`shared/web/`): `styles.css`,
   `highlight.min.js`, `mermaid.min.js`, and a light/dark hljs theme.
4. `highlight.js` highlights `pre code` on load; mermaid runs on `.mermaid`.

## Links (intercepted, not followed by the webview)

- **External** (`https:`, `mailto:`, …) → open in the system browser.
- **In-page anchor** (`#heading`) → let the webview scroll natively.
- **Relative to another document** (`OTHER.md`, `../notes/X.md`) → open that
  file in the viewer. Fragment is dropped (no cross-file deep-link yet).
- Resolution is relative to the **document's folder**, and must handle `../`
  traversal, percent-encoded and raw spaces, and bare-fragment / empty hrefs
  (which resolve to nothing).

## Behavior

- **Reload**: re-read the file on demand; **auto-reload** when it changes on
  disk (a cross-platform file watcher). Atomic saves (write-temp-then-rename)
  must be tracked, not just in-place writes.
- **Find** in page, **zoom** in/out/reset, **export to PDF**, **reveal in the
  system file manager**.

## Gotchas (learned the hard way — every webview port hits these)

- **Don't re-navigate from inside a navigation callback.** Following a link
  means loading a new document, but doing that *synchronously* from within the
  webview's navigation hook (Qt `acceptNavigationRequest`, and the equivalent
  elsewhere) re-enters the engine's navigation machinery and **traps/aborts**
  (SIGTRAP, exit 133). Defer the load: Qt uses a queued signal; Tauri should
  post the open onto the next tick / an event rather than acting in the handler.
  Both the Python and C++ ports crashed on the first real link click and were
  fixed by deferring. Guard it with a nav test, not just a render smoke.

## The fixtures

- `fixtures/render_cases.json` — markdown input → substring assertions on the
  emitted HTML. Assertions are deliberately tolerant (assert `<table>` exists,
  not exact attribute order), so different GFM engines can pass the same
  contract.
- `fixtures/link_cases.json` — (href, document path) → resolved path or null.