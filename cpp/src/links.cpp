#include "links.h"

#include <QUrl>

#include <filesystem>

namespace links {

std::optional<QString> externalUrl(const QString &href) {
    const QUrl url(href);
    const QString scheme = url.scheme();
    if (!scheme.isEmpty() && scheme != QLatin1String("file"))
        return href;
    return std::nullopt;
}

std::optional<QString> documentRelativePath(const QString &href, const QString &docPath) {
    // Keep an empty left side: "#frag" -> "" before the fragment.
    const QString pathPart = href.section('#', 0, 0);
    if (pathPart.isEmpty())
        return std::nullopt;

    // An href may be percent-encoded ("My%20Doc.md") or raw ("My Doc.md");
    // fromPercentEncoding handles the former and leaves the latter untouched.
    const QString decoded = QUrl::fromPercentEncoding(pathPart.toUtf8());

    namespace fs = std::filesystem;
    const fs::path folder = fs::path(docPath.toStdString()).parent_path();
    // Lexical normalization (no filesystem access) so ".." collapses without
    // following symlinks — matching Swift standardizedFileURL / Python normpath.
    const fs::path resolved = (folder / decoded.toStdString()).lexically_normal();
    return QString::fromStdString(resolved.string());
}

} // namespace links
