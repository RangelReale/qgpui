#include "StoreAvailability.h"

#include "gamestores/Finder.h"

#include <QJsonArray>
#include <QUrlQuery>

namespace {

QList<QJsonObject> pcInstallInputs(const questports::Game& game)
{
    QList<QJsonObject> inputs;
    for (const QJsonValue& value : game.recipe.value(QLatin1StringView("inputs")).toArray()) {
        const QJsonObject input = value.toObject();
        if (input.value(QLatin1StringView("kind")).toString() == QLatin1StringView("pc-install"))
            inputs.append(input);
    }
    return inputs;
}

} // namespace

StoreFlags storesSelling(const questports::Game& game)
{
    StoreFlags flags;
    for (const QJsonObject& input : pcInstallInputs(game)) {
        for (const QJsonValue& value : input.value(QLatin1StringView("stores")).toArray()) {
            const QString store = value.toObject().value(QLatin1StringView("store")).toString();
            if (store == QLatin1StringView("steam"))
                flags |= StoreFlag::SoldOnSteam;
            else if (store == QLatin1StringView("gog"))
                flags |= StoreFlag::SoldOnGog;
        }
    }
    return flags;
}

StoreFlags storesHolding(const questports::Game& game, const gamestores::GameFinder& finder)
{
    StoreFlags flags;
    for (const QJsonObject& input : pcInstallInputs(game)) {
        for (const gamestores::Location& location : finder.find(input)) {
            if (location.status == gamestores::Location::Status::Incomplete)
                continue;
            if (location.store == QLatin1StringView("steam"))
                flags |= StoreFlag::InSteamLibrary;
            else if (location.store == QLatin1StringView("gog"))
                flags |= StoreFlag::InGogLibrary;
        }
    }
    return flags;
}

QUrl StorePage::url() const
{
    if (store == QLatin1StringView("steam")) {
        if (!productId.isEmpty())
            return QUrl(QStringLiteral("https://store.steampowered.com/app/%1/").arg(productId));
        QUrl search(QStringLiteral("https://store.steampowered.com/search/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("term"), name);
        search.setQuery(query);
        return search;
    }
    QUrl search(QStringLiteral("https://www.gog.com/en/games"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("query"), name);
    search.setQuery(query);
    return search;
}

QUrl StorePage::gogPageLookup() const
{
    if (store != QLatin1StringView("gog") || productId.isEmpty())
        return {};
    return QUrl(QStringLiteral("https://api.gog.com/products/%1").arg(productId));
}

QList<StorePage> storePages(const QJsonObject& input)
{
    QList<StorePage> pages;
    for (const QJsonValue& value : input.value(QLatin1StringView("stores")).toArray()) {
        const QJsonObject entry = value.toObject();
        const QString store = entry.value(QLatin1StringView("store")).toString();
        if (store != QLatin1StringView("steam") && store != QLatin1StringView("gog"))
            continue;
        QString name = entry.value(QLatin1StringView("installDir")).toString();
        if (name.isEmpty())
            name = input.value(QLatin1StringView("label")).toString();
        pages.append({store, entry.value(QLatin1StringView("appId")).toString(), name});
    }
    return pages;
}
