#pragma once

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

// What qgp knows about a game beyond the catalog: which stores sell the PC game its recipe needs,
// which of them have it on this PC, and whether the game is on the current headset.
enum class GameFlag {
    SoldOnSteam = 0x1,   // A pc-install input lists Steam in its stores.
    SoldOnGog = 0x2,     // ... lists GOG.
    InSteamLibrary = 0x4, // The input was found in a Steam install on this PC.
    InGogLibrary = 0x8,   // ... in a GOG install, or GOG Galaxy lists it as owned.
    OnHeadset = 0x10,     // The game's package is installed on the current headset.
    InstalledByQgp = 0x20,      // ... and the headset's install records list it.
    InstalledOutsideQgp = 0x40, // ... and they do not: it was installed some other way.
    SelfContained = 0x80, // qgp installs it on its own: no files from the user, or only free ones.
};
Q_DECLARE_FLAGS(GameFlags, GameFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(GameFlags)

// The game's package: the recipe's override, else the payload's, else the one the recipe was
// written against. Empty when none is known.
QString gamePackageName(const questports::Game& game);

// SelfContained when the game has an automatic recipe whose APKs qgp downloads itself (no manual
// source), whose required inputs are all marked free and whose groups each have a free input.
GameFlags selfContained(const questports::Game& game);

// SoldOnSteam and SoldOnGog, from the recipe alone.
GameFlags storesSelling(const questports::Game& game);
// InSteamLibrary and InGogLibrary, from the stores of this PC.
GameFlags storesHolding(const questports::Game& game, const gamestores::GameFinder& finder);
// OnHeadset with InstalledByQgp or InstalledOutsideQgp, from the headset's third-party packages and
// the packages its install records list. No flags when the game's package is not installed.
GameFlags headsetFlags(const questports::Game& game, const QSet<QString>& installed, const QSet<QString>& recorded);

// A store page of one stores entry of a pc-install input.
struct StorePage
{
    QString store;     // "steam" or "gog".
    QString productId; // Steam app ID or GOG product ID; empty when the recipe has none.
    QString name;      // What to search for when there is no ID: the install folder name.
    // Steam: the app's page, or a search. GOG: a search; gogPageLookup() gives the real page.
    QUrl url() const;
    // GOG's product API, which answers with the page's address in links.product_card.
    QUrl gogPageLookup() const;
};

// The Steam and GOG pages of one pc-install input (an object of a recipe's "inputs").
QList<StorePage> storePages(const QJsonObject& input);
