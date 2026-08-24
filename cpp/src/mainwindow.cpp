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
#include <QIcon>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSet>
#include <QSettings>
#include <QTextBrowser>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineFindTextResult>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {
// Shown when no document is open. The ports are ordinary single-window apps and
// can be launched with no file, which the SwiftUI original never could - it was
// document-based, so macOS put up the open panel instead. See shared/spec.
constexpr auto kEmptyStateBody =
    "<p style='opacity:.6;padding:1rem'>Open a Markdown file to view it.</p>";

#if defined(Q_OS_MACOS)
constexpr auto kRevealText = "Show in Finder";
#elif defined(Q_OS_WIN)
constexpr auto kRevealText = "Show in Explorer";
#else
constexpr auto kRevealText = "Show in File Manager";
#endif

// Toolbar glyphs, from the set shared with the other two ports so all three
// draw the same icons. They are stroke="currentColor", so they follow the
// palette and one set covers light and dark. See shared/icons/ICONS.md.
QIcon icon(const QString &name) {
    return QIcon(assets::iconsDir() + "/" + name + ".svg");
}
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
    m_reload = make("Reload", &MainWindow::reload, QKeySequence::Refresh);
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

    // Find shows the find bar (created below) and puts the caret in it.
    auto *findAct = make("Find…", &MainWindow::toggleFind, QKeySequence::Find);
    auto *findNextAct = make("Find Next", [this] { findText(false); }, QKeySequence::FindNext);
    auto *findPrevAct =
        make("Find Previous", [this] { findText(true); }, QKeySequence::FindPrevious);

    auto *helpAct = make("Marklens Help", &MainWindow::showHelp, QKeySequence::HelpContents);
    auto *aboutAct = make("About Marklens", &MainWindow::showAbout);
    aboutAct->setMenuRole(QAction::AboutRole); // → application menu on macOS

    // --- menu bar ---
    auto *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(openAct);
    m_recentMenu = fileMenu->addMenu("Open Recent");
    rebuildRecentMenu();
    fileMenu->addAction(m_reload);
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

    // --- toolbar ---
    // Icons rather than labels, in the Swift app's order, pushed to the right
    // as its toolbar items are. The document name is not repeated here: Swift
    // shows it as the title bar's proxy icon, which Qt 6 cannot reproduce
    // (setUnifiedTitleAndToolBarOnMac is gone), and the window title already
    // carries it.
    //
    // Open is deliberately absent, as it is in the Swift toolbar - a document
    // app opens documents through File ▸ Open and the Open Recent menu.
    // Back is deliberately present, though that toolbar has none: Swift opens a
    // link in a new window, while all three ports here replace the document in
    // place and so need a way back. Its glyph is the one Swift uses for exactly
    // that button on iOS, where the same thing happens.
    auto *tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setIconSize(QSize(18, 18));

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    m_back->setIcon(icon("back"));
    findAct->setIcon(icon("find"));
    zoomOutAct->setIcon(icon("zoom-out"));
    zoomInAct->setIcon(icon("zoom-in"));
    zoomResetAct->setIcon(icon("actual-size"));
    pdfAct->setIcon(icon("export"));
    revealAct->setIcon(icon("reveal"));
    m_reload->setIcon(icon("reload"));

    tb->addAction(m_back);
    tb->addAction(findAct);
    tb->addAction(zoomOutAct);
    tb->addAction(zoomInAct);
    tb->addAction(zoomResetAct);
    tb->addAction(pdfAct);
    tb->addAction(revealAct);
    tb->addAction(m_reload);

    buildFindBar();

    render(); // nothing open yet, so this puts up the empty state
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
    // "0.1.0 (4)" - base version, then the build number, which is the git
    // commit count baked in at configure time. See CMakeLists.txt.
    const QString version = QStringLiteral(MARKLENS_BUILD).isEmpty()
                                ? QStringLiteral(MARKLENS_VERSION)
                                : QStringLiteral(MARKLENS_VERSION " (" MARKLENS_BUILD ")");
    QMessageBox::about(
        this, "About Marklens",
        QStringLiteral("<h3>Marklens</h3>"
                       "<p>A native Markdown viewer, C++/Qt port.</p>"
                       "<p>Version %1<br>Qt %2</p>"
                       "<p><a href='%3'>%3</a></p>"
                       "<p>One of three ports of the same viewer -- Python/PySide6, "
                       "C++/Qt and Rust/Tauri -- kept behaviourally identical by a "
                       "shared specification.</p>"
                       "<p>A reimplementation of <a href='%4'>Marklens</a> by "
                       "Donald Jackson.</p>"
                       "<p>Licensed under the MIT License.</p>")
            .arg(version, QStringLiteral(QT_VERSION_STR),
                 QStringLiteral("https://github.com/dwsdolce/marklens-ports"),
                 QStringLiteral("https://github.com/donald-jackson/marklens")));
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

