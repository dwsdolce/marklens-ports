#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStandardPaths>

namespace {

constexpr auto kRecentKey = "recentFiles";
constexpr auto kLastOpenDirKey = "lastOpenDir";
constexpr auto kToolBarStyleKey = "toolBarStyle";
constexpr int kRecentMax = 10;

// A corrupt file is treated as absent rather than fatal: settings are a
// convenience, and refusing to start because a recent-files list will not parse
// would be a poor trade.
QJsonObject load() {
    QFile f(settings::filePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

void save(const QJsonObject &data) {
    const QString path = settings::filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
}

// Read-modify-write, so a key another port owns is not dropped.
void put(const QString &key, const QJsonValue &value) {
    QJsonObject data = load();
    data.insert(key, value);
    save(data);
}

// Windows filenames are case-insensitive, so the comparison key folds case
// there and nowhere else.
QString recentKey(const QString &path) {
#if defined(Q_OS_WIN)
    return settings::canonical(path).toLower();
#else
    return settings::canonical(path);
#endif
}

} // namespace

namespace settings {

QString canonical(const QString &path) { return QDir::fromNativeSeparators(path); }

QString filePath() {
    const QByteArray override = qgetenv("MARKLENS_SETTINGS");
    if (!override.isEmpty())
        return QString::fromLocal8Bit(override);
    // Qt's GenericConfigLocation and Rust's dirs::preference_dir() return the
    // same directory on all three platforms - measured, not assumed, because an
    // honest-looking guess that they differed on Windows sent this to
    // AppData/Roaming for a while and silently unshared the file.
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return root + "/Marklens/settings.json";
}

QStringList recentFiles() {
    QStringList out;
    QSet<QString> seen;
    for (const QJsonValue &value : load().value(kRecentKey).toArray()) {
        if (!value.isString())
            continue;
        const QString spelled = canonical(value.toString());
        const QString key = recentKey(spelled);
        if (!seen.contains(key)) {
            seen.insert(key);
            out << spelled;
        }
    }
    return out;
}

void addRecent(const QString &path) {
    const QString spelled = canonical(path);
    const QString key = recentKey(spelled);

    QStringList recent;
    recent << spelled;
    for (const QString &existing : recentFiles()) {
        if (recentKey(existing) != key)
            recent << existing;
    }
    while (recent.size() > kRecentMax)
        recent.removeLast();

    QJsonArray array;
    for (const QString &entry : recent)
        array.append(entry);
    put(kRecentKey, array);
}

void clearRecent() { put(kRecentKey, QJsonArray{}); }

QString lastOpenDir() {
    const QJsonValue value = load().value(kLastOpenDirKey);
    return value.isString() ? canonical(value.toString()) : QString();
}

void setLastOpenDir(const QString &path) { put(kLastOpenDirKey, canonical(path)); }

int toolBarStyle(int fallback) {
    const QJsonValue value = load().value(kToolBarStyleKey);
    if (value.isDouble())
        return value.toInt(fallback);
    // A store written by hand could hold a string; take it if it is a number.
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        if (ok)
            return parsed;
    }
    return fallback;
}

void setToolBarStyle(int style) { put(kToolBarStyleKey, style); }

} // namespace settings
