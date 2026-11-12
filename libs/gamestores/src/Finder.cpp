#include "gamestores/Finder.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>

#include <utility>

namespace gamestores {

namespace {

QString key(const QString& path)
{
    const QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    return cleaned.toLower();
#else
    return cleaned;
#endif
}

QStringList stringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& item : value.toArray())
        list.append(item.toString());
    return list;
}

bool hasWildcard(const QString& segment)
{
    return segment.contains(u'*') || segment.contains(u'?');
}

// A candidate install: one Steam app or one installed GOG game.
struct Install
{
    QString store;
    QString productId;
    QString name;
    QString folder;
    bool complete = true; // Steam: StateFlags says fully installed.
};

} // namespace

GameFinder::GameFinder(std::optional<SteamLibrary> steam, GogLibrary gog)
    : m_steam(std::move(steam))
    , m_gog(std::move(gog))
{}

GameFinder GameFinder::forThisPc(QStringList* warnings)
{
    auto warn = [&](const QString& message) {
        if (warnings)
            warnings->append(message);
    };

    std::optional<SteamLibrary> steam;
    const QString steamRoot = SteamLibrary::findSteamRoot();
    if (steamRoot.isEmpty()) {
        warn(QStringLiteral("Steam was not found."));
    } else {
        QString error;
        steam = SteamLibrary::load(steamRoot, &error);
        if (!steam)
            warn(error);
    }

    GogLibrary gog;
    gog.addRegistry();
    const QString galaxy = GogLibrary::findGalaxyDatabase();
    if (!galaxy.isEmpty()) {
        QString error;
        if (!gog.addGalaxyDatabase(galaxy, &error))
            warn(error);
    }
    // Fill in names that neither source gave, from the install's own goggame-<id>.info.
    const QList<GogGame> games = gog.games();
    for (const GogGame& game : games) {
        if (game.installed() && game.name.isEmpty()) {
            if (const auto info = GogLibrary::readInfoFile(game.installPath))
                gog.add({game.productId, info->name, {}, {}, false});
        }
    }
    return GameFinder(std::move(steam), std::move(gog));
}

QString GameFinder::applyRoot(const QString& folder, const QString& root)
{
    if (!root.isEmpty()) {
        const QString rooted = QDir(folder).filePath(root);
        if (QFileInfo(rooted).isDir())
            return QDir::cleanPath(rooted);
    }
    return QDir::cleanPath(folder);
}

bool GameFinder::matchesDetect(const QString& folder, const QStringList& detect)
{
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        return false;
    for (const QString& path : detect) {
        QStringList current{folder};
        const QStringList segments = QDir::fromNativeSeparators(path).split(u'/', Qt::SkipEmptyParts);
        for (const QString& segment : segments) {
            QStringList next;
            for (const QString& base : std::as_const(current)) {
                if (hasWildcard(segment)) {
                    const QDir dir(base);
                    const QStringList names =
                        dir.entryList({segment}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
                    for (const QString& name : names)
                        next.append(dir.filePath(name));
                } else {
                    const QString candidate = QDir(base).filePath(segment);
                    if (QFileInfo::exists(candidate))
                        next.append(candidate);
                }
            }
            current = next;
            if (current.isEmpty())
                return false;
        }
    }
    return true;
}

QList<Location> GameFinder::find(const QJsonObject& input) const
{
    const QString root = input.value(QLatin1StringView("root")).toString();
    const QStringList detect = stringList(input.value(QLatin1StringView("detect")));

    QList<Install> installs;
    if (m_steam) {
        for (const SteamApp& app : m_steam->apps())
            installs.append({QStringLiteral("steam"), app.appId, app.name, app.installPath, app.fullyInstalled});
    }
    for (const GogGame& game : m_gog.games()) {
        if (game.installed())
            installs.append({QStringLiteral("gog"), game.productId, game.name, game.installPath, true});
    }

    QList<Location> found;
    QSet<QString> seenFolders;
    QSet<QString> seenOwned;

    auto report = [&](const Install& install, bool onlyIfInstalled) {
        const QString folder = applyRoot(install.folder, root);
        const QString folderKey = key(folder);
        if (seenFolders.contains(folderKey))
            return;
        const bool ok = install.complete && matchesDetect(folder, detect);
        if (!ok && onlyIfInstalled)
            return;
        seenFolders.insert(folderKey);
        found.append({ok ? Location::Status::Installed : Location::Status::Incomplete, install.store,
                      install.productId, install.name, folder});
    };

    const QJsonArray stores = input.value(QLatin1StringView("stores")).toArray();

    // 1. By ID.
    for (const QJsonValue& value : stores) {
        const QJsonObject entry = value.toObject();
        const QString store = entry.value(QLatin1StringView("store")).toString();
        const QString id = entry.value(QLatin1StringView("appId")).toString();
        if (id.isEmpty())
            continue;
        bool matched = false;
        for (const Install& install : std::as_const(installs)) {
            if (install.store == store && install.productId == id) {
                report(install, false);
                matched = true;
            }
        }
        if (!matched && store == QLatin1StringView("gog")) {
            const auto game = m_gog.game(id);
            if (game && game->owned && !seenOwned.contains(id)) {
                seenOwned.insert(id);
                found.append({Location::Status::OwnedNotInstalled, store, id, game->name, {}});
            }
        }
    }

    // 2. By install folder name.
    for (const QJsonValue& value : stores) {
        const QJsonObject entry = value.toObject();
        const QString store = entry.value(QLatin1StringView("store")).toString();
        const QString dir = entry.value(QLatin1StringView("installDir")).toString();
        if (dir.isEmpty())
            continue;
        for (const Install& install : std::as_const(installs)) {
            if (install.store == store && QFileInfo(install.folder).fileName().compare(dir, Qt::CaseInsensitive) == 0)
                report(install, false);
        }
    }

    // 3. By content.
    if (!detect.isEmpty()) {
        for (const Install& install : std::as_const(installs))
            report(install, true);
    }
    return found;
}

} // namespace gamestores