// Reopen the document last looked at, which is what the Swift app does and
// what the recent list is already there to remember. The list outlives the
// files in it - renamed, deleted, on a volume that is not mounted - so it is
// walked until something opens rather than trusting the first entry.
bool MainWindow::openMostRecent() {
    for (const QString &path : loadRecent()) {
        if (QFileInfo::exists(path)) {
            openPath(path);
            return true;
        }
    }
    return false;
}

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
    if (m_current.isEmpty()) {
        m_view->setHtml(renderer::page(kEmptyStateBody, assets::assetBaseUrl()));
        setWindowTitle("Marklens");
        return;
    }
    QFile f(m_current);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    const QString text = QString::fromUtf8(f.readAll());
    const QString html = renderer::page(renderer::renderBody(text), assets::assetBaseUrl());
    const QUrl base = QUrl::fromLocalFile(QFileInfo(m_current).absolutePath() + "/");
    m_view->setHtml(html, base);
    setWindowTitle(QFileInfo(m_current).fileName() + " — Marklens");
    setStale(false); // whatever changed on disk is now on screen
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
    if (changed != m_current)
        return;
    if (m_autoReload)
        render();
    else
        setStale(true); // badge the reload glyph; the user decides when
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

// A bar of its own below the toolbar, rather than a field inside it. That is
// where the Swift app puts it, and it leaves room for the match count and the
// previous/next/close buttons that a toolbar field had nowhere to show.
void MainWindow::buildFindBar() {
    m_findBar = new QToolBar("Find", this);
    m_findBar->setMovable(false);
    addToolBarBreak();
    addToolBar(m_findBar);

    auto *glyph = new QLabel(this);
    glyph->setPixmap(icon("find").pixmap(16, 16));
    glyph->setContentsMargins(6, 0, 2, 0);
    m_findBar->addWidget(glyph);

    m_find = new QLineEdit(this);
    m_find->setPlaceholderText("Find");
    m_find->setClearButtonEnabled(true);
    m_find->setMaximumWidth(240);
    // Searching as you type, as the Swift bar does; Return steps to the next.
    connect(m_find, &QLineEdit::textChanged, this, [this] { findText(false); });
    connect(m_find, &QLineEdit::returnPressed, this, [this] { findText(false); });
    m_findBar->addWidget(m_find);

    m_findCount = new QLabel(this);
    m_findCount->setEnabled(false); // reads as secondary text in every style
    m_findCount->setContentsMargins(6, 0, 6, 0);
    m_findBar->addWidget(m_findCount);

    auto button = [this](const QString &name, const QString &tip, auto slot) {
        auto *b = new QPushButton(icon(name), {}, this);
        b->setFlat(true);
        b->setToolTip(tip);
        b->setFixedWidth(28);
        connect(b, &QPushButton::clicked, this, slot);
        m_findBar->addWidget(b);
    };
    button("find-prev", "Previous match", [this] { findText(true); });
    button("find-next", "Next match", [this] { findText(false); });
    button("close", "Close find bar", [this] { hideFind(); });

    m_findBar->hide();

    // Escape closes it, as in the Swift bar.
    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this] {
        if (m_findBar->isVisible())
            hideFind();
    });
}

void MainWindow::toggleFind() {
    // Cmd+F on an open bar that already has the caret means "put it away";
    // on an open bar that does not, it means "come back to it".
    if (m_findBar->isVisible() && m_find->hasFocus()) {
        hideFind();
        return;
    }
    m_findBar->show();
    m_find->setFocus();
    m_find->selectAll();
}

void MainWindow::hideFind() {
    m_findBar->hide();
    m_view->findText({}); // drops the highlight
    m_findCount->clear();
    m_view->setFocus();
}

void MainWindow::findText(bool backward) {
    const QString needle = m_find->text();
    if (needle.isEmpty()) {
        m_view->findText({});
        m_findCount->clear();
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (backward)
        flags |= QWebEnginePage::FindBackward;
    // The count comes back asynchronously, so the label is filled in from the
    // callback rather than alongside the search.
    m_view->findText(needle, flags, [this](const QWebEngineFindTextResult &r) {
        m_findCount->setText(r.numberOfMatches()
                                 ? QString("%1 of %2").arg(r.activeMatch()).arg(r.numberOfMatches())
                                 : QStringLiteral("No matches"));
    });
}

// The Swift app fills in its reload glyph when the file has changed underneath
// and auto-reload is off, so there is something to notice before acting on it.
void MainWindow::setStale(bool stale) {
    if (m_stale == stale)
        return;
    m_stale = stale;
    m_reload->setIcon(icon(stale ? "reload-alert" : "reload"));
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
