#include "assets.h"

#include <QFile>
#include <QUrl>

#ifndef MARKLENS_SHARED_DIR
#error "MARKLENS_SHARED_DIR must be defined by the build (path to shared/)"
#endif

namespace {
QString readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}
} // namespace

namespace assets {

QString webDir() {
    return QStringLiteral(MARKLENS_SHARED_DIR) + QStringLiteral("/web");
}

QString assetBaseUrl() {
    return QUrl::fromLocalFile(webDir()).toString();
}

QString helpHtml() {
    const QString shared = QStringLiteral(MARKLENS_SHARED_DIR);
#if defined(Q_OS_MACOS)
    const QString os = "macos";
#elif defined(Q_OS_WIN)
    const QString os = "windows";
#else
    const QString os = "linux";
#endif
    const QString steps = readFile(shared + "/help_default_" + os + ".html");
    return readFile(shared + "/help.html").replace("<!--DEFAULT_APP_STEPS-->", steps);
}

} // namespace assets
