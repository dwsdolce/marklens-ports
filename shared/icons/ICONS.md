# Toolbar icons

Thin-line icons approximating the Swift app's SF Symbols. **Not** SF Symbols
themselves — those are Apple-platform-only under the SF Symbols license and
can't be redistributed here. These are legal look-alikes, shared by all three
ports so they render identically.

| File | Function | Swift SF Symbol | Source |
|---|---|---|---|
| back.svg | Back to Previous Document | arrow.uturn.backward | Lucide `undo-2` |
| find.svg | Find | magnifyingglass | Lucide `search` |
| zoom-out.svg | Zoom Out | minus.magnifyingglass | Lucide `zoom-out` |
| zoom-in.svg | Zoom In | plus.magnifyingglass | Lucide `zoom-in` |
| actual-size.svg | Actual Size | 1.magnifyingglass | original (magnifier + "1") |
| export.svg | Export as PDF | arrow.up.doc | Lucide `file-up` (document + up arrow, deliberately NOT the share/box glyph so it isn't mistaken for the macOS export/share icon) |
| reveal.svg | Reveal in Finder | folder | Lucide `folder` |
| reload.svg | Reload | arrow.clockwise | Lucide `rotate-cw` |
| reload-alert.svg | Reload (stale) | arrow.clockwise.circle.fill | Lucide `rotate-cw` + accent dot |
| find-prev.svg | Previous match | chevron.up | Lucide `chevron-up` |
| find-next.svg | Next match | chevron.down | Lucide `chevron-down` |
| close.svg | Close find bar | xmark | Lucide `x` |

Lucide icons are ISC-licensed (© Lucide Contributors), which permits
redistribution. Icons use `stroke="currentColor"` so they follow light/dark.

Two glyphs have no counterpart in any open set and are drawn here. `actual-size`
answers SF's `1.magnifyingglass`, which nothing else offers: the nearest stock
candidates (Material's `magnify-expand`, Remix's aspect-ratio box) both read as
"fit to window", the wrong action. `reload-alert` badges the reload glyph the
way SF's filled circle variant does, rather than substituting a warning symbol
that stops reading as reload at all.

`back` has no counterpart in the Swift app's macOS toolbar, which opens links in
new windows and so never needs it. It is taken from the iOS toolbar, where a
link replaces the document in place exactly as it does in all three ports here.
