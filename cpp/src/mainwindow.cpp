#include "mainwindow.h"

#include "assets.h"
#include "page.h"
#include "renderer.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QTextBrowser>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {
#if defined(Q_OS_MACOS)
constexpr auto kRevealText = "Show in Finder";
#elif defined(Q_OS_WIN)
constexpr auto kRevealText = "Show in Explorer";
#else
constexpr auto kRevealText = "Show in File Manager";
#endif
} // namespace

MainWindow::MainWindow() {
    setWindowTitle("Marklens");
    resize(900, 720);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);

    m_view = new QWebEngineView(this);
    m_page = new MarkdownPage(m_view);
    // Queued, NOT direct: openDocument fires from inside the page's
    // acceptNavigationRequest, and openPath calls setHtml. Re-entering
    // QtWebEngine's navigation machinery synchronously traps (SIGTRAP), so
    // defer the load until the current navigation callback has returned.
    connect(m_page, &MarkdownPage::openDocument, this,
            [this](const QString &p) { openPath(p); }, Qt::QueuedConnection);
    m_view->setPage(m_page);
    // Assets are referenced by absolute file:// URLs from the doc-folder base;
    // allow local content to load them.
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    setCentralWidget(m_view);

    buildUi();
}

void MainWindow::buildUi() {
    // One QAction per command, shared between the menu bar (filled out on
    // macOS, unlike a toolbar-only app) and the toolbar.
    auto make = [&](const QString &text, auto slot, const QKeySequence &sc = {}) {
        auto *a = new QAction(text, this);
        connect(a, &QAction::triggered, this, slot);
        if (!sc.isEmpty())
            a->setShortcut(sc);
        return a;
    };

    auto *openAct = make("Open…", &MainWindow::openDialog, QKeySequence::Open);
    auto *reloadAct = make("Reload", &MainWindow::reload, QKeySequence::Refresh);
    auto *pdfAct = make("Export as PDF…", &MainWindow::exportPdf, QKeySequence("Ctrl+Shift+E"));
    auto *revealAct = make(kRevealText, &MainWindow::reveal);
    auto *closeAct = make("Close", [this] { close(); }, QKeySequence::Close);

    // Reload the view automatically when the file changes on disk (on by
    // default). Matches the Swift app's File ▸ Auto-Reload on Change.
    auto *autoReloadAct = new QAction("Auto-Reload on Change", this);
    autoReloadAct->setCheckable(true);
    autoReloadAct->setChecked(m_autoReload);
    connect(autoReloadAct, &QAction::toggled, this, [this](bool on) { m_autoReload = on; });

    m_back = make("Back", &MainWindow::goBack, QKeySequence("Ctrl+["));
    m_back->setEnabled(false);
    auto *zoomInAct = make("Zoom In", [this] { zoom(+1); }, QKeySequence("Ctrl+="));
    auto *zoomOutAct = make("Zoom Out", [this] { zoom(-1); }, QKeySequence("Ctrl+-"));
    auto *zoomResetAct = make("Actual Size", &MainWindow::zoomReset, QKeySequence("Ctrl+0"));

    // Find focuses the toolbar's search field (created below).
    auto *findAct = make(
        "Find…", [this] { m_find->setFocus(); m_find->selectAll(); }, QKeySequence::Find);
    auto *findNextAct = make("Find Next", &MainWindow::findNext, QKeySequence::FindNext);
    auto *findPrevAct = make("Find Previous", &MainWindow::findPrevious, QKeySequence::FindPrevious);

    auto *helpAct = make("Marklens Help", &MainWindow::showHelp, QKeySequence::HelpContents);
    auto *aboutAct = make("About Marklens", &MainWindow::showAbout);
    aboutAct->setMenuRole(QAction::AboutRole); // → application menu on macOS

    // --- menu bar ---
    auto *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(openAct);
    m_recentMenu = fileMenu->addMenu("Open Recent");
    rebuildRecentMenu();
    fileMenu->addAction(reloadAct);
    fileMenu->addAction(autoReloadAct);
    fileMenu->addSeparator();
    fileMenu->addAction(pdfAct);
    fileMenu->addAction(revealAct);
    fileMenu->addSeparator();
    fileMenu->addAction(closeAct);

    auto *editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction(findAct);
    editMenu->addAction(findNextAct);
    editMenu->addAction(findPrevAct);

    auto *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction(m_back);
    viewMenu->addSeparator();
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(zoomResetAct);

    auto *windowMenu = menuBar()->addMenu("Window");
    windowMenu->addAction(make("Minimize", &MainWindow::showMinimized, QKeySequence("Ctrl+M")));
    windowMenu->addAction(make("Zoom", &MainWindow::toggleZoom));

    auto *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction(helpAct);
    helpMenu->addAction(aboutAct);

    // --- toolbar (a subset, for quick access) ---
    auto *tb = addToolBar("Main");
    tb->setMovable(false);
    tb->addAction(openAct);
    tb->addAction(m_back);
    tb->addAction(reloadAct);
    tb->addSeparator();
    tb->addAction(zoomOutAct);
    tb->addAction(zoomInAct);
    tb->addAction(zoomResetAct);
    tb->addSeparator();

    m_find = new QLineEdit(this);
    m_find->setPlaceholderText("Find…");
    m_find->setMaximumWidth(200);
    connect(m_find, &QLineEdit::returnPressed, this, &MainWindow::find);
    tb->addWidget(m_find);

    tb->addSeparator();
    tb->addAction(pdfAct);
    tb->addAction(revealAct);
}

void MainWindow::showHelp() {
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Marklens Help");
    dlg->resize(580, 620);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto *browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(assets::helpHtml());

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(browser);
    dlg->show(); // modeless, like the Swift help window
}

