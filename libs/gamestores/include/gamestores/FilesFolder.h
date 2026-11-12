#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QString>
#include <QStringList>

namespace gamestores {

// What the files folder holds for one recipe input.
struct FilesMatch
{
    enum class Status {
        NotFound,  // Nothing that fits.
        Found,     // paths holds the input's files (file) or its one folder (pc-install).
        Partial,   // A file input with names: paths holds the names found, missing the others.
        Ambiguous, // paths holds the candidates; the user chooses.
    };

    Status status = Status::NotFound;
    QStringList paths;   // Absolute paths with / separators.
    QStringList missing; // Partial: the names not found.
};

// The user's files folder (decision 0009): one folder per game slug, an optional folder per input
// below it, and shared files anywhere. qgp looks here for a recipe input's files before asking.
class FilesFolder
{
public:
    explicit FilesFolder(QString root);

    // <Documents>/qgp/files.
    static QString defaultRoot();

    QString root() const { return m_root; }
    QString gameFolder(const QString& slug) const;
    QString inputFolder(const QString& slug, const QString& inputId) const;

    // Looks for the input with ID inputId among inputs (a recipe's "inputs" array). In order:
    // 1. the input folder, <slug>/<inputId>/, when it exists; then only there;
    // 2. otherwise the game folder, <slug>/, and the folders below it, except other inputs' folders;
    // 3. for a file input with names, each name not found yet anywhere else in the files folder.
    FilesMatch find(const QString& slug, const QJsonArray& inputs, const QString& inputId) const;

    // The IDs of the inputs that need a folder of their own: those that accept an extension that
    // another input of the game also accepts, so the game folder cannot tell them apart.
    static QStringList inputsNeedingFolders(const QJsonArray& inputs);

    // The guide's file name. qgp writes it into a game folder and never finds or pushes it as an
    // input's file; installer::planPush skips the same name.
    static constexpr QLatin1StringView kGuideFileName{"qgp - what goes here.txt"};

    // A plain-text explanation of what goes into the game's folders: each input with its label,
    // help, where to put it and what qgp recognises it by.
    QString guideText(const QString& title, const QJsonArray& inputs) const;

    // Creates the game folder and the input folders that inputsNeedingFolders names, if they are
    // missing. When the game has inputs, it also writes the guide (kGuideFileName) into the game
    // folder, replacing an older one. Returns false, and sets *errorMessage when it is given, if a
    // folder or the guide cannot be written.
    bool create(const QString& slug, const QString& title, const QJsonArray& inputs,
                QString* errorMessage = nullptr) const;

private:
    QString m_root;
};

} // namespace gamestores
