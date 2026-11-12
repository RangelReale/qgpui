#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>

namespace gamestores {

// One app from a Steam library's appmanifest_<appid>.acf.
struct SteamApp
{
    QString appId;
    QString name;
    QString libraryPath;  // The library folder that holds the app.
    QString installPath;  // <library>/steamapps/common/<installdir>.
    bool fullyInstalled = false; // StateFlags has bit 4: not downloading or half-updated.
    QString buildId;
    QString betaBranch;   // The selected beta branch (BetaKey); empty for the default branch.
};

// The Steam libraries on this PC and the apps installed in them.
class SteamLibrary
{
public:
    // Steam's own folder: from the registry on Windows, else the usual places. Empty when Steam is
    // not found.
    static QString findSteamRoot();

    // Reads <steamRoot>/steamapps/libraryfolders.vdf, then every appmanifest_*.acf in every library
    // it lists, and in steamRoot itself. A library whose folder is missing (a drive that is not
    // connected) is skipped. A manifest that cannot be read is skipped. Returns nothing, and sets
    // *errorMessage when it is given, when steamRoot has no steamapps folder or libraryfolders.vdf
    // exists but cannot be parsed.
    static std::optional<SteamLibrary> load(const QString& steamRoot, QString* errorMessage = nullptr);

    QString root() const { return m_root; }
    QStringList libraries() const { return m_libraries; } // Folders that exist, root first.
    const QHash<QString, SteamApp>& apps() const { return m_apps; } // By app ID.
    std::optional<SteamApp> app(const QString& appId) const;

private:
    QString m_root;
    QStringList m_libraries;
    QHash<QString, SteamApp> m_apps;
};

} // namespace gamestores
