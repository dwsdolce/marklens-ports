#include "assets.h"
#include "mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QDir>
#include <QIcon>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QtGlobal>

namespace {

// QApplication that opens documents macOS delivers as events.
//
// A double-clicked or "Open With" document does not arrive in argv on macOS.
// The OS sends an Apple Event, which Qt turns into a QFileOpenEvent aimed at
// the application object and discards if nothing handles it - so the app comes
// up on its empty state, looking exactly like a file association that was never
// registered.
//
// This overrides event() rather than installing an application-wide event
// filter, matching the Python port. A filter would be safe here, since C++ has
// none of the wrapper marshalling that made one segfault under PySide, but
// event() is the narrower hook in either language: it sees only what is
// addressed to the application, which is where QFileOpenEvent is sent, rather
// than every event delivered to every object.
//
// The event can also arrive before exec(), when opening the document is what
// launched the app, so a path that turns up before the window exists is held
// and opened once there is somewhere to put it.
class Application : public QApplication {
public:
    Application(int &argc, char **argv) : QApplication(argc, argv) {}

    // Attach the first window and flush whatever arrived before it existed.
    void setWindow(MainWindow *window) {
        m_window = window;
        if (!m_pending.isEmpty()) {
            openDocument(m_pending);
            m_pending.clear();
        }
    }

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent *>(e)->file();
            if (!path.isEmpty()) {
                if (m_window)
                    openDocument(path);
                else
                    m_pending = path; // launched by the document; window pending
            }
            return true;
        }
        return QApplication::event(e);
    }

private:
    // A document the system hands over gets an instance of its own, because
    // there is no relationship between it and whatever is already open:
    // replacing the document in place would put an unrelated file on the Back
    // stack, and Back means "the page I came from". Following a link is the
    // opposite case and still replaces in place.
    //
    // Windows and Linux reach this the other way round and never get here at
    // all: having no single-instance rule, their file managers simply run the
    // executable again. macOS routes every document to the application already
    // running, so a second instance has to be asked for.
    void openDocument(const QString &path) {
        // An empty window has nothing to displace, and leaving one behind while
        // a second instance starts is what no document application does.
        if (!m_window->hasDocument()) {
            m_window->openPath(path);
            m_window->show();
            m_window->raise();
            m_window->activateWindow();
            return;
        }
        if (!startNewInstance(path))
            m_window->openPath(path); // no bundle to start: see below
    }

    // False when there is no application bundle to start, which is a
    // development build run straight from the build tree. Opening in place is
    // then the only thing left, and is what happened before this existed.
    static bool startNewInstance(const QString &path) {
        QDir bundle(QCoreApplication::applicationDirPath()); // <app>.app/Contents/MacOS
        if (!bundle.cdUp() || !bundle.cdUp())
            return false;
        if (!bundle.absolutePath().endsWith(".app"))
            return false;
        // -n is what overrides the single-instance rule; without it the open is
        // handed straight back to this process and nothing happens.
        return QProcess::startDetached(
            "/usr/bin/open", {"-n", "-a", bundle.absolutePath(), path});
    }

    MainWindow *m_window = nullptr;
    QString m_pending;
};

} // namespace

int main(int argc, char **argv) {
    Application app(argc, argv);
    app.setApplicationName("Marklens");
    app.setOrganizationName("Marklens"); // gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(assets::iconPath()));

    MainWindow window;
    window.show();

    bool opened = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith('-')) {
            window.openPath(arg);
            opened = true;
            break;
        }
    }

    app.setWindow(&window);

    // Nothing named on the command line: pick up where you left off.
    if (!opened) {
#if defined(Q_OS_MACOS)
        // Except that on macOS "nothing named" is also what opening a document
        // from Finder looks like - the path arrives as an Apple Event once the
        // event loop is running, not in argv. Reopening immediately would show
        // the previous document first and push it back to the top of the recent
        // list, so the fallback waits a moment and stands down if a document
        // turns up in the meantime.
        QTimer::singleShot(250, &window, [&window] {
            if (!window.hasDocument())
                window.openMostRecent();
        });
#else
        window.openMostRecent();
#endif
    }

    return app.exec();
}
