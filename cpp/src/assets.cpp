#include "assets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>
#include <QUrl>

#ifndef MARKLENS_SHARED_DIR
#error "MARKLENS_SHARED_DIR must be defined by the build (path to shared/)"
#endif

namespace {

QString readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// Where the packaging scripts put the assets, relative to the executable.
QStringList bundleCandidates() {
    if (!QCoreApplication::instance())
        return {}; // applicationDirPath() is meaningless before the app exists
    const QString appDir = QCoreApplication::applicationDirPath();
    return {
#if defined(Q_OS_MACOS)
        // <bundle>.app/Contents/MacOS/<exe> -> <bundle>.app/Contents/Resources
        appDir + QStringLiteral("/../Resources/shared"),
#endif
        // Windows and Linux: alongside the executable in the deployed tree.
        appDir + QStringLiteral("/shared"),
    };
}

QString resolveSharedDir() {
    // An explicit override wins, which is what makes it possible to point a
    // packaged build at a working copy while debugging.
    const QString fromEnv = QString::fromLocal8Bit(qgetenv("MARKLENS_SHARED"));
    if (!fromEnv.isEmpty() && QFileInfo(fromEnv).isDir())
        return QDir(fromEnv).absolutePath();

    for (const QString &candidate : bundleCandidates()) {
        if (QFileInfo(candidate).isDir())
            return QDir(candidate).canonicalPath();
    }

    // Development build: read straight out of the repository.
    return QStringLiteral(MARKLENS_SHARED_DIR);
}

} // namespace

namespace assets {

QString sharedDir() {
    // Resolved once. Every caller runs after QApplication is constructed, so
    // the bundle probe above has an application directory to work from.
    static const QString dir = resolveSharedDir();
    return dir;
}

QString webDir() {
    return sharedDir() + QStringLiteral("/web");
}

QString assetBaseUrl() {
    return QUrl::fromLocalFile(webDir()).toString();
}

QString iconPath() {
    return sharedDir() + QStringLiteral("/icon.png");
}

QString helpHtml() {
    const QString shared = sharedDir();
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
