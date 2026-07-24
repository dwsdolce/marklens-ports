// Core tests driven by the shared, language-neutral fixtures — the same
// contract the Python port satisfies. QtTest + QJsonDocument, no extra deps.

#include "links.h"
#include "renderer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

void TestCore::linkCases() {
    QFETCH(QString, href);
    QFETCH(QString, doc);
    QFETCH(QString, external);
    QFETCH(QString, resolved);

    QCOMPARE(links::externalUrl(href).value_or(QString()), external);

    // resolved is only asserted for non-external hrefs (matches the fixture).
    if (external.isEmpty())
        QCOMPARE(links::documentRelativePath(href, doc).value_or(QString()), resolved);
}

QTEST_MAIN(TestCore)
#include "test_core.moc"
