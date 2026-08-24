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

// A window with a web view, a toolbar, and a file watcher. Off-sandbox the
// page's base URL is the document's folder, so Qt resolves relative images and
// links natively; we only intercept navigations to route them.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

public slots:
    void openPath(const QString &path, bool recordHistory = true);

public:
    // Reopen the document last looked at. False when the recent list is empty
    // or nothing in it still exists, leaving the empty state up.
    bool openMostRecent();

    bool hasDocument() const { return !m_current.isEmpty(); }

    // Test seam: lets the nav test click links and observe loads on the real
    // app wiring (the queued openDocument connection lives in the ctor).
    QWebEngineView *webView() const { return m_view; }

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
    void exportPdf();
    void reveal();

    QWebEngineView *m_view;
    MarkdownPage *m_page;
    QFileSystemWatcher *m_watcher;
    QLineEdit *m_find;
    QAction *m_back;
    QAction *m_reload;
    QMenu *m_recentMenu;
    QToolBar *m_findBar;
    QLabel *m_findCount;
    bool m_stale = false;

    QStringList m_history;
    QString m_current;
    bool m_autoReload = true;
};
