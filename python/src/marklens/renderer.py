"""Markdown → HTML, plus the Mermaid rewrite and the page shell.

The Markdown engine differs from the Swift app's (markdown-it-py vs
swift-markdown/cmark), but the *contract* is the same — see
``shared/spec/fixtures/render_cases.json``. HTML is enabled so raw ``<img>``
and ``<p align>`` tags in the source pass through, as the Swift viewer allowed.
"""

from __future__ import annotations

import re
from html import unescape
from typing import cast

from markdown_it import MarkdownIt
from markdown_it.renderer import RendererHTML
from mdit_py_plugins.tasklists import tasklists_plugin


def _make_parser() -> MarkdownIt:
    md = MarkdownIt("commonmark", {"html": True, "linkify": False})
    md.enable(["table", "strikethrough"])
    md.use(tasklists_plugin, enabled=True)
    # markdown-it-py renders strikethrough as <s>; GFM (and cmark-gfm, and
    # GitHub) use <del>. Align to the GFM convention so all ports agree.
    # Stubs type rule values as MethodType, but any render-rule callable works.
    rules = cast(RendererHTML, md.renderer).rules
    rules["s_open"] = lambda *_a, **_k: "<del>"  # type: ignore[assignment]
    rules["s_close"] = lambda *_a, **_k: "</del>"  # type: ignore[assignment]
    return md


_MD = _make_parser()

# <pre><code class="language-mermaid">…</code></pre>  emitted by the fenced-code
# renderer for a ```mermaid block. markdown-it may add extra classes, so match
# loosely on the language token.
_MERMAID_BLOCK = re.compile(
    r'<pre><code class="[^"]*\blanguage-mermaid\b[^"]*">(.*?)</code></pre>',
    re.DOTALL,
)


# Heading slugs, GitHub's algorithm - see shared/spec/fixtures/render_cases.json.
# CommonMark says nothing about heading ids, so a plain parser emits <h2> with no
# id and every "#section" link in a document lands nowhere. GitHub, and VS Code's
# preview, add them; documents are written expecting that.
_HEADING = re.compile(r"<h([1-6])>(.*?)</h([1-6])>", re.DOTALL)
_TAGS = re.compile(r"<[^>]+>")
# Everything except word characters, hyphens and spaces is dropped rather than
# replaced: "Route A - Android Studio" (em dash) loses the dash while the spaces
# around it survive as two hyphens, hence links like "#route-a--android-studio".
_NOT_IN_SLUG = re.compile(r"[^\w\- ]", re.UNICODE)


def _slug(inner_html: str) -> str:
    text = unescape(_TAGS.sub("", inner_html)).strip().lower()
    return _NOT_IN_SLUG.sub("", text).replace(" ", "-")


def _add_heading_ids(html: str) -> str:
    seen: dict[str, int] = {}

    def repl(m: re.Match[str]) -> str:
        level, inner, closing = m.group(1), m.group(2), m.group(3)
        if level != closing:  # can't happen: headings don't nest
            return m.group(0)
        base = _slug(inner)
        if not base:
            return m.group(0)  # nothing to slug: leave the heading alone
        n = seen.get(base, 0)
        seen[base] = n + 1
        # Repeats get -1, -2 ..., so two "Prerequisites" headings stay reachable.
        ident = base if n == 0 else f"{base}-{n}"
        return f'<h{level} id="{ident}">{inner}</h{level}>'

    return _HEADING.sub(repl, html)


def render_body(markdown_text: str) -> str:
    """Markdown → HTML body (Mermaid blocks rewritten, no page shell)."""
    html = _MD.render(markdown_text)
    return _add_heading_ids(_rewrite_mermaid(html))


def _rewrite_mermaid(html: str) -> str:
    """Turn highlighted ```mermaid blocks into raw <div class="mermaid"> that
    mermaid.js can render — unescaping the diagram source, since the code
    renderer HTML-escaped it."""

    def repl(m: re.Match[str]) -> str:
        return f'<div class="mermaid">{unescape(m.group(1))}</div>'

    return _MERMAID_BLOCK.sub(repl, html)


def contains_mermaid(markdown_text: str) -> bool:
    return bool(_MERMAID_BLOCK.search(_MD.render(markdown_text)))


def page(body: str, *, asset_base: str, dark: bool = False) -> str:
    """Wrap a rendered body in the shared HTML shell.

    ``asset_base`` is an absolute ``file://…/shared/web`` URL; the document's
    own images resolve against the webview's base URL (its folder) instead.
    """
    theme = "dark" if dark else "light"
    hljs = "hljs-dark.css" if dark else "hljs-light.css"
    return f"""<!DOCTYPE html>
<html lang="en" data-theme="{theme}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="{asset_base}/styles.css">
<link rel="stylesheet" id="hljs-theme" href="{asset_base}/{hljs}">
<script src="{asset_base}/highlight.min.js" defer></script>
<script src="{asset_base}/mermaid.min.js" defer></script>
</head>
<body>
<article id="content">
{body}
</article>
<script>
// Back has to return to where a link was read, not merely to the previous
// document: following an anchor within a document is a move too. The host
// cannot record the position itself - acceptNavigationRequest is synchronous
// and the scroll offset lives here - so the page captures it on the way down,
// before the browser scrolls, and hands it back when the host asks.
window.__mlBack = (function () {{
    var stack = [];
    document.addEventListener('click', function (e) {{
        var a = e.target && e.target.closest ? e.target.closest('a[href^="#"]') : null;
        if (a) {{ stack.push(document.scrollingElement.scrollTop); }}
    }}, true);
    window.__mlBackDepth = function () {{ return stack.length; }};
    return function () {{
        if (!stack.length) {{ return false; }}
        document.scrollingElement.scrollTop = stack.pop();
        return true;
    }};
}})();
window.addEventListener('DOMContentLoaded', function () {{
    if (window.hljs) {{
        document.querySelectorAll('pre code').forEach(function (el) {{ window.hljs.highlightElement(el); }});
    }}
    if (window.mermaid) {{
        var dark = document.documentElement.dataset.theme === 'dark';
        window.mermaid.initialize({{ startOnLoad: false, securityLevel: 'strict', theme: dark ? 'dark' : 'default' }});
        window.mermaid.run({{ querySelector: '.mermaid' }});
    }}
}});
</script>
</body>
</html>"""