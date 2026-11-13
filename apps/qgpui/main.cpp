#include "HeadsetConnection.h"
#include "MainWindow.h"
#include "SingleInstance.h"

#include "installer/KeepAwake.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStandardPaths>

namespace {

// How long QGPUI waits on exit for the headsets to answer the keep-awake releases.
constexpr int kReleaseWaitMsecs = 2000;
// How long QGPUI waits on exit for the adb server it started to stop.
constexpr int kStopServerWaitMsecs = 2000;

// Where to read the catalog when the command line names none: the file opened last, else
// qgpui-catalog.json in the app's data folder, else the one the installer put next to qgpui.exe,
// else (in a development build) the repository's local/ folder, where tools/questports-payload's
// build-catalog writes it.
QString defaultCatalogPath()
{
    const QString last = QSettings().value(QStringLiteral("catalogPath")).toString();
    if (!last.isEmpty() && QFileInfo::exists(last))
        return last;

    const QString appData = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                .filePath(QStringLiteral("qgpui-catalog.json"));
    if (QFileInfo::exists(appData))
        return appData;

    const QString shipped =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("qgpui-catalog.json"));
    if (QFileInfo::exists(shipped))
        return shipped;
#ifdef QGPUI_DEV_CATALOG
    if (QFileInfo::exists(QStringLiteral(QGPUI_DEV_CATALOG)))
        return QStringLiteral(QGPUI_DEV_CATALOG);
#endif
    return appData;
}

QIcon appIcon()
{
    QIcon icon;
#ifdef Q_OS_MACOS
    // Also the Dock icon, which needs the large sizes.
    for (int size : {16, 32, 64, 128, 256, 512, 1024})
#else
    for (int size : {16, 24, 32, 48, 64, 128, 256})
#endif
        icon.addFile(QStringLiteral(":/icons/qgpui-%1.png").arg(size), QSize(size, size));
    return icon;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // QSettings stores nothing on Windows without an organization name.
    QApplication::setOrganizationName(QStringLiteral("qgpui"));
    QApplication::setApplicationName(QStringLiteral("qgpui"));
    QApplication::setApplicationDisplayName(QStringLiteral("QGPUI"));
    QApplication::setApplicationVersion(QStringLiteral(QGPUI_VERSION));
    QApplication::setWindowIcon(appIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meta Quest game ports manager."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("catalog"), QStringLiteral("QGPUI catalog file to open."),
                                 QStringLiteral("[catalog]"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    // The other instance has another working folder, so pass it an absolute path.
    const QString catalogArg = args.isEmpty() ? QString() : QFileInfo(args.first()).absoluteFilePath();

    SingleInstance instance;
    if (!instance.claim(catalogArg))
        return 0;

    MainWindow window;
    window.loadCatalog(catalogArg.isEmpty() ? defaultCatalogPath() : catalogArg);
    window.resize(1400, 900);
    window.show();

    // A second start brings this window to the front, and opens the catalog it was given.
    QObject::connect(&instance, &SingleInstance::activated, &window, [&window](const QString& catalog) {
        if (!catalog.isEmpty())
            window.loadCatalog(catalog);
        if (window.isMinimized())
            window.showNormal();
        window.show();
        window.raise();
        window.activateWindow();
    });
    const int result = QApplication::exec();
    // Hand every kept-awake headset back to its proximity sensor while the windows and the adb
    // client still exist (decision 0015).
    installer::KeepAwake::releaseAll(kReleaseWaitMsecs);
    // Then stop the adb server if QGPUI started it, so that nothing of QGPUI keeps running: macOS
    // would report QGPUI as running in the background, and Windows would keep its folder locked
    // (decision 0021).
    window.headset()->stopServerIfStarted(kStopServerWaitMsecs);
    return result;
}
