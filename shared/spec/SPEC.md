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

## Images

The page's base URL is the document's own folder, so `![](diagram.png)`
resolves beside the file. What decodes it is the webview, not the port — which
is why the supported formats are a spec matter at all. The three ports embed
three different engines, and each engine carries its own decoders.

Every port, on every platform, renders:

`PNG`, `JPEG`, `GIF`, `WebP`, `BMP`, `ICO`, `SVG`

That list is the contract. It is deliberately a list of what works rather than
a list of what does not, because the set that does not work is open-ended and
changes with each engine and each platform. Anything absent from it is
unsupported; a document that needs one of those formats should carry a
converted copy.

A port may happen to render more, and that is not a promise. A macOS build
displays TIFF, HEIC and whatever else the system decoder knows, because
WKWebView hands decoding to ImageIO and inherits the operating system's list.
A document relying on that is relying on a platform rather than on Marklens.
Nothing joins the required set until all three ports render it on all three
platforms.

This is a deliberate divergence from the macOS original, which has no format
list of its own: it passes the HTML to WKWebView and inherits ImageIO's, so it
shows formats these ports cannot. The ports that embed Chromium — Qt WebEngine
for the C++ and Python ports everywhere, WebView2 for Rust on Windows — and
WebKitGTK for Rust on Linux all ship a smaller, web-oriented set instead.
Closing the gap would mean enumerating every format ImageIO decodes and
shipping a converter in three languages, for formats a Markdown document
almost never carries.

The seven were measured, not assumed: the same document rendered through
Chromium and through WebKitGTK, the two narrowest engines in use. Both decode
all seven; neither decodes TIFF.

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

## Launch with no document

Started with no file named on the command line, every port reopens the document
it last had open: the newest entry in the recent list that still exists. The
list is walked rather than trusting its head, because entries outlive the files
they name - renamed, deleted, or on a volume that is not mounted.

macOS complicates this, because a document opened from Finder also arrives with
nothing on the command line. The path comes later, as an Apple Event, once the
event loop is running. Reopening at once would show the previous document first
and push it back to the top of the recent list, so the reopen is deferred
briefly and stands down if a document turns up in the meantime.

## Empty state

With nothing to reopen - a first run, or a list whose every entry has since
gone - each port shows the shared page shell containing:

```html
<p style='opacity:.6;padding:1rem'>Open a Markdown file to view it.</p>
```

and hides the toolbar's document name, there being nothing to name.

This has no counterpart in the original, which was document-based: a SwiftUI
`DocumentGroup` never presents an empty document window, so macOS puts up the
open panel instead and the situation cannot arise. The ports are ordinary
single-window applications and can be started with no argument, so they need an
answer the original never had to give. It is written down here because it was
invented once, in the Rust port, and the other two spent a while silently
disagreeing by showing a blank view.

## Window chrome

The title bar names the application and its version, and never changes:
`Marklens C++ 0.1.0 (39)`, `Marklens Python 0.1.0 (39)`, `Marklens Rust
0.1.0 (39)`. The document is named on the toolbar instead, at the left of the
same row as the icons, with a menu listing its path - the file, then each
enclosing folder out to the filesystem root. Choosing the file reveals it in the
file manager; choosing a folder opens it.

The original puts the document's name in the title bar and hangs that menu off
the macOS proxy icon beside it. Only macOS has such a thing: Qt can show one
(`setWindowFilePath`) but Windows and Linux have no equivalent at all, so
drawing the name and its menu in the toolbar is what lets every port offer the
same affordance in the same place on every platform.

It follows that the window title says nothing about which document is open.
Anything that needs to know - a test, most obviously - has to ask the port, not
read the title.

## Toolbar

Icons, no labels, in this order, pushed to the right:

    back · find · zoom out · zoom in · actual size · export · reveal · reload

which is the original's order with **back** added at the front. That toolbar has
no back button because it opens a link in a new window; these ports replace the
document in place and so need a way back. The glyph is the one the original uses
for that button on iOS, where the same thing happens.

**Open** is deliberately absent, as it is there: a document app opens documents
through File and Open Recent.

The glyphs are not SF Symbols, which are Apple-platform-only and cannot be
redistributed. They are open look-alikes shared by all three ports; see
`shared/icons/ICONS.md` for the substitutions and why two of them are drawn by
hand.

## Find

A bar of its own below the toolbar, hidden until asked for, holding a search
field, the match count (`3 of 12`, or `No matches`), and previous, next and
close buttons. Escape closes it. Searching happens as you type; Return steps to
the next match.

The count is the reason the Rust port cannot use the webview's own `window.find`
- it reports whether something matched, not how many - so that port marks the
hits in the DOM itself and colours the active one apart from the rest.

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

## Testing

Two layers, and the ports are not level with each other on the second.

**The shared fixtures** above are the contract every port meets, and all three
do: `test_core` (C++), `test_renderer`/`test_links` (Python), `fixtures.rs`
(Rust).

**Driving the real application** is the layer that catches what fixtures cannot:
that a document *renders*, rather than merely that the window opened. The worst
bugs found so far were both of that kind - a WebEngine helper that could not
start, so nothing ever rendered while the app looked healthy, and relative
images silently resolving against the wrong folder. C++ and Python cover it with
`smoke_gui` (loads the sample and asserts, via JavaScript, that the image
loaded, mermaid drew and code highlighted) and `nav_smoke` (clicks a relative
link and asserts the viewer navigated).

The Rust port has no equivalent, and the obstacle is structural rather than a
missing file. C++ keeps its window code in a library, `marklens_gui`, which both
GUI tests link against. Rust's `lib.rs` holds only the renderer and the link
resolver; the window, the menus and the commands all live in the binary, where
no test can reach them. Closing the gap means moving the application into the
library and adding a second binary to drive it - the arrangement C++ already
has.

Headless is not part of the contract. The Qt tests run under
`QT_QPA_PLATFORM=offscreen` so they work on a CI runner with no display and do
not seize the screen on every build, but they assert on the DOM rather than on
pixels, so a real window would satisfy them equally. Tauri has no offscreen mode
at all, so a Rust GUI test would necessarily use a real window - a reason to
write one, not a reason not to.
