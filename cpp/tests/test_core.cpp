// Core tests driven by the shared, language-neutral fixtures — the same
// contract the Python port satisfies. QtTest + QJsonDocument, no extra deps.

#include "links.h"
#include "titles.h"
#include "renderer.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#ifndef MARKLENS_SHARED_DIR
#error "MARKLENS_SHARED_DIR must be defined by the build"
#endif

namespace {

QJsonObject loadFixture(const QString &name) {
    QFile f(QStringLiteral(MARKLENS_SHARED_DIR) + "/spec/fixtures/" + name);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open fixture %s", qPrintable(name));
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

class TestCore : public QObject {
    Q_OBJECT

private slots:
    void renderCases_data();
    void renderCases();

    void linkCases_data();
    void linkCases();

    void titleCases();

    void settingsCases();
};

void TestCore::renderCases_data() {
    QTest::addColumn<QString>("md");
    QTest::addColumn<QStringList>("contains");
    QTest::addColumn<QStringList>("absent");

    const QJsonArray cases = loadFixture("render_cases.json").value("cases").toArray();
    for (const QJsonValue &v : cases) {
        const QJsonObject c = v.toObject();
        QStringList contains, absent;
        for (const QJsonValue &s : c.value("contains").toArray())
            contains << s.toString();
        for (const QJsonValue &s : c.value("absent").toArray())
            absent << s.toString();
        QTest::newRow(c.value("name").toString().toUtf8().constData())
            << c.value("md").toString() << contains << absent;
    }
}

void TestCore::renderCases() {
    QFETCH(QString, md);
    QFETCH(QStringList, contains);
    QFETCH(QStringList, absent);

    const QString html = renderer::renderBody(md);
    for (const QString &needle : contains)
        QVERIFY2(html.contains(needle), qPrintable("expected: " + needle + "\nin:\n" + html));
    for (const QString &needle : absent)
        QVERIFY2(!html.contains(needle), qPrintable("unexpected: " + needle + "\nin:\n" + html));
}

void TestCore::linkCases_data() {
    QTest::addColumn<QString>("href");
    QTest::addColumn<QString>("doc");
    QTest::addColumn<QString>("external"); // null -> empty QString
    QTest::addColumn<QString>("resolved"); // null -> empty QString

    const QJsonObject data = loadFixture("link_cases.json");
    const QString doc = data.value("doc").toString();
    const QJsonArray cases = data.value("cases").toArray();
    for (const QJsonValue &v : cases) {
        const QJsonObject c = v.toObject();
        QTest::newRow(c.value("href").toString().isEmpty()
                          ? "<empty>"
                          : c.value("href").toString().toUtf8().constData())
            << c.value("href").toString() << doc
            << (c.value("external").isNull() ? QString() : c.value("external").toString())
            << (c.value("resolved").isNull() ? QString() : c.value("resolved").toString());
    }
}

// Separators as the fixture writes them.
//
// The fixture is language- *and* platform-neutral, so it spells paths the POSIX
// way. documentRelativePath deliberately does not: std::filesystem hands back
// what the host OS wants, which on Windows means backslashes, because that is
// what gets passed on to the file APIs. The contract being pinned here is which
// file a link resolves to, not how the separator is spelled, so the separator is
// normalised before comparing - the same tolerance the render cases use for
// engine-specific HTML.
static QString posix(const QString &path) {
    return QString(path).replace(QDir::separator(), QLatin1Char('/'));
}

void TestCore::linkCases() {
    QFETCH(QString, href);
    QFETCH(QString, doc);
    QFETCH(QString, external);
    QFETCH(QString, resolved);

    QCOMPARE(links::externalUrl(href).value_or(QString()), external);

    // resolved is only asserted for non-external hrefs (matches the fixture).
    if (external.isEmpty())
        QCOMPARE(posix(links::documentRelativePath(href, doc).value_or(QString())), resolved);
}

// The window-title convention, checked for both platforms from either one.
// macOS puts the application's name in the menu bar, so a title repeating it is
// a Windows convention in the wrong place. titleFor takes the convention as an
// argument rather than reading the platform, which is what lets this run
// anywhere - the rule is shared by all three ports and written down in
// shared/spec/SPEC.md.
void TestCore::titleCases() {
    QCOMPARE(titles::forDocument("index.md", true), QStringLiteral("index.md"));
    QVERIFY(titles::forDocument("index.md", false).startsWith("Marklens C++ "));

    // An empty title bar would be worse than a redundant one.
    QCOMPARE(titles::forDocument(QString(), true), QStringLiteral("Marklens C++"));
    QVERIFY(titles::forDocument(QString(), false).startsWith("Marklens C++ "));

    // The #if is outside the macro deliberately: a preprocessor directive
    // inside a macro argument list is undefined, and MSVC rejects it outright
    // ("'#': invalid character") while GCC and Clang let it pass.
#if defined(Q_OS_MACOS)
    QCOMPARE(titles::kDocumentOnly, true);
#else
    QCOMPARE(titles::kDocumentOnly, false);
#endif
}

// The settings file is a contract between the three ports, not an
// implementation detail of this one: one spelling for paths, case-insensitive
// de-duplication on Windows, a cap of ten, and keys another port owns left
// alone. MARKLENS_SETTINGS keeps the real file out of it - without the override
// this test would eat the recent list of whichever port ran last.
void TestCore::settingsCases() {
    // Before the override: the derived path is the assertion that all three
    // ports open the same file. It fails silently otherwise - two files, two
    // recent lists, no error - which is exactly what a wrong guess about
    // AppData/Roaming produced while this was being written.
    {
        qunsetenv("MARKLENS_SETTINGS");
        QString root;
#if defined(Q_OS_WIN)
        root = qEnvironmentVariable("LOCALAPPDATA");
#elif defined(Q_OS_MACOS)
        root = QDir::homePath() + "/Library/Preferences";
#else
        root = qEnvironmentVariable("XDG_CONFIG_HOME");
        if (root.isEmpty())
            root = QDir::homePath() + "/.config";
#endif
        const QString expected = QDir::fromNativeSeparators(root) + "/Marklens/settings.json";
        const QString actual = QDir::fromNativeSeparators(settings::filePath());
#if defined(Q_OS_WIN)
        // Windows disagrees with itself about case between an environment
        // variable and QStandardPaths; elsewhere the comparison is exact.
        QCOMPARE(actual.toLower(), expected.toLower());
#else
        QCOMPARE(actual, expected);
#endif
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    qputenv("MARKLENS_SETTINGS", (dir.path() + "/settings.json").toUtf8());

    QVERIFY(settings::recentFiles().isEmpty());
    QVERIFY(settings::lastOpenDir().isEmpty());
    QCOMPARE(settings::toolBarStyle(7), 7);

    // Newest first, and the same document twice is one entry.
    settings::addRecent("/tmp/a.md");
    settings::addRecent("/tmp/b.md");
    settings::addRecent("/tmp/a.md");
    const QStringList expected{"/tmp/a.md", "/tmp/b.md"};
    QCOMPARE(settings::recentFiles(), expected);

    // Stored with forward slashes whatever the caller hands over.
    settings::addRecent(QDir::toNativeSeparators("/tmp/c.md"));
    // 0x5C rather than a literal: the assertion is that no backslash
    // survives, and writing one here is how it would sneak back in.
    QVERIFY(!settings::recentFiles().first().contains(QChar(0x5C)));

    // Capped.
    for (int i = 0; i < 15; ++i)
        settings::addRecent(QStringLiteral("/tmp/n%1.md").arg(i));
    QCOMPARE(settings::recentFiles().size(), 10);
    QCOMPARE(settings::recentFiles().first(), QStringLiteral("/tmp/n14.md"));

    settings::setLastOpenDir("/tmp/docs");
    QCOMPARE(settings::lastOpenDir(), QStringLiteral("/tmp/docs"));
    settings::setToolBarStyle(2);
    QCOMPARE(settings::toolBarStyle(7), 2);

    // A key this port does not own survives a write from this port.
    {
        QFile f(settings::filePath());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject data = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        data.insert("somethingElse", 42);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(data).toJson());
        f.close();
    }
    settings::addRecent("/tmp/after.md");
    QFile f(settings::filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject data = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(data.value("somethingElse").toInt(), 42);
    QCOMPARE(data.value("toolBarStyle").toInt(), 2);

    qunsetenv("MARKLENS_SETTINGS");
}

QTEST_MAIN(TestCore)
#include "test_core.moc"