void MainWindow::showAbout() {
    QMessageBox::about(this, "About Marklens",
                       "<b>Marklens</b> (C++/Qt port)<br>A native Markdown viewer.");
}

void MainWindow::openPath(const QString &path, bool recordHistory) {
    const QString resolved = QFileInfo(path).absoluteFilePath();
    if (recordHistory && !m_current.isEmpty() && m_current != resolved) {
        m_history.append(m_current);
        m_back->setEnabled(true);
    }
    watch(resolved);
    m_current = resolved;
    m_page->setDocumentPath(resolved);
    addRecent(resolved);
    render();
}

// --- recent files (persisted via QSettings) --------------------------------

namespace {

// This port and the Python one share a single QSettings store - same
// organisation and application name - but reach it spelling paths differently:
// Qt hands back forward slashes (QFileDialog, QUrl::toLocalFile) while Python's
// str(Path) is native, so on Windows the same document landed in the list
// twice. Forward slashes are the canonical form because that is what the rest
// of this file already works with; converting instead would change m_current
// and break the same-document fragment check in page.cpp.
QString canonicalRecent(const QString &path) {
    return QDir::fromNativeSeparators(path);
}

// Windows filenames are case-insensitive as well, so the comparison key folds
// case there and nowhere else.
QString recentKey(const QString &path) {
#if defined(Q_OS_WIN)
    return canonicalRecent(path).toLower();
#else
    return canonicalRecent(path);
#endif
}

// Read the stored list, canonicalising and de-duplicating as it goes, so a list
// written by an older build (or by the Python port) is cleaned up on sight
// rather than needing a migration step.
QStringList loadRecent() {
    QStringList out;
    QSet<QString> seen;
    for (const QString &path : QSettings().value("recentFiles").toStringList()) {
        const QString canonical = canonicalRecent(path);
        if (!seen.contains(recentKey(canonical))) {
            seen.insert(recentKey(canonical));
            out << canonical;
        }
    }
    return out;
}

} // namespace

void MainWindow::addRecent(const QString &path) {
    const QString canonical = canonicalRecent(path);
    const QString key = recentKey(canonical);

    QStringList recent;
    for (const QString &existing : loadRecent()) {
        if (recentKey(existing) != key)
            recent << existing;
    }
    recent.prepend(canonical);
    while (recent.size() > 10)
        recent.removeLast();

    QSettings().setValue("recentFiles", recent);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    m_recentMenu->clear();
    const QStringList recent = loadRecent();
    if (recent.isEmpty()) {
        m_recentMenu->addAction("No Recent Documents")->setEnabled(false);
        return;
    }
    for (const QString &path : recent) {
        QAction *a = m_recentMenu->addAction(QFileInfo(path).fileName());
        a->setToolTip(path);
        connect(a, &QAction::triggered, this, [this, path] { openPath(path); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Clear Menu", this, &MainWindow::clearRecent);
}

void MainWindow::clearRecent() {
    QSettings().remove("recentFiles");
    rebuildRecentMenu();
}

void MainWindow::toggleZoom() {
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::render() {
    if (m_current.isEmpty())
        return;
    QFile f(m_current);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(f.readAll());
    const QString html = renderer::page(renderer::renderBody(text), assets::assetBaseUrl());
    const QUrl base = QUrl::fromLocalFile(QFileInfo(m_current).absolutePath() + "/");
    m_view->setHtml(html, base);
    setWindowTitle(QFileInfo(m_current).fileName() + " — Marklens");
}

void MainWindow::reload() { render(); }

void MainWindow::goBack() {
    if (m_history.isEmpty())
        return;
    const QString previous = m_history.takeLast();
    m_back->setEnabled(!m_history.isEmpty());
    openPath(previous, /*recordHistory=*/false);
}

void MainWindow::watch(const QString &path) {
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    m_watcher->addPath(path);
}

void MainWindow::onFileChanged(const QString &changed) {
    // Atomic saves (write-temp-then-rename) drop the watch — re-add, mirroring
    // the Swift kqueue re-arm, then re-render.
    if (QFileInfo::exists(changed) && !m_watcher->files().contains(changed))
        m_watcher->addPath(changed);
    if (m_autoReload && changed == m_current)
        render();
}

void MainWindow::openDialog() {
    const QString name = QFileDialog::getOpenFileName(
        this, "Open Markdown", QString(),
        "Markdown (*.md *.markdown *.mdown *.mkd);;All files (*)");
    if (!name.isEmpty())
        openPath(name);
}

void MainWindow::zoom(int direction) {
    m_view->setZoomFactor(m_view->zoomFactor() * (direction > 0 ? 1.1 : 1.0 / 1.1));
}

void MainWindow::zoomReset() { m_view->setZoomFactor(1.0); }

void MainWindow::find() { m_view->findText(m_find->text()); }

void MainWindow::findNext() { m_view->findText(m_find->text()); }

void MainWindow::findPrevious() {
    m_view->findText(m_find->text(), QWebEnginePage::FindBackward);
}

void MainWindow::exportPdf() {
    if (m_current.isEmpty())
        return;
    const QString suggested = QFileInfo(m_current).completeBaseName() + ".pdf";
    const QString name = QFileDialog::getSaveFileName(this, "Export PDF", suggested, "PDF (*.pdf)");
    if (!name.isEmpty())
        m_page->printToPdf(name);
}

void MainWindow::reveal() {
    if (m_current.isEmpty())
        return;
#if defined(Q_OS_MACOS)
    QProcess::startDetached("open", {"-R", m_current});
#elif defined(Q_OS_WIN)
    QProcess::startDetached("explorer", {"/select," + QDir::toNativeSeparators(m_current)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_current).absolutePath()));
#endif
}
