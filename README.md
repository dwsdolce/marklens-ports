# marklens-ports

The Marklens Markdown viewer, reimplemented three ways for fun and comparison:
**Python/PySide6**, **C++/Qt**, and **Rust/Tauri**. A Rosetta Stone — one
behavior, three stacks.

The original is a native macOS/iOS app (SwiftUI + WebKit). These target
Windows and Linux (and macOS) as ordinary desktop apps, so everything the
original did to satisfy the App Sandbox is gone — see `shared/spec/SPEC.md`.

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
```

The point of `shared/spec/fixtures` is that all three implementations prove
themselves against the *same* cases. Different Markdown engines emit slightly
different HTML, so the render assertions are tolerant substrings (assert a
`<table>` exists, not its exact attributes).

## Status

- [x] Phase 0 — shared foundation (assets, spec, fixtures, sample)
- [x] Phase 1 — Python/PySide6 (ruff+mypy clean, 34 tests, smoke passes)
- [x] Phase 2 — C++/Qt (25/25 fixture rows, smoke passes; md4c engine)
- [x] Phase 3 — Rust/Tauri (core fixtures pass; comrak engine; app launches —
      visual GUI check is manual, Tauri has no headless webview)