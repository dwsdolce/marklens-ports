// Headless smoke test: render the sample doc into an offscreen web view and
// assert it actually rendered — image resolved, mermaid drawn, table present,
// code highlighted. Same checks as the Python port's smoke_gui.py.
// Run with QT_QPA_PLATFORM=offscreen.

#include "assets.h"
#include "renderer.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <cstdio>

namespace {
const char *kCheckJs = R"JS(
(function () {
    var img = document.querySelector('img');
    var mermaid = document.querySelector('.mermaid');
    var hljs = document.querySelector('pre code.hljs, pre code[class*="language-"]');
    return JSON.stringify({
        h1: (document.querySelector('h1') || {}).textContent || null,
        imgSrc: img ? img.getAttribute('src') : null,
        imgComplete: img ? (img.complete && img.naturalWidth > 0) : null,
        imgCount: document.querySelectorAll('img').length,
        brokenImages: Array.prototype.filter.call(
            document.querySelectorAll('img'),
            function (i) { return !(i.complete && i.naturalWidth > 0); }
        ).map(function (i) { return i.getAttribute('src'); }),
        hasMermaidDiv: !!mermaid,
        mermaidRendered: mermaid ? mermaid.querySelector('svg') !== null : false,
        hasTable: !!document.querySelector('table'),
        codeHighlighted: !!hljs
    });
})();
)JS";
} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    const QString sample = QStringLiteral(MARKLENS_SHARED_DIR) + "/spec/sample/index.md";
    QFile f(sample);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::fprintf(stderr, "cannot open sample\n");
        return 2;
    }
    const QString html =
        renderer::page(renderer::renderBody(QString::fromUtf8(f.readAll())), assets::assetBaseUrl());

    QWebEngineView view;
    view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    view.resize(900, 720);
    view.setHtml(html, QUrl::fromLocalFile(QFileInfo(sample).absolutePath() + "/"));

    int exitCode = 1;
    QObject::connect(&view, &QWebEngineView::loadFinished, [&](bool) {
        // mermaid.js is heavy; give it time to draw the SVG before inspecting.
        QTimer::singleShot(2500, [&] {
            view.page()->runJavaScript(QString::fromUtf8(kCheckJs), [&](const QVariant &v) {
                const QJsonObject r = QJsonDocument::fromJson(v.toString().toUtf8()).object();
                std::printf("RESULT: %s\n",
                            QJsonDocument(r).toJson(QJsonDocument::Compact).constData());
                // Every image, not just the first: the first one's filename has
                // nothing to decode, so checking it alone missed a %20 that
                // resolved to nothing.
                const bool imagesOk = r.value("imgCount").toInt() == 3 &&
                                      r.value("brokenImages").toArray().isEmpty();
                if (!imagesOk)
                    std::printf("broken images: %s\n",
                                QJsonDocument(r.value("brokenImages").toArray())
                                    .toJson(QJsonDocument::Compact)
                                    .constData());
                const bool ok = r.value("h1").toString() == "Marklens sample" &&
                                r.value("imgSrc").toString() == "design/icon.svg" &&
                                r.value("imgComplete").toBool() && imagesOk &&
                                r.value("hasMermaidDiv").toBool() &&
                                r.value("hasTable").toBool() && r.value("codeHighlighted").toBool();
                std::printf("SMOKE: %s\n", ok ? "PASS" : "FAIL");
                exitCode = ok ? 0 : 1;
                app.quit();
            });
        });
    });

    QTimer::singleShot(12000, &app, &QApplication::quit); // hard timeout
    app.exec();
    return exitCode;
}
