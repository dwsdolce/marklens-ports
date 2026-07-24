#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Marklens");
    app.setOrganizationName("Marklens"); // gives QSettings (recent files) a home
    app.setWindowIcon(QIcon(QStringLiteral(MARKLENS_SHARED_DIR) + "/icon.png"));

    MainWindow window;
    window.show();

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith('-')) {
            window.openPath(arg);
            break;
        }
    }

    return app.exec();
}
