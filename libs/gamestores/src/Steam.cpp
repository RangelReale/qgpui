#include "gamestores/Steam.h"

#include "gamestores/Vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace gamestores {

namespace {

constexpr int kStateFullyInstalled = 4;

QString clean(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

bool samePath(const QString& a, const QString& b)
{
#ifdef Q_OS_WIN
    return clean(a).compare(clean(b), Qt::CaseInsensitive) == 0;
#else
    return clean(a) == clean(b);
#endif
}

std::optional<VdfObject> readVdf(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    QString parseError;
    auto vdf = parseVdf(file.readAll(), &parseError);
    if (!vdf && errorMessage)
        *errorMessage = QStringLiteral("%1: %2").arg(path, parseError);
    return vdf;
}

// The library folders libraryfolders.vdf lists. The current layout is
// "libraryfolders" { "0" { "path" "..." ... } }; the old one is "LibraryFolders" { "1" "D:\\..." }.
QStringList listedLibraries(const VdfObject& vdf)
{
    QStringList paths;
    const VdfObject folders = vdfObject(vdf, {QStringLiteral("libraryfolders")});
    for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
        bool numeric = false;
        it.key().toInt(&numeric);
        if (!numeric)
            continue; // Old files also hold "TimeNextStatsReport" and "ContentStatsID".
        if (it.value().typeId() == QMetaType::QString)
            paths.append(it.value().toString());
        else
            paths.append(vdfString(it.value().toMap(), {QStringLiteral("path")}));
    }
    return paths;
}

std::optional<SteamApp> readManifest(const QString& library, const QString& path)
{
    const auto vdf = readVdf(path, nullptr);
    if (!vdf)
        return std::nullopt;
    const VdfObject state = vdfObject(*vdf, {QStringLiteral("AppState")});
    SteamApp app;
    app.appId = vdfString(state, {QStringLiteral("appid")});
    if (app.appId.isEmpty())
        return std::nullopt;
    app.name = vdfString(state, {QStringLiteral("name")});
    app.libraryPath = library;
    const QString installDir = vdfString(state, {QStringLiteral("installdir")});
    if (!installDir.isEmpty())
        app.installPath = clean(library + QStringLiteral("/steamapps/common/") + installDir);
    app.fullyInstalled = (vdfString(state, {QStringLiteral("StateFlags")}).toInt() & kStateFullyInstalled) != 0;
    app.buildId = vdfString(state, {QStringLiteral("buildid")});
    app.betaBranch = vdfString(state, {QStringLiteral("UserConfig"), QStringLiteral("BetaKey")});
    if (app.betaBranch.isEmpty())
        app.betaBranch = vdfString(state, {QStringLiteral("MountedConfig"), QStringLiteral("BetaKey")});
    return app;
}

} // namespace

QString SteamLibrary::findSteamRoot()
{
    QStringList candidates;
#ifdef Q_OS_WIN
    candidates << QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QSettings::NativeFormat)
                      .value(QStringLiteral("SteamPath"))
                      .toString()
               << QSettings(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam"), QSettings::Registry32Format)
                      .value(QStringLiteral("InstallPath"))
                      .toString();
#elif defined(Q_OS_MACOS)
    candidates << QDir::homePath() + QStringLiteral("/Library/Application Support/Steam");
#else
    candidates << QDir::homePath() + QStringLiteral("/.steam/steam")
               << QDir::homePath() + QStringLiteral("/.local/share/Steam");
#endif
    for (const QString& candidate : std::as_const(candidates)) {
        if (!candidate.isEmpty() && QFileInfo::exists(candidate + QStringLiteral("/steamapps")))
            return clean(candidate);
    }
    return {};
}

std::optional<SteamLibrary> SteamLibrary::load(const QString& steamRoot, QString* errorMessage)
{
    const QString root = clean(steamRoot);
    if (root.isEmpty() || !QFileInfo(root + QStringLiteral("/steamapps")).isDir()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1 is not a Steam folder (no steamapps).").arg(steamRoot);
        return std::nullopt;
    }

    QStringList candidates{root};
    const QString foldersFile = root + QStringLiteral("/steamapps/libraryfolders.vdf");
    if (QFileInfo::exists(foldersFile)) {
        const auto vdf = readVdf(foldersFile, errorMessage);
        if (!vdf)
            return std::nullopt;
        candidates += listedLibraries(*vdf);
    }

    SteamLibrary library;
    library.m_root = root;
    for (const QString& candidate : std::as_const(candidates)) {
        if (candidate.isEmpty() || !QFileInfo(candidate + QStringLiteral("/steamapps")).isDir())
            continue;
        const bool known = std::any_of(library.m_libraries.cbegin(), library.m_libraries.cend(),
                                       [&](const QString& seen) { return samePath(seen, candidate); });
        if (!known)
            library.m_libraries.append(clean(candidate));
    }

    for (const QString& folder : std::as_const(library.m_libraries)) {
        const QDir steamapps(folder + QStringLiteral("/steamapps"));
        const QStringList manifests =
            steamapps.entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files, QDir::Name);
        for (const QString& manifest : manifests) {
            if (auto app = readManifest(folder, steamapps.filePath(manifest)))
                library.m_apps.insert(app->appId, *app);
        }
    }
    return library;
}

std::optional<SteamApp> SteamLibrary::app(const QString& appId) const
{
    const auto it = m_apps.constFind(appId);
    if (it == m_apps.cend())
        return std::nullopt;
    return *it;
}

} // namespace gamestores
