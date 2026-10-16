// The shared find contract, driven through the real find bar.
//
// shared/spec/fixtures/find_cases.json says what each search should find in
// find_doc.md. This runs every case the way a reader would - typing in the
// field and ticking the boxes - so it covers the bar's wiring as well as the
// engine underneath it. The Python port runs the same file.
//
// Run with QT_QPA_PLATFORM=offscreen.

#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWebEngineView>

#include <cstdio>
#include <cstdlib>
#include <functional>

#ifndef MARKLENS_SHARED_DIR
#error "MARKLENS_SHARED_DIR must be defined by the build"
#endif

namespace {

QStringList failures;

void fail(const QString &why) {
    failures << why;
}

void finish() {
    for (const QString &problem : failures)
        std::printf("  FAIL %s\n", problem.toUtf8().constData());
    std::printf("FIND CASES: %s\n", failures.isEmpty() ? "PASS" : "FAIL");
    std::exit(failures.isEmpty() ? 0 : 1);
}

// The find bar's widgets are private to MainWindow, so the test reaches them
// the way a user does - by what they are, not by name.
QToolBar *barNamed(QWidget *window, const QString &title) {
    for (auto *bar : window->findChildren<QToolBar *>())
        if (bar->windowTitle() == title)
            return bar;
    return nullptr;
}

QCheckBox *boxNamed(QWidget *panel, const QString &text) {
    for (auto *box : panel->findChildren<QCheckBox *>())
        if (box->text() == text)
            return box;
    return nullptr;
}

QLabel *countLabel(QToolBar *bar) {
    // Two labels sit in the bar: the magnifier glyph and the match count.
    // Only the glyph carries a pixmap.
    for (auto *label : bar->findChildren<QLabel *>())
        if (label->pixmap().isNull())
            return label;
    return nullptr;
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    const QString fixtures = QStringLiteral(MARKLENS_SHARED_DIR) + "/spec/fixtures/";
    QFile file(fixtures + "find_cases.json");
    if (!file.open(QIODevice::ReadOnly)) {
        std::printf("FIND CASES: FAIL (no find_cases.json)\n");
        return 1;
    }
    const QJsonObject spec = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray cases = spec.value("cases").toArray();

    MainWindow window;
    window.resize(900, 720);
    window.show();
    window.openPath(fixtures + spec.value("doc").toString());

    QToolBar *bar = barNamed(&window, "Find");
    QToolBar *panel = barNamed(&window, "Find options");
    if (!bar || !panel) {
        std::printf("FIND CASES: FAIL (no find bar or no options panel)\n");
        return 1;
    }
    auto *input = bar->findChild<QLineEdit *>();
    auto *count = countLabel(bar);
    auto *results = panel->findChild<QListWidget *>();
    auto *caseBox = boxNamed(panel, "Case sensitive");
    auto *wordBox = boxNamed(panel, "Whole words only");
    auto *regexBox = boxNamed(panel, "Regular expression");
    auto *wrapBox = boxNamed(panel, "Wrap around");

    // The disclosure button is the only checkable one in the bar.
    QToolButton *options = nullptr;
    for (auto *button : bar->findChildren<QToolButton *>())
        if (button->isCheckable())
            options = button;

    // Stepping is driven by pressing Next, rather than by a seam added for
    // the test: the button is what a reader uses, and it is what the bar
    // wires up.
    QPushButton *next = nullptr;
    for (auto *button : bar->findChildren<QPushButton *>())
        if (button->toolTip() == QLatin1String("Next match"))
            next = button;

    if (!input || !count || !results || !caseBox || !wordBox || !regexBox || !wrapBox
        || !options || !next) {
        std::printf("FIND CASES: FAIL (the find bar is missing a widget)\n");
        return 1;
    }

    bar->show(); // the bar is hidden until asked for

    // What the count label says, read back as a number.
    auto reported = [count](bool *isError) {
        const QString shown = count->text();
        *isError = shown == QLatin1String("Bad pattern");
        if (shown.isEmpty() || *isError || shown == QLatin1String("No matches"))
            return 0;
        return shown.section(" of ", 1).remove('+').toInt();
    };

    int at = -1;
    std::function<void()> nextCase;
    std::function<void()> panelChecks;

    auto check = [&] {
        const QJsonObject c = cases.at(at).toObject();
        bool isError = false;
        const int got = reported(&isError);
        const int want = c.value("count").toInt();
        const bool wantError = c.value("error").toBool();
        if (got != want || isError != wantError) {
            fail(QStringLiteral("%1: wanted %2%3, got %4%5")
                     .arg(c.value("why").toString())
                     .arg(want)
                     .arg(wantError ? " +error" : "")
                     .arg(got)
                     .arg(isError ? " +error" : ""));
        }
        nextCase();
    };

    nextCase = [&] {
        if (++at >= cases.size()) {
            panelChecks();
            return;
        }
        const QJsonObject c = cases.at(at).toObject();
        caseBox->setChecked(c.value("caseSensitive").toBool());
        regexBox->setChecked(c.value("regex").toBool());
        // Whole words is disabled while a regex is in force, and a disabled
        // box cannot answer for the query, so the regex cases say so.
        wordBox->setChecked(c.value("wholeWord").toBool());
        input->setText(c.value("phrase").toString());
        // Everything is synchronous in the page, but the result comes back
        // through runJavaScript, which is not.
        QTimer::singleShot(250, check);
    };

    // -- the panel: the options and the list are the point of all this -------

    panelChecks = [&] {
        caseBox->setChecked(false);
        regexBox->setChecked(false);
        wordBox->setChecked(false);
        wrapBox->setChecked(true);
        options->setChecked(true); // opens the panel
        input->setText("needle");

        QTimer::singleShot(400, [&] {
            if (results->count() != 4)
                fail(QStringLiteral("results list: wanted 4 rows, got %1").arg(results->count()));
            else if (!results->item(0)->text().contains("needle"))
                fail(QStringLiteral("results list: row 0 does not show the match (%1)")
                         .arg(results->item(0)->text()));
            if (count->text() != QLatin1String("1 of 4"))
                fail(QStringLiteral("panel: count reads '%1', wanted '1 of 4'").arg(count->text()));

            // A regex owns its own word boundaries, so the box is refused
            // rather than silently wrapping somebody's pattern.
            regexBox->setChecked(true);
            if (wordBox->isEnabled())
                fail("whole words stayed enabled while a regex was in force");
            regexBox->setChecked(false);

            wrapBox->setChecked(false);
            QTimer::singleShot(300, [&] {
                for (int i = 0; i < 6; ++i) // more steps than there are matches
                    next->click();
                QTimer::singleShot(400, [&] {
                    if (count->text() != QLatin1String("4 of 4"))
                        fail(QStringLiteral("wrap off: stopped at '%1', wanted '4 of 4'")
                                 .arg(count->text()));
                    wrapBox->setChecked(true);
                    next->click();
                    QTimer::singleShot(400, [&] {
                        if (count->text() != QLatin1String("1 of 4"))
                            fail(QStringLiteral("wrap on: went to '%1', wanted '1 of 4'")
                                     .arg(count->text()));
                        results->setCurrentRow(2);
                        QTimer::singleShot(400, [&] {
                            if (count->text() != QLatin1String("3 of 4"))
                                fail(QStringLiteral("clicking row 2 gave '%1', wanted '3 of 4'")
                                         .arg(count->text()));
                            finish();
                        });
                    });
                });
            });
        });
    };

    bool started = false;
    QObject::connect(window.webView(), &QWebEngineView::loadFinished, [&](bool ok) {
        if (!ok || started)
            return;
        started = true;
        // mermaid and highlight.js rewrite the document; search what the
        // reader would actually be looking at.
        QTimer::singleShot(1200, nextCase);
    });

    QTimer::singleShot(60000, [] {
        std::printf("FIND CASES: FAIL (timed out)\n");
        std::exit(1);
    });
    return app.exec();
}
