//! Markdown → HTML, the Mermaid rewrite, and the page shell.
//! Contract: ../../shared/spec/fixtures/render_cases.json.

use comrak::{markdown_to_html, Options};
use regex::Regex;
use std::sync::OnceLock;

/// Markdown → HTML body (Mermaid blocks rewritten, no page shell).
pub fn render_body(markdown: &str) -> String {
    let mut options = Options::default();
    options.extension.table = true;
    options.extension.strikethrough = true; // renders <del>, matching GFM
    options.extension.tasklist = true;
    options.extension.autolink = true;
    // Let raw <img>/<p align> in the source pass through, like the Swift viewer.
    options.render.unsafe_ = true;

    rewrite_mermaid(&markdown_to_html(markdown, &options))
}

pub fn contains_mermaid(markdown: &str) -> bool {
    mermaid_re().is_match(&render_body(markdown))
}

fn mermaid_re() -> &'static Regex {
    static RE: OnceLock<Regex> = OnceLock::new();
    RE.get_or_init(|| {
        // <pre><code class="…language-mermaid…">…</code></pre>
        Regex::new(r#"(?s)<pre><code class="[^"]*\blanguage-mermaid\b[^"]*">(.*?)</code></pre>"#)
            .unwrap()
    })
}

/// Turn highlighted ```mermaid blocks into raw <div class="mermaid"> that
/// mermaid.js can render, unescaping the diagram source.
fn rewrite_mermaid(html: &str) -> String {
    mermaid_re()
        .replace_all(html, |caps: &regex::Captures| {
            format!("<div class=\"mermaid\">{}</div>", unescape_html(&caps[1]))
        })
        .into_owned()
}

fn unescape_html(s: &str) -> String {
    s.replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'")
        .replace("&amp;", "&") // last, so "&amp;lt;" -> "&lt;" not "<"
}

/// Wrap a rendered body in the shared HTML shell. `asset_base` is an absolute
/// URL to the shared web dir; the document's own images resolve separately.
pub fn page(body: &str, asset_base: &str, dark: bool) -> String {
    let theme = if dark { "dark" } else { "light" };
    let hljs = if dark { "hljs-dark.css" } else { "hljs-light.css" };
    format!(
        r#"<!DOCTYPE html>
<html lang="en" data-theme="{theme}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="{base}/styles.css">
<link rel="stylesheet" id="hljs-theme" href="{base}/{hljs}">
<script src="{base}/highlight.min.js" defer></script>
<script src="{base}/mermaid.min.js" defer></script>
</head>
<body>
<article id="content">
{body}
</article>
<script>
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
</html>"#,
        theme = theme,
        base = asset_base,
        hljs = hljs,
        body = body
    )
}
