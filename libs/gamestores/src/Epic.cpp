#include "gamestores/Epic.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace gamestores {

QString EpicLibrary::findManifestsFolder()
{
    QStringList candidates;
#ifdef Q_OS_WIN
    const QSettings launcher(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Epic Games\\EpicGamesLauncher"),
                             QSettings::Registry32Format);
    const QString appData = launcher.value(QStringLiteral("AppDataPath")).toString();
    if (!appData.isEmpty())
        candidates.append(QDir(QDir::fromNativeSeparators(appData)).filePath(QStringLiteral("Manifests")));
    const QString programData = qEnvironmentVariable("PROGRAMDATA");
    if (!programData.isEmpty())
        candidates.append(QDir::fromNativeSeparators(programData) +
                          QStringLiteral("/Epic/EpicGamesLauncher/Data/Manifests"));
#elif defined(Q_OS_MACOS)
    candidates.append(QDir::homePath() +
                      QStringLiteral("/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests"));
#endif
    for (const QString& candidate : std::as_const(candidates)) {
        if (QFileInfo(candidate).isDir())
            return QDir::cleanPath(candidate);
    }
    return {};
}

std::optional<EpicLibrary> EpicLibrary::load(const QString& folder, QString* errorMessage)
{
    const QDir dir(folder);
    if (folder.isEmpty() || !dir.exists()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Epic Games Launcher manifests: %1 does not exist.").arg(folder);
        return std::nullopt;
    }

    EpicLibrary library;
    for (const QString& name : dir.entryList({QStringLiteral("*.item")}, QDir::Files, QDir::Name)) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject item = QJsonDocument::fromJson(file.readAll()).object();
        const QString appName = item.value(QLatin1StringView("AppName")).toString();
        const QString location = item.value(QLatin1StringView("InstallLocation")).toString();
        if (appName.isEmpty() || location.isEmpty())
            continue;
        // An add-on has its own manifest that names the game it belongs to, with the same folder.
        const QString mainGame = item.value(QLatin1StringView("MainGameAppName")).toString();
        if (!mainGame.isEmpty() && mainGame != appName)
            continue;
        library.add({appName, item.value(QLatin1StringView("DisplayName")).toString(),
                     QDir::cleanPath(QDir::fromNativeSeparators(location)),
                     !item.value(QLatin1StringView("bIsIncompleteInstall")).toBool()});
    }
    return library;
}

} // namespace gamestores
