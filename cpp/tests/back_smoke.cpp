// Regression test for Back after an in-page anchor. History used to hold
// documents only, so following "#section" and pressing Back reopened the
// PREVIOUS DOCUMENT instead of returning to where the link was read. Drives the
// real MainWindow: scroll, click an anchor, press Back, read the offset back.
// Run with QT_QPA_PLATFORM=offscreen.

#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <cstdio>
#include <cstdlib>

namespace {

MainWindow *window = nullptr;
double before = 0;
double afterJump = 0;

void js(const QString &code, const std::function<void(const QVariant &)> &then) {
    window->webView()->page()->runJavaScript(code, then);
}

void fail(const char *why) {
    std::printf("BACK: FAIL (%s)\n", why);
    std::exit(1);
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QTemporaryDir dir;
    // The link sits well down the document and its target further still, so a
    // successful jump and a successful return are both unambiguous.
    QString filler;
    for (int i = 0; i < 120; ++i)
        filler += "filler\n\n";
    QString tail;
    for (int i = 0; i < 200; ++i)
        tail += "filler\n\n";
    QFile f(dir.filePath("doc.md"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        fail("could not write doc.md");
    f.write(("# Top\n\n" + filler + "[jump](#the-target)\n\n" + tail + "## The Target\n\ncontent\n")
                .toUtf8());
    f.close();

    MainWindow w;
    window = &w;
    w.resize(900, 720);
    w.show();
    w.openPath(dir.filePath("doc.md"));

    QObject::connect(w.webView(), &QWebEngineView::loadFinished, [&](bool ok) {
        if (!ok)
            fail("the document did not load");
        QTimer::singleShot(600, [] {
            js("document.scrollingElement.scrollTop = 3000;"
               " document.scrollingElement.scrollTop",
               [](const QVariant &v) {
                   before = v.toDouble();
                   js("document.querySelector('a[href=\"#the-target\"]').click(); 1",
                      [](const QVariant &) {
                          QTimer::singleShot(600, [] {
                              js("document.scrollingElement.scrollTop", [](const QVariant &v) {
                                  afterJump = v.toDouble();
                                  if (afterJump <= before)
                                      fail("the anchor did not scroll anywhere");
                                  window->goBack();
                                  QTimer::singleShot(700, [] {
                                      js("document.scrollingElement.scrollTop",
                                         [](const QVariant &v) {
                                             const double back = v.toDouble();
                                             const bool ok = qAbs(back - before) < 5;
                                             std::printf("BACK: %s (before %.0f, jump %.0f, "
                                                         "back %.0f)\n",
                                                         ok ? "PASS" : "FAIL", before, afterJump,
                                                         back);
                                             std::exit(ok ? 0 : 1);
                                         });
                                  });
                              });
                          });
                      });
               });
        });
    });

    QTimer::singleShot(25000, [] { fail("timed out"); });
    return app.exec();
}
