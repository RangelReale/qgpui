#include "GameFlags.h"

#include "gamestores/Finder.h"

#include <QHash>
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

QString gamePackageName(const questports::Game& game)
{
    QString package = game.recipe.value(QLatin1StringView("packageName")).toString();
    if (package.isEmpty())
        package = game.install.value(QLatin1StringView("packageName")).toString();
    if (package.isEmpty()) {
        package = game.recipe.value(QLatin1StringView("checkedAgainst")).toObject()
                      .value(QLatin1StringView("packageName")).toString();
    }
    return package;
}

GameFlags headsetFlags(const questports::Game& game, const QSet<QString>& installed, const QSet<QString>& recorded)
{
    const QString package = gamePackageName(game);
    if (package.isEmpty() || !installed.contains(package))
        return {};
    return GameFlag::OnHeadset | (recorded.contains(package) ? GameFlag::InstalledByQgp : GameFlag::InstalledOutsideQgp);
}

GameFlags selfContained(const questports::Game& game)
{
    // Guided and manual recipes leave work to the user outside qgp, such as running the port's
    // own installer on their game files.
    if (!game.hasRecipe() || game.recipe.value(QLatin1StringView("support")).toString() != QLatin1StringView("automatic"))
        return {};
    for (const QJsonValue& apk : game.recipe.value(QLatin1StringView("apks")).toArray()) {
        if (apk.toObject().value(QLatin1StringView("source")).toString() == QLatin1StringView("manual"))
            return {};
    }
    QHash<QString, bool> freeGroups; // Group -> one of its inputs is free.
    for (const QJsonValue& value : game.recipe.value(QLatin1StringView("inputs")).toArray()) {
        const QJsonObject input = value.toObject();
        const bool required = input.value(QLatin1StringView("required")).toBool(true);
        const bool free = input.value(QLatin1StringView("free")).toBool();
        if (required && !free)
            return {};
        const QString group = input.value(QLatin1StringView("group")).toString();
        if (!group.isEmpty())
            freeGroups[group] = freeGroups.value(group) || free;
    }
    for (bool free : std::as_const(freeGroups)) {
        if (!free)
            return {};
    }
    return GameFlag::SelfContained;
}

GameFlags storesSelling(const questports::Game& game)
{
    GameFlags flags;
    for (const QJsonObject& input : pcInstallInputs(game)) {
        for (const QJsonValue& value : input.value(QLatin1StringView("stores")).toArray()) {
            const QString store = value.toObject().value(QLatin1StringView("store")).toString();
            if (store == QLatin1StringView("steam"))
                flags |= GameFlag::SoldOnSteam;
            else if (store == QLatin1StringView("gog"))
                flags |= GameFlag::SoldOnGog;
        }
    }
    return flags;
}

GameFlags storesHolding(const questports::Game& game, const gamestores::GameFinder& finder)
{
    GameFlags flags;
    for (const QJsonObject& input : pcInstallInputs(game)) {
        for (const gamestores::Location& location : finder.find(input)) {
            if (location.status == gamestores::Location::Status::Incomplete)
                continue;
            if (location.store == QLatin1StringView("steam"))
                flags |= GameFlag::InSteamLibrary;
            else if (location.store == QLatin1StringView("gog"))
                flags |= GameFlag::InGogLibrary;
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
