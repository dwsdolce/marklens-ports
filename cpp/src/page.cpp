#include "page.h"

#include "links.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QSet>

namespace {

bool isMarkdown(const QString &path) {
    static const QSet<QString> exts{"md", "markdown", "mdown", "mkd", "txt"};
    return exts.contains(QFileInfo(path).suffix().toLower());
}

} // namespace

bool MarkdownPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool) {
    if (type != NavigationTypeLinkClicked)
        return true; // our own setHtml load, form posts, etc.

    // External (http/https/mailto/…) → hand to the system browser. Qt has
    // already resolved the href against the base URL; classify the resolved
    // URL with the same rule the fixtures pin down.
    if (links::externalUrl(url.toString()).has_value()) {
        QDesktopServices::openUrl(url);
        return false;
    }

    // Same document, just a #fragment → let the view scroll natively.
    if (url.hasFragment() && !m_documentPath.isEmpty() && url.toLocalFile() == m_documentPath)
        return true;

    // Another local document → open it in the viewer; other local files (image,
    // pdf) → let the OS handle them.
    if (url.isLocalFile()) {
        const QString target = url.toLocalFile();
        if (isMarkdown(target))
            emit openDocument(target);
        else
            QDesktopServices::openUrl(url);
        return false;
    }

    return false;
}
