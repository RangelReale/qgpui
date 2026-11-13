#include "gamestores/Gog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace gamestores {

namespace {

const QString kReleasePrefix = QStringLiteral("gog_");

QString clean(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

// .info files hold IDs as strings or as numbers.
QString idString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toInteger());
    return {};
}

// Runs the Galaxy queries on an open database.
bool readGalaxy(QSqlDatabase& db, GogLibrary& library, QString* errorMessage)
{
    auto fail = [&](const QSqlQuery& query) {
        if (errorMessage)
            *errorMessage = QStringLiteral("GOG Galaxy database: %1").arg(query.lastError().text());
        return false;
    };

    QHash<QString, QString> titles;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT gp.releaseKey, gp.value FROM GamePieces gp "
                                   "JOIN GamePieceTypes t ON t.id = gp.gamePieceTypeId "
                                   "WHERE t.type = 'title'")))
        return fail(query);
    while (query.next()) {
        const QString key = query.value(0).toString();
        if (!key.startsWith(kReleasePrefix))
            continue;
        const QJsonObject value = QJsonDocument::fromJson(query.value(1).toString().toUtf8()).object();
        titles.insert(key.mid(kReleasePrefix.size()), value.value(QLatin1StringView("title")).toString());
    }

    if (!query.exec(QStringLiteral("SELECT lr.releaseKey FROM LibraryReleases lr "
                                   "JOIN LicensedReleases li ON li.libraryId = lr.id "
                                   "WHERE li.isOwned = 1")))
        return fail(query);
    while (query.next()) {
        const QString key = query.value(0).toString();
        if (!key.startsWith(kReleasePrefix))
            continue; // Games from other platforms connected to Galaxy, such as steam_<appid>.
        const QString id = key.mid(kReleasePrefix.size());
        library.add({id, titles.value(id), {}, {}, true});
    }

    if (!query.exec(QStringLiteral("SELECT productId, installationPath, buildId FROM InstalledBaseProducts")))
        return fail(query);
    while (query.next()) {
        const QString id = query.value(0).toString();
        library.add({id, titles.value(id), clean(query.value(1).toString()), query.value(2).toString(), false});
    }
    return true;
}

} // namespace

void GogLibrary::add(const GogGame& game)
{
    if (game.productId.isEmpty())
        return;
    auto it = m_games.find(game.productId);
    if (it == m_games.end()) {
        m_games.insert(game.productId, game);
        return;
    }
    if (it->name.isEmpty())
        it->name = game.name;
    if (it->installPath.isEmpty())
        it->installPath = game.installPath;
    if (it->buildId.isEmpty())
        it->buildId = game.buildId;
    it->owned = it->owned || game.owned;
}

int GogLibrary::addRegistry()
{
    int count = 0;
#ifdef Q_OS_WIN
    QSettings games(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games"), QSettings::Registry32Format);
    const QStringList ids = games.childGroups();
    for (const QString& id : ids) {
        games.beginGroup(id);
        GogGame game;
        game.productId = games.value(QStringLiteral("gameID"), id).toString();
        game.name = games.value(QStringLiteral("gameName")).toString();
        game.installPath = clean(games.value(QStringLiteral("path")).toString());
        games.endGroup();
        add(game);
        ++count;
    }
#endif
    return count;
}

bool GogLibrary::addGalaxyDatabase(const QString& path, QString* errorMessage)
{
    QTemporaryDir dir;
    const QString copy = dir.filePath(QStringLiteral("galaxy.db"));
    if (!dir.isValid() || !QFile::copy(path, copy)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot copy %1.").arg(path);
        return false;
    }
    // Recent writes may still be in the write-ahead log; SQLite applies it when the copy opens.
    if (QFileInfo::exists(path + QStringLiteral("-wal")))
        QFile::copy(path + QStringLiteral("-wal"), copy + QStringLiteral("-wal"));

    const QString connection = QStringLiteral("gamestores-galaxy-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(copy);
        if (!db.open()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Cannot open the GOG Galaxy database: %1").arg(db.lastError().text());
        } else {
            ok = readGalaxy(db, *this, errorMessage);
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

QString GogLibrary::findGalaxyDatabase()
{
#if defined(Q_OS_WIN)
    const QString folder = qEnvironmentVariable("PROGRAMDATA", QStringLiteral("C:/ProgramData"))
                           + QStringLiteral("/GOG.com/Galaxy/storage");
#elif defined(Q_OS_MACOS)
    const QString folder = QStringLiteral("/Users/Shared/GOG.com/Galaxy/Storage");
#else
    const QString folder;
#endif
    if (folder.isEmpty())
        return {};
    const QDir storage(folder);
    const QStringList found = storage.entryList({QStringLiteral("galaxy-2.*.db")}, QDir::Files, QDir::Name);
    if (!found.isEmpty())
        return clean(storage.filePath(found.last()));
    return {};
}

std::optional<GogGame> GogLibrary::readInfoFile(const QString& folder)
{
    const QStringList infos =
        QDir(folder).entryList({QStringLiteral("goggame-*.info")}, QDir::Files, QDir::Name);
    for (const QString& info : infos) {
        QFile file(QDir(folder).filePath(info));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        GogGame game;
        game.productId = idString(json.value(QLatin1StringView("gameId")));
        // A DLC carries its own .info next to the base game's; the base game's has gameId == rootGameId.
        const QString root = idString(json.value(QLatin1StringView("rootGameId")));
        if (game.productId.isEmpty() || (!root.isEmpty() && root != game.productId))
            continue;
        game.name = json.value(QLatin1StringView("name")).toString();
        game.buildId = idString(json.value(QLatin1StringView("buildId")));
        game.installPath = clean(folder);
        return game;
    }
    return std::nullopt;
}

std::optional<GogGame> GogLibrary::game(const QString& productId) const
{
    const auto it = m_games.constFind(productId);
    if (it == m_games.cend())
        return std::nullopt;
    return *it;
}

} // namespace gamestores
