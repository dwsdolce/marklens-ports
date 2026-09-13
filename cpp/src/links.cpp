#include "links.h"

#include <QUrl>

#include <filesystem>

namespace {

// Decode %XX escapes, leaving any % that does not start one alone.
//
// Not QUrl::fromPercentEncoding, which reads the two characters after every %
// as hex whether or not they are: "100%.md" came out as "100", a NUL byte, then
// "d", so a file with a percent sign in its name could not be opened. The
// other two ports' decoders leave an invalid escape untouched, and the shared
// link fixtures now require it.
QString percentDecode(const QString &text) {
    const auto hexValue = [](char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    const QByteArray in = text.toUtf8();
    QByteArray out;
    out.reserve(in.size());
    for (qsizetype i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            const int high = hexValue(in[i + 1]);
            const int low = hexValue(in[i + 2]);
            if (high >= 0 && low >= 0) {
                out.append(static_cast<char>(high * 16 + low));
                i += 2;
                continue;
            }
        }
        out.append(in[i]);
    }
    return QString::fromUtf8(out);
}

} // namespace

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
    // decoding handles the former and leaves the latter untouched.
    const QString decoded = percentDecode(pathPart);

    namespace fs = std::filesystem;
    const fs::path folder = fs::path(docPath.toStdString()).parent_path();
    // Lexical normalization (no filesystem access) so ".." collapses without
    // following symlinks — matching Swift standardizedFileURL / Python normpath.
    const fs::path resolved = (folder / decoded.toStdString()).lexically_normal();
    return QString::fromStdString(resolved.string());
}

} // namespace links
