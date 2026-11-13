#pragma once

#include "installer/Recipe.h"
#include "questports/Catalog.h"

#include <QFlags>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QUrl>

namespace gamestores {
class GameFinder;
}

// What QGPUI knows about a game beyond the catalog: which stores sell the PC game its recipe needs,
// which of them have it on this PC, and whether the game is on the current headset.
enum class GameFlag {
    SoldOnSteam = 0x1,   // A pc-install input lists Steam in its stores.
    SoldOnGog = 0x2,     // ... lists GOG.
    InSteamLibrary = 0x4, // The input was found in a Steam install on this PC.
    InGogLibrary = 0x8,   // ... in a GOG install, or GOG Galaxy lists it as owned.
    OnHeadset = 0x10,     // The game's package is installed on the current headset.
    InstalledByQgpui = 0x20,      // ... and the headset's install records list it.
    InstalledOutsideQgpui = 0x40, // ... and they do not: it was installed some other way.
    SelfContained = 0x80, // QGPUI installs it on its own: no files from the user, or only free ones.
    UpdateAvailable = 0x100, // On the headset, and its APK has a newer release.
    SoldOnEpic = 0x200,     // A pc-install input lists Epic in its stores.
    InEpicLibrary = 0x400,  // The input was found in an Epic Games Launcher install on this PC.
};
Q_DECLARE_FLAGS(GameFlags, GameFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(GameFlags)

// The game's package: the recipe's override, else the payload's, else the one the recipe was
// written against. Empty when none is known.
QString gamePackageName(const questports::Game& game);

// SelfContained when the game has an automatic recipe whose APKs QGPUI downloads itself (no manual
// source), whose required inputs are all marked free and whose groups each have a free input.
GameFlags selfContained(const questports::Game& game);

// SoldOnSteam, SoldOnGog and SoldOnEpic, from the recipe alone.
GameFlags storesSelling(const questports::Game& game);
// InSteamLibrary, InGogLibrary and InEpicLibrary, from the stores of this PC.
GameFlags storesHolding(const questports::Game& game, const gamestores::GameFinder& finder);
// OnHeadset with InstalledByQgpui or InstalledOutsideQgpui, from the headset's third-party packages and
// the packages its install records list. No flags when the game's package is not installed.
GameFlags headsetFlags(const questports::Game& game, const QSet<QString>& installed, const QSet<QString>& recorded);

// The APK source whose releases tell whether the game has an update: the recipe's first APK, whose
// release QGPUI records at install, when it comes from GitHub. Nothing for other sources.
std::optional<installer::ApkSource> updateSource(const installer::Recipe& recipe);

// A store page of one stores entry of a pc-install input.
struct StorePage
{
    QString store;     // "steam", "gog" or "epic".
    QString productId; // Steam app ID, GOG product ID or Epic app name; empty when the recipe has none.
    QString name;      // What to search for when there is no ID: the install folder name.
    // Steam: the app's page, or a search. GOG: a search; gogPageLookup() gives the real page.
    // Epic: a search, because the store's page addresses are not the launcher's app names.
    QUrl url() const;
    // GOG's product API, which answers with the page's address in links.product_card.
    QUrl gogPageLookup() const;
};

// The Steam, GOG and Epic pages of one pc-install input (an object of a recipe's "inputs").
QList<StorePage> storePages(const QJsonObject& input);
