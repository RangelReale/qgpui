#pragma once

#include "gamestores/Epic.h"
#include "gamestores/Gog.h"
#include "gamestores/Steam.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace gamestores {

// Where a recipe's pc-install input was found.
struct Location
{
    enum class Status {
        Installed,         // The folder holds every detect file.
        Incomplete,        // The store lists the game here, but the install is unfinished or files are missing.
        OwnedNotInstalled, // GOG Galaxy lists the game as owned; it is not installed.
    };

    Status status = Status::Installed;
    QString store;     // "steam", "gog" or "epic".
    QString productId; // Steam app ID, GOG product ID or Epic app name; empty when unknown.
    QString name;
    QString path;      // The folder with the recipe's root applied; empty for OwnedNotInstalled.
};

// Finds the PC installs that a recipe's pc-install inputs ask for, in the Steam, GOG and Epic libraries.
class GameFinder
{
public:
    GameFinder(std::optional<SteamLibrary> steam, GogLibrary gog, EpicLibrary epic = {});

    // Reads the Steam library, the GOG registry entries, GOG Galaxy's database and the Epic Games
    // Launcher's manifests of this PC. A
    // store that is missing or cannot be read is left out; *warnings, when given, collects why.
    static GameFinder forThisPc(QStringList* warnings = nullptr);

    // Locations for one pc-install input (an object of a recipe's "inputs"). In order:
    // 1. each entry of the input's "stores" with an appId, looked up by ID;
    // 2. each entry with an installDir, matched against install folder names;
    // 3. every other installed Steam, GOG and Epic game whose folder holds the "detect" files, so packs,
    //    other editions and games without an ID in the recipe are still found.
    // Each folder appears once. Steps 1 and 2 report Incomplete for a matching game whose files are
    // missing; step 3 reports only Installed folders.
    QList<Location> find(const QJsonObject& input) const;

    // The folder detect paths are relative to: folder/root when that exists, else folder itself,
    // because a user may also pick the root folder directly.
    static QString applyRoot(const QString& folder, const QString& root);

    // True when every detect path exists below folder. A path segment may hold * and ? wildcards;
    // such a segment matches any file or folder name it fits.
    static bool matchesDetect(const QString& folder, const QStringList& detect);

    const std::optional<SteamLibrary>& steam() const { return m_steam; }
    const GogLibrary& gog() const { return m_gog; }
    const EpicLibrary& epic() const { return m_epic; }

private:
    std::optional<SteamLibrary> m_steam;
    GogLibrary m_gog;
    EpicLibrary m_epic;
};

} // namespace gamestores
