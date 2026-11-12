#include "questports/Catalog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

namespace questports {

namespace {

std::optional<Catalog> fail(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
    return std::nullopt;
}

// Supabase writes timestamps such as "2026-10-11T16:35:37.471366+00:00". Returns an invalid
// QDateTime for null or text that is not a timestamp.
QDateTime timestamp(const QJsonValue& value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

// Null and missing values give a null QString.
QString optionalString(const QJsonValue& value)
{
    return value.isString() ? value.toString() : QString();
}

QList<Drift> driftList(const QJsonArray& array)
{
    QList<Drift> list;
    list.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QJsonObject entry = value.toObject();
        list.append({entry.value(QLatin1StringView("field")).toString(),
                     optionalString(entry.value(QLatin1StringView("recipe"))),
                     optionalString(entry.value(QLatin1StringView("payload")))});
    }
    return list;
}

} // namespace

bool Drift::isSerious() const
{
    return field == QLatin1StringView("packageName") || field == QLatin1StringView("targetPath");
}

std::optional<Catalog> parseCatalog(const QByteArray& json, QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(errorMessage, QStringLiteral("Not valid JSON: %1 at offset %2.")
                                      .arg(parseError.errorString())
                                      .arg(parseError.offset));
    if (!document.isObject())
        return fail(errorMessage, QStringLiteral("The catalog is not a JSON object."));

    const QJsonObject root = document.object();
    const QString format = root.value(QLatin1StringView("format")).toString();
    if (format == QLatin1StringView("qgp-questports-payload"))
        return fail(errorMessage, QStringLiteral("This is a QuestPorts payload, not a qgp catalog. Build the "
                                                 "catalog from it with \"task catalog\"."));
    if (format != QLatin1StringView(kCatalogFormat))
        return fail(errorMessage, QStringLiteral("Not a qgp catalog (format \"%1\").").arg(format));
    const int version = root.value(QLatin1StringView("version")).toInt(-1);
    if (version != kCatalogVersion)
        return fail(errorMessage, QStringLiteral("Unsupported catalog version %1; this build reads version %2.")
                                      .arg(version)
                                      .arg(kCatalogVersion));

    const QJsonValue gamesValue = root.value(QLatin1StringView("games"));
    if (!gamesValue.isArray())
        return fail(errorMessage, QStringLiteral("The catalog has no games list."));

    Catalog catalog;
    catalog.generatedAt = timestamp(root.value(QLatin1StringView("generatedAt")));
    catalog.payloadGeneratedAt = timestamp(root.value(QLatin1StringView("sources"))
                                               .toObject()
                                               .value(QLatin1StringView("payload"))
                                               .toObject()
                                               .value(QLatin1StringView("generatedAt")));

    const QJsonArray games = gamesValue.toArray();
    catalog.games.reserve(games.size());
    for (qsizetype i = 0; i < games.size(); ++i) {
        const QJsonObject entry = games.at(i).toObject();
        Game game;
        game.slug = entry.value(QLatin1StringView("slug")).toString();
        game.port = entry.value(QLatin1StringView("port")).toObject();
        game.install = entry.value(QLatin1StringView("install")).toObject();
        game.campaigns = entry.value(QLatin1StringView("campaigns")).toArray();
        game.resolved = entry.value(QLatin1StringView("resolved")).toObject();
        game.recipe = entry.value(QLatin1StringView("recipe")).toObject();
        game.drift = driftList(entry.value(QLatin1StringView("drift")).toArray());
        game.title = game.port.value(QLatin1StringView("title")).toString();
        const QString cover = game.port.value(QLatin1StringView("cover_image_url")).toString();
        if (!cover.isEmpty())
            game.coverUrl = QUrl(cover);
        game.developer = game.port.value(QLatin1StringView("developer")).toString();
        game.addedAt = timestamp(game.port.value(QLatin1StringView("created_at")));
        game.releasedAt = timestamp(game.port.value(QLatin1StringView("last_github_update")));

        if (game.slug.isEmpty())
            return fail(errorMessage, QStringLiteral("Game %1 has no slug.").arg(i));
        if (game.title.isEmpty())
            return fail(errorMessage, QStringLiteral("Game \"%1\" has no title.").arg(game.slug));
        catalog.games.append(std::move(game));
    }
    return catalog;
}

std::optional<Catalog> loadCatalog(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorMessage, QStringLiteral("Cannot read %1: %2").arg(path, file.errorString()));
    return parseCatalog(file.readAll(), errorMessage);
}

} // namespace questports
