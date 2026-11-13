#pragma once

#include <QHash>
#include <QString>

#include <optional>

namespace gamestores {

// One game installed by the Epic Games Launcher, from its <id>.item manifest.
struct EpicGame
{
    QString appName;     // The launcher's ID for the game ("AppName"), such as "Snapper".
    QString name;        // "DisplayName".
    QString installPath; // "InstallLocation".
    bool complete = true; // false while the launcher still lists the install as incomplete.
};

// The games the Epic Games Launcher has installed on this PC.
class EpicLibrary
{
public:
    // The launcher's manifest folder: <AppDataPath>Manifests, where AppDataPath comes from the
    // registry on Windows (default %PROGRAMDATA%\Epic\EpicGamesLauncher\Data\), or
    // ~/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests on macOS. Empty when the
    // folder does not exist.
    static QString findManifestsFolder();

    // Reads every *.item file in folder. A manifest that cannot be parsed, has no install folder,
    // or belongs to an add-on of another game is skipped. Returns nothing, and sets *errorMessage
    // when it is given, when folder does not exist.
    static std::optional<EpicLibrary> load(const QString& folder, QString* errorMessage = nullptr);

    // Adds a game, replacing one with the same app name.
    void add(const EpicGame& game) { m_games.insert(game.appName, game); }

    const QHash<QString, EpicGame>& games() const { return m_games; } // By app name.

private:
    QHash<QString, EpicGame> m_games;
};

} // namespace gamestores
