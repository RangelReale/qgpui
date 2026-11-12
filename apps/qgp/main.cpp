#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

namespace {

// Where to read the catalog when the command line names none: the file opened last, else
// qgp-catalog.json in the app's data folder, else (in a development build) the repository's local/
// folder, where tools/questports-payload's build-catalog writes it.
QString defaultCatalogPath()
{
    const QString last = QSettings().value(QStringLiteral("catalogPath")).toString();
    if (!last.isEmpty() && QFileInfo::exists(last))
        return last;

    const QString appData = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                .filePath(QStringLiteral("qgp-catalog.json"));
#ifdef QGP_DEV_CATALOG
    if (!QFileInfo::exists(appData) && QFileInfo::exists(QStringLiteral(QGP_DEV_CATALOG)))
        return QStringLiteral(QGP_DEV_CATALOG);
#endif
    return appData;
}

QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/qgp-%1.png").arg(size), QSize(size, size));
    return icon;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // QSettings stores nothing on Windows without an organization name.
    QApplication::setOrganizationName(QStringLiteral("qgp"));
    QApplication::setApplicationName(QStringLiteral("qgp"));
    QApplication::setApplicationDisplayName(QStringLiteral("qgp"));
    QApplication::setWindowIcon(appIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meta Quest game ports manager."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("catalog"), QStringLiteral("qgp catalog file to open."),
                                 QStringLiteral("[catalog]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    MainWindow window;
    window.loadCatalog(args.isEmpty() ? defaultCatalogPath() : args.first());
    window.resize(1400, 900);
    window.show();
    return QApplication::exec();
}
