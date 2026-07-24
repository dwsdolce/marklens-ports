// Regression test for the link-navigation trace trap. Drives the REAL
// MainWindow (its queued openDocument connection), clicks the sample's relative
// link, and verifies the viewer navigated to the target instead of trapping.
// If the connection ever regresses to direct, this crashes (SIGTRAP) and fails.
// Run with QT_QPA_PLATFORM=offscreen.

#include "mainwindow.h"

#include <QApplication>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineView>

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    const QString sample = QStringLiteral(MARKLENS_SHARED_DIR) + "/spec/sample/index.md";

    MainWindow window;
    window.resize(900, 720);
    window.show();
    window.openPath(sample);

    int loads = 0;
    QObject::connect(window.webView(), &QWebEngineView::loadFinished, [&](bool) {
        if (++loads == 1) {
            // First document up — click the relative link to OTHER.md.
            QTimer::singleShot(500, [&] {
                window.webView()->page()->runJavaScript(
                    "var a=document.querySelector('a[href=\\'OTHER.md\\']'); a && a.click();");
            });
        } else {
            // Navigated without trapping — confirm we're on the target doc.
            const bool ok = window.windowTitle().contains("OTHER");
            std::printf("NAV: %s (title: %s)\n", ok ? "PASS" : "FAIL",
                        window.windowTitle().toUtf8().constData());
            std::exit(ok ? 0 : 1);
        }
    });

    QTimer::singleShot(12000, [] {
        std::printf("NAV: FAIL (timeout — no navigation)\n");
        std::exit(3);
    });
    return app.exec();
}
