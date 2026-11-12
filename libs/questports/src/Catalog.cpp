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

} // namespace

std::optional<Catalog> parseCatalog(const QByteArray& json, QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(errorMessage, QStringLiteral("Not valid JSON: %1 at offset %2.")
                                      .arg(parseError.errorString())
                                      .arg(parseError.offset));
    if (!document.isObject())
        return fail(errorMessage, QStringLiteral("The payload is not a JSON object."));

    const QJsonObject root = document.object();
    const QString format = root.value(QLatin1StringView("format")).toString();
    if (format != QLatin1StringView(kPayloadFormat))
        return fail(errorMessage, QStringLiteral("Not a QuestPorts payload (format \"%1\").").arg(format));
    const int version = root.value(QLatin1StringView("version")).toInt(-1);
    if (version != kPayloadVersion)
        return fail(errorMessage, QStringLiteral("Unsupported payload version %1; this build reads version %2.")
                                      .arg(version)
                                      .arg(kPayloadVersion));

    const QJsonValue gamesValue = root.value(QLatin1StringView("games"));
    if (!gamesValue.isArray())
        return fail(errorMessage, QStringLiteral("The payload has no games list."));

    Catalog catalog;
    catalog.generatedAt = QDateTime::fromString(root.value(QLatin1StringView("generatedAt")).toString(),
                                                Qt::ISODateWithMs);

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
