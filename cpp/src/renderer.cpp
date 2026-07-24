#include "renderer.h"

#include <QRegularExpression>

#include <md4c-html.h>

namespace {

// md4c-html streams output in chunks; collect them.
void collect(const MD_CHAR *text, MD_SIZE size, void *userdata) {
    auto *out = static_cast<QByteArray *>(userdata);
    out->append(text, static_cast<int>(size));
}

QString markdownToHtml(const QString &markdown) {
    const QByteArray utf8 = markdown.toUtf8();
    QByteArray out;
    // GitHub dialect: tables + strikethrough (<del>) + tasklists + permissive
    // autolinks. Raw HTML passes through (no MD_FLAG_NOHTML), so inline <img>
    // and <p align> survive — matching the Swift viewer.
    md_html(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), collect, &out,
            MD_DIALECT_GITHUB, 0);
    return QString::fromUtf8(out);
}

// <pre><code class="…language-mermaid…">…</code></pre> -> raw
// <div class="mermaid">…</div>, unescaping the diagram source (the code
// renderer HTML-escaped it).
const QRegularExpression &mermaidBlock() {
    static const QRegularExpression re(
        R"(<pre><code class="[^"]*\blanguage-mermaid\b[^"]*">(.*?)</code></pre>)",
        QRegularExpression::DotMatchesEverythingOption);
    return re;
}

QString unescapeHtml(QString s) {
    s.replace("&lt;", "<");
    s.replace("&gt;", ">");
    s.replace("&quot;", "\"");
    s.replace("&#39;", "'");
    s.replace("&amp;", "&"); // last, so "&amp;lt;" -> "&lt;" not "<"
    return s;
}

QString rewriteMermaid(const QString &html) {
    QString result;
    result.reserve(html.size());
    qsizetype last = 0;
    auto it = mermaidBlock().globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        result += html.mid(last, m.capturedStart() - last);
        result += "<div class=\"mermaid\">" + unescapeHtml(m.captured(1)) + "</div>";
        last = m.capturedEnd();
    }
    result += html.mid(last);
    return result;
}

} // namespace

namespace renderer {

QString renderBody(const QString &markdown) {
    return rewriteMermaid(markdownToHtml(markdown));
}

bool containsMermaid(const QString &markdown) {
    return mermaidBlock().match(markdownToHtml(markdown)).hasMatch();
}

QString page(const QString &body, const QString &assetBase, bool dark) {
    const QString theme = dark ? "dark" : "light";
    const QString hljs = dark ? "hljs-dark.css" : "hljs-light.css";
    return QStringLiteral(R"(<!DOCTYPE html>
<html lang="en" data-theme="%1">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="%2/styles.css">
<link rel="stylesheet" id="hljs-theme" href="%2/%3">
<script src="%2/highlight.min.js" defer></script>
<script src="%2/mermaid.min.js" defer></script>
</head>
<body>
<article id="content">
%4
</article>
<script>
window.addEventListener('DOMContentLoaded', function () {
    if (window.hljs) {
        document.querySelectorAll('pre code').forEach(function (el) { window.hljs.highlightElement(el); });
    }
    if (window.mermaid) {
        var dark = document.documentElement.dataset.theme === 'dark';
        window.mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: dark ? 'dark' : 'default' });
        window.mermaid.run({ querySelector: '.mermaid' });
    }
});
</script>
</body>
</html>)")
        .arg(theme, assetBase, hljs, body);
}

} // namespace renderer
