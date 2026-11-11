#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("adbmanager"));

    MainWindow window;
    window.resize(1100, 760);
    window.show();
    return QApplication::exec();
}
