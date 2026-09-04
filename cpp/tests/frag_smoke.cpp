// Regression test for cross-file #fragments. A link like "setup.md#shells" has
// to open the other document AND land on the heading; links::documentRelativePath
// deliberately drops the fragment (the shared link contract pins that), so the
// fragment travels separately and is applied once the new page has rendered.
// Drives the REAL MainWindow and reads the scroll position back out of the view.
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

void writeFile(const QString &path, const QString &text) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        std::printf("FRAGMENT: FAIL (could not write %s)\n", path.toUtf8().constData());
        std::exit(1);
    }
    f.write(text.toUtf8());
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QTemporaryDir dir;
    // The heading has to sit far enough down that reaching it MUST scroll,
    // otherwise a passing scroll position would prove nothing.
    QString filler;
    for (int i = 0; i < 200; ++i)
        filler += "filler paragraph\n\n";
    writeFile(dir.filePath("target.md"), "# Top\n\n" + filler + "## Windows Shells\n\ncontent\n");
    writeFile(dir.filePath("source.md"), "[go](target.md#windows-shells)\n");

    MainWindow window;
    window.resize(900, 720);
    window.show();
    window.openPath(dir.filePath("source.md"));

    int loads = 0;
    QObject::connect(window.webView(), &QWebEngineView::loadFinished, [&](bool) {
        if (++loads == 1) {
            QTimer::singleShot(500, [&] {
                window.webView()->page()->runJavaScript("document.querySelector('a').click();");
            });
            return;
        }
        // The scroll is queued behind this signal, so give it a turn first.
        QTimer::singleShot(900, [&] {
            window.webView()->page()->runJavaScript(
                "JSON.stringify({scrolled: document.scrollingElement.scrollTop > 0,"
                " target: !!document.getElementById('windows-shells')})",
                [&](const QVariant &v) {
                    const QString r = v.toString();
                    const bool ok = r.contains("\"target\":true") && r.contains("\"scrolled\":true");
                    std::printf("FRAGMENT: %s (%s, document: %s)\n", ok ? "PASS" : "FAIL",
                                r.toUtf8().constData(),
                                window.currentDocument().toUtf8().constData());
                    std::exit(ok ? 0 : 1);
                });
        });
    });

    QTimer::singleShot(25000, [] {
        std::printf("FRAGMENT: FAIL (timed out)\n");
        std::exit(1);
    });
    return app.exec();
}
