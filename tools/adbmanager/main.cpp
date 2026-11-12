#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // QSettings stores nothing on Windows without an organization name.
    QApplication::setOrganizationName(QStringLiteral("qgpui"));
    QApplication::setApplicationName(QStringLiteral("adbmanager"));

    MainWindow window;
    window.resize(1100, 760);
    window.show();
    return QApplication::exec();
}
