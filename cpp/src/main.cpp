#include "assets.h"
#include "mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QIcon>
#include <QString>

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

    // Attach the window and flush whatever arrived before it existed.
    void setWindow(MainWindow *window) {
        m_window = window;
        if (!m_pending.isEmpty()) {
            m_window->openPath(m_pending);
            m_pending.clear();
        }
    }

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent *>(e)->file();
            if (!path.isEmpty()) {
                if (m_window)
                    m_window->openPath(path);
                else
                    m_pending = path; // launched by the document; window pending
            }
            return true;
        }
        return QApplication::event(e);
    }

private:
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

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith('-')) {
            window.openPath(arg);
            break;
        }
    }

    app.setWindow(&window);

    return app.exec();
}
