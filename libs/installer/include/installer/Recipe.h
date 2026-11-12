#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace installer {

// An install recipe as qgp runs it. The JSON format is data/recipe.schema.json, described in
// docs/architecture/install-recipes.md; build-catalog has already checked it, so parsing only rejects what the
// runner cannot work with.

enum class Support { Automatic, Guided, Manual };

struct ApkSource
{
    enum class Kind { GitHub, Url, Manual };
    Kind kind = Kind::GitHub;
    QString repo;        // GitHub: owner/name.
    QString tag;         // GitHub: a regular expression for the release tag; empty for any.
    bool prerelease = false;
    QString asset;       // GitHub and Manual: a regular expression for the file name.
    QString url;         // Url.
    QString page;        // Manual: where the user downloads it.
    QString help;        // Manual.
    QString inZip;       // GitHub and Url: the download is a zip; the APK's path matches this.
    QString packageName; // Optional: the APK's package, when it differs from the game's.
};

struct Input
{
    enum class Kind { PcInstall, File };
    Kind kind = Kind::File;
    QString id;
    QString label;
    QString help;
    bool required = true;
    // File inputs.
    QStringList names;
    QStringList extensions;
    bool multiple = false;
    bool keepArchived = false;
    // pc-install inputs.
    QString root;
    QStringList detect;
    QJsonObject json; // The input as written, for gamestores::GameFinder::find().
};

struct PushMapping
{
    QString from; // Empty: every file of the input.
    QString to;   // Under /sdcard/. Ends in / for a folder.
};

struct Step
{
    enum class Kind { InstallApk, Grant, Launch, Push, Verify, Shell, Tell };
    Kind kind = Kind::Tell;
    bool grantAll = false;      // InstallApk: install with -g.
    QString appop;              // Grant.
    QString permission;         // Grant.
    int waitSeconds = 10;       // Launch.
    bool stop = true;           // Launch: force-stop afterwards.
    QString input;              // Push.
    QList<PushMapping> map;     // Push.
    QStringList files;          // Verify.
    QStringList args;           // Shell.
    QString text;               // Tell.
    QString why;                // Grant, Launch, Verify, Shell.
};

struct Recipe
{
    QString status; // "draft", "reviewed" or "tested".
    Support support = Support::Automatic;
    QString packageName; // The game's main package.
    qint64 freeSpaceMB = 0;
    QList<ApkSource> apks;
    QList<Input> inputs;
    QList<Step> steps;

    const Input* input(const QString& id) const;
};

// Reads a recipe object, as the catalog's games carry it. defaultPackageName is the payload's
// install.packageName, used unless the recipe overrides it. Returns nothing, and sets
// *errorMessage when it is given, for an unknown step, source or input kind, or a push of an
// input the recipe does not have.
std::optional<Recipe> parseRecipe(const QJsonObject& json, const QString& defaultPackageName,
                                  QString* errorMessage = nullptr);

// A short description of a step for progress displays, such as "Push melee to /sdcard/Download/".
QString describeStep(const Step& step);

} // namespace installer
