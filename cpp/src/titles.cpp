#include "titles.h"

#include <QtGlobal>

namespace titles {

const bool kDocumentOnly =
#if defined(Q_OS_MACOS)
    true;
#else
    false;
#endif

QString appTitle() {
    return QStringLiteral(MARKLENS_DISPLAY_NAME " ") +
           (QStringLiteral(MARKLENS_BUILD).isEmpty()
                ? QStringLiteral(MARKLENS_VERSION)
                : QStringLiteral(MARKLENS_VERSION " (" MARKLENS_BUILD ")"));
}

QString forDocument(const QString &document, bool documentOnly) {
    if (documentOnly)
        return document.isEmpty() ? QStringLiteral(MARKLENS_DISPLAY_NAME) : document;
    return appTitle();
}

} // namespace titles
