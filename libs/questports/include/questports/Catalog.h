#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace questports {

// The payload format this library reads. tools/questports-payload writes it; its README describes
// the fields.
inline constexpr auto kPayloadFormat = "qgp-questports-payload";
inline constexpr int kPayloadVersion = 1;

// One game: a row of the QuestPorts ports table joined with its install requirements.
struct Game
{
    QString slug;
    QString title;
    QUrl coverUrl; // Empty when the row has no cover.

    QJsonObject port;     // The database row, unchanged.
    QJsonObject install;  // PORT_PACKAGE_CONFIGS[slug]; empty when the game has none.
    QJsonArray campaigns; // PORT_EXPANSIONS[slug]; empty when the game has none.
    QJsonObject resolved; // Values computed by the site's functions.

    bool hasInstall() const { return !install.isEmpty(); }
};

struct Catalog
{
    QDateTime generatedAt;
    QList<Game> games; // In payload order: by title, then slug.
};

// Reads a payload. Returns nothing, and sets *errorMessage when it is given, if the data is not a
// payload of kPayloadFormat and kPayloadVersion or a game has no slug or title.
std::optional<Catalog> parseCatalog(const QByteArray& json, QString* errorMessage = nullptr);

// Reads a payload file. Fails as parseCatalog does, and when the file cannot be read.
std::optional<Catalog> loadCatalog(const QString& path, QString* errorMessage = nullptr);

} // namespace questports
