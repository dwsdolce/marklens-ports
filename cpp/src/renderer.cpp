#include "renderer.h"

#include <QHash>
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

// Heading slugs, GitHub's algorithm - see shared/spec/fixtures/render_cases.json.
// md4c has no notion of heading ids, so this runs over its output. CommonMark
// says nothing about them either, but documents are written expecting GitHub's:
// without them every "#section" link in a document lands nowhere.
const QRegularExpression &headingTag() {
    // No backreference for the closing level: headings cannot nest, so the lazy
    // (.*?) already stops at the first close, and the caller checks they agree.
    static const QRegularExpression re(R"(<h([1-6])>(.*?)</h([1-6])>)",
                                       QRegularExpression::DotMatchesEverythingOption);
    return re;
}

QString slugFor(const QString &innerHtml) {
    static const QRegularExpression tags(R"(<[^>]+>)");
    // Anything that is not a word character, hyphen or space is dropped rather
    // than replaced, so "Route A - Android Studio" (em dash) keeps the spaces
    // either side of it and slugs to "route-a--android-studio".
    static const QRegularExpression notInSlug(R"([^\w\- ])",
                                              QRegularExpression::UseUnicodePropertiesOption);
    QString text = unescapeHtml(QString(innerHtml).remove(tags)).trimmed().toLower();
    return text.remove(notInSlug).replace(QLatin1Char(' '), QLatin1Char('-'));
}

QString addHeadingIds(const QString &html) {
    QHash<QString, int> seen;
    QString result;
    result.reserve(html.size());
    qsizetype last = 0;
    auto it = headingTag().globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        result += html.mid(last, m.capturedStart() - last);
        const QString level = m.captured(1);
        const QString inner = m.captured(2);
        const QString slug = slugFor(inner);
        if (slug.isEmpty() || level != m.captured(3)) {
            result += m.captured(0); // nothing to slug, or levels disagree
        } else {
            const int n = seen[slug]++;
            // Repeats get -1, -2 ..., so two "Prerequisites" stay reachable.
            const QString id = n == 0 ? slug : slug + "-" + QString::number(n);
            result += "<h" + level + " id=\"" + id + "\">" + inner + "</h" + level + ">";
        }
        last = m.capturedEnd();
    }
    result += html.mid(last);
    return result;
}

} // namespace

namespace renderer {

QString renderBody(const QString &markdown) {
    return addHeadingIds(rewriteMermaid(markdownToHtml(markdown)));
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
