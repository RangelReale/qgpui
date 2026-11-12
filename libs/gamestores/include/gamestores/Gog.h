#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace gamestores {

// One GOG product: installed, owned, or both.
struct GogGame
{
    QString productId;
    QString name;
    QString installPath; // Empty when the game is not installed.
    QString buildId;
    bool owned = false;  // Known only from GOG Galaxy's database.

    bool installed() const { return !installPath.isEmpty(); }
};

// The GOG games on this PC, gathered from several sources and merged by product ID.
class GogLibrary
{
public:
    // Adds a game. When the product ID is already known, empty fields are filled in from game and
    // owned becomes true if either says so.
    void add(const GogGame& game);

    // Adds the games that GOG's installers and GOG Galaxy record in the registry
    // (HKLM\SOFTWARE\GOG.com\Games, 32-bit view). Does nothing outside Windows. Returns the number
    // of games read.
    int addRegistry();

    // Adds the installed and owned games of GOG Galaxy 2's database (galaxy-2.0.db). The database
    // is copied, with its write-ahead log, to a temporary folder first, so Galaxy's own file is
    // never opened. Returns false, and sets *errorMessage when it is given, when the copy or a
    // query fails; the schema is not documented and can change with a Galaxy update.
    bool addGalaxyDatabase(const QString& path, QString* errorMessage = nullptr);

    // Galaxy's database under %PROGRAMDATA%\GOG.com\Galaxy\storage: the highest galaxy-2.*.db.
    // Empty when there is none.
    static QString findGalaxyDatabase();

    // Reads goggame-<productId>.info in folder, the file every GOG install carries. Returns its
    // product ID and name, with installPath set to folder, or nothing when there is no such file.
    static std::optional<GogGame> readInfoFile(const QString& folder);

    QList<GogGame> games() const { return m_games.values(); }
    std::optional<GogGame> game(const QString& productId) const;

private:
    QHash<QString, GogGame> m_games;
};

} // namespace gamestores
