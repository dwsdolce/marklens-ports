#include "assets.h"
#include "mainwindow.h"

#include <QApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QIcon>
#include <QObject>
#include <QString>

namespace {

// macOS does not pass a double-clicked or "Open With" document in argv. It
// sends an Apple Event, which Qt delivers as a QFileOpenEvent to the
// application object - and drops on the floor if nothing is listening. The app
// then launches showing its empty state, which looks exactly like a file
// association that was never registered, though the association is fine and
// only the event was ignored.
//
// The event can also arrive before exec() starts, when the app is launched *by*
// opening a document, so a path that turns up before the window is ready is
// held and opened once it is.
class DocumentOpener : public QObject {
public:
    explicit DocumentOpener(QObject *parent = nullptr) : QObject(parent) {}

    // Attach the window and flush whatever arrived before it existed.
    void setWindow(MainWindow *window) {
        m_window = window;
        if (!m_pending.isEmpty()) {
            m_window->openPath(m_pending);
            m_pending.clear();
        }
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent *>(event)->file();
            if (!path.isEmpty()) {
                if (m_window)
                    m_window->openPath(path);
                else
                    m_pending = path; // launched by the document; window pending
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    MainWindow *m_window = nullptr;
    QString m_pending;
};

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Marklens");
    app.setOrganizationName("Marklens"); // gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(assets::iconPath()));

    // Installed before the window is built, so a launch-by-document event that
    // arrives during construction is caught rather than missed.
    auto *opener = new DocumentOpener(&app);
    app.installEventFilter(opener);

    MainWindow window;
    window.show();

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith('-')) {
            window.openPath(arg);
            break;
        }
    }

    opener->setWindow(&window);

    return app.exec();
}
