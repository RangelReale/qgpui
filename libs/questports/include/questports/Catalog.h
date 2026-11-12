#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace questports {

// The catalog format this library reads. tools/questports-payload's build-catalog command writes
// it; the tool's README describes the fields.
inline constexpr auto kCatalogFormat = "qgp-catalog";
inline constexpr int kCatalogVersion = 1;

// One payload value that changed since the game's recipe was written.
struct Drift
{
    QString field;   // "packageName", "version" or "targetPath".
    QString recipe;  // The value the recipe was written against; null when unset.
    QString payload; // The payload's value now; null when unset.

    // A change that can make the recipe install the wrong app or to the wrong place.
    bool isSerious() const;
};

// One game: a row of the QuestPorts ports table joined with its install requirements and, when
// there is one, qgp's install recipe.
struct Game
{
    QString slug;
    QString title;
    QUrl coverUrl; // Empty when the row has no cover.
    QString developer;
    QDateTime addedAt;    // When the game was added to QuestPorts (created_at).
    QDateTime releasedAt; // The port's latest GitHub release (last_github_update); invalid when unknown.

    QJsonObject port;     // The database row, unchanged.
    QJsonObject install;  // PORT_PACKAGE_CONFIGS[slug]; empty when the game has none.
    QJsonArray campaigns; // PORT_EXPANSIONS[slug]; empty when the game has none.
    QJsonObject resolved; // Values computed by the site's functions.
    QString originPlatform; // resolved.originPlatform, such as "pc" or "n64"; empty when unknown.
    QJsonObject recipe;   // qgp's install recipe (data/recipes/<slug>.json); empty when there is none.
    QList<Drift> drift;   // Payload values that changed since the recipe was written.

    bool hasInstall() const { return !install.isEmpty(); }
    bool hasRecipe() const { return !recipe.isEmpty(); }
};

// A source platform of the site's catalog filter ("pc", "PC").
struct OriginPlatform
{
    QString id;
    QString label;
};

struct Catalog
{
    QDateTime generatedAt;        // When the catalog was built.
    QDateTime payloadGeneratedAt; // When the payload it was built from was generated.
    QList<Game> games;            // In payload order: by title, then slug.
    QList<OriginPlatform> originPlatforms; // In the site's order; empty in older catalogs.
};

// Reads a catalog. Returns nothing, and sets *errorMessage when it is given, if the data is not a
// catalog of kCatalogFormat and kCatalogVersion or a game has no slug or title.
std::optional<Catalog> parseCatalog(const QByteArray& json, QString* errorMessage = nullptr);

// Reads a catalog file. Fails as parseCatalog does, and when the file cannot be read.
std::optional<Catalog> loadCatalog(const QString& path, QString* errorMessage = nullptr);

} // namespace questports
