// Regression test for the find bar losing its place across a re-render.
//
// Auto-reload re-renders the document on every save of the file being read.
// The page that the search ran against is then gone: the highlights go with
// it and the count label still shows a number for a page that no longer
// exists, so the arrows and Return look like they have stopped responding.
// This asserts the count comes back by itself once the new page has loaded.
// Run with QT_QPA_PLATFORM=offscreen.

#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QWebEngineView>

#include <cstdio>
#include <cstdlib>

namespace {

void fail(const QString &why) {
    std::printf("FIND: FAIL (%s)\n", why.toUtf8().constData());
    std::exit(1);
}

// The find bar's widgets are private to MainWindow, so the test reaches them
// the way a user does - by what they are, not by name.
QToolBar *findBar(QWidget *window) {
    for (auto *tb : window->findChildren<QToolBar *>())
        if (tb->windowTitle() == "Find")
            return tb;
    return nullptr;
}

QLabel *countLabel(QToolBar *bar) {
    // Two labels sit in the bar: the magnifier glyph and the match count.
    // Only the glyph carries a pixmap.
    for (auto *l : bar->findChildren<QLabel *>())
        if (l->pixmap().isNull())
            return l;
    return nullptr;
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QTemporaryDir dir;
    const QString doc = dir.filePath("index.md");
    QFile f(doc);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        fail("could not write the document");
    f.write("# Haystack\n\nneedle one\n\nneedle two\n\nneedle three\n");
    f.close();

    MainWindow window;
    window.resize(900, 720);
    window.show();
    window.openPath(doc);

    QToolBar *bar = findBar(&window);
    if (!bar)
        fail("no find bar");
    QLineEdit *input = bar->findChild<QLineEdit *>();
    QLabel *count = countLabel(bar);
    if (!input || !count)
        fail("find bar has no input or no count label");

    int loads = 0;
    QObject::connect(window.webView(), &QWebEngineView::loadFinished, [&](bool ok) {
        if (!ok)
            return;
        if (++loads == 1) {
            bar->show();
            input->setText("needle"); // textChanged runs the search
            QTimer::singleShot(800, [&] {
                const QString before = count->text();
                if (before != "1 of 3")
                    fail(QString("search found '%1', expected '1 of 3'").arg(before));
                // Blank it so only the window itself can put a count back,
                // then re-render - the same signal auto-reload arrives on.
                count->clear();
                window.openPath(doc, false);
            });
            return;
        }
        QTimer::singleShot(800, [&] {
            const QString after = count->text();
            const bool passed = after == "1 of 3";
            std::printf("FIND: %s (after the re-render the count read '%s')\n",
                        passed ? "PASS" : "FAIL", after.toUtf8().constData());
            std::exit(passed ? 0 : 1);
        });
    });

    QTimer::singleShot(25000, [] {
        std::printf("FIND: FAIL (timed out)\n");
        std::exit(1);
    });
    return app.exec();
}
