#pragma once
#include <QMainWindow>
#include <QStringList>

class QWebEngineView;
class MarkdownPage;
class QFileSystemWatcher;
class QLineEdit;
class QAction;
class QMenu;
class QLabel;
class QToolBar;
class QToolButton;

// A window with a web view, a toolbar, and a file watcher. Off-sandbox the
// page's base URL is the document's folder, so Qt resolves relative images and
// links natively; we only intercept navigations to route them.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

public slots:
    void openPath(const QString &path, bool recordHistory = true,
                  const QString &fragment = QString());

public:
    // Reopen the document last looked at. False when the recent list is empty
    // or nothing in it still exists, leaving the empty state up.
    bool openMostRecent();

    bool hasDocument() const { return !m_current.isEmpty(); }


    // Test seams: let the nav test click links and observe loads on the real
    // app wiring (the queued openDocument connection lives in the ctor), and
    // ask which document ended up open. The window title cannot answer that -
    // it names the application, not the document.
    QWebEngineView *webView() const { return m_view; }
    QString currentDocument() const { return m_current; }

private:
    void buildUi(); // menu bar + toolbar, sharing the same actions
    void showHelp();
    void showAbout();
    void render();
    void reload();
    void goBack();
    void watch(const QString &path);
    void onFileChanged(const QString &changed);

    void openDialog();
    void addRecent(const QString &path);
    void rebuildRecentMenu();
    void clearRecent();
    void toggleZoom();
    void zoom(int direction);
    void zoomReset();
    void buildFindBar();
    void toggleFind();
    void hideFind();
    void findText(bool backward);
    void setStale(bool stale);
    void buildPathMenu();
    void updateDocButton();
    void showToolBarMenu(const QPoint &pos);
    void showContextMenu(const QPoint &pos);
    QMenu *buildContextMenu();
    void setToolBarStyle(Qt::ToolButtonStyle style, bool remember = true);
    void exportPdf();
    void reveal();

    QWebEngineView *m_view;
    MarkdownPage *m_page;
    QFileSystemWatcher *m_watcher;
    QLineEdit *m_find;
    QAction *m_back;
    QAction *m_reload;
    QMenu *m_recentMenu;
    QToolBar *m_toolBar;
    QToolBar *m_findBar;
    QToolButton *m_docButton;
    QMenu *m_pathMenu;
    QLabel *m_findCount;
    bool m_stale = false;

    QStringList m_history;
    QString m_current;
    // Held until loadFinished: the document is rendered with setHtml, so
    // there is nothing to scroll to until the new page exists.
    QString m_pendingFragment;
    bool m_autoReload = true;
};
