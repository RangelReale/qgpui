#pragma once

#include "questports/Catalog.h"

#include <QFlags>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

namespace gamestores {
class GameFinder;
}

// How a game's recipe relates to Steam and GOG: which stores sell the PC game it needs, and which
// of them have it on this PC.
enum class StoreFlag {
    SoldOnSteam = 0x1,   // A pc-install input lists Steam in its stores.
    SoldOnGog = 0x2,     // ... lists GOG.
    InSteamLibrary = 0x4, // The input was found in a Steam install on this PC.
    InGogLibrary = 0x8,   // ... in a GOG install, or GOG Galaxy lists it as owned.
};
Q_DECLARE_FLAGS(StoreFlags, StoreFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(StoreFlags)

// SoldOnSteam and SoldOnGog, from the recipe alone.
StoreFlags storesSelling(const questports::Game& game);
// InSteamLibrary and InGogLibrary, from the stores of this PC.
StoreFlags storesHolding(const questports::Game& game, const gamestores::GameFinder& finder);

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
