#pragma once

#include "installer/PushPlan.h"
#include "installer/Recipe.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace gamestores {
class GameFinder;
}

// Where one recipe input's files come from.
struct ResolvedInput
{
    enum class State {
        Found,     // source is ready to use.
        Ambiguous, // Several candidates fit; the user chooses.
        Missing,   // Nothing found yet; partial holds names already found.
    };

    installer::Input input;
    State state = State::Missing;
    installer::InputSource source;
    QString origin;        // Where it was found, for the user: "Steam", "your files folder", "your choice".
    QStringList candidates; // Ambiguous: the files or folders that fit.
    QStringList partial;    // Missing file input with names: the files already found.
    QStringList missingNames;
};

// Finds a recipe's inputs: a pc-install input in the Steam and GOG libraries, then any input in the
// user's files folder (decision 0009). Also checks what the user picks by hand.
class InputResolver
{
public:
    InputResolver(const gamestores::GameFinder* finder, QString filesRoot);

    QList<ResolvedInput> resolve(const QString& slug, const installer::Recipe& recipe) const;

    // Takes the user's folder for a pc-install input. Returns false, and sets *errorMessage, when
    // the folder does not hold the input's detect files.
    static bool chooseFolder(ResolvedInput& resolved, const QString& folder, QString* errorMessage);
    // Takes the user's files for a file input. With names, the picked files and those already found
    // must cover every name; with extensions, each file needs one of them, and only one file is
    // taken without multiple. Returns false, and sets *errorMessage, otherwise.
    static bool chooseFiles(ResolvedInput& resolved, const QStringList& files, QString* errorMessage);

    // The inputs ready for installer::Runner. Missing optional inputs are left out.
    static QHash<QString, installer::InputSource> sources(const QList<ResolvedInput>& inputs);
    // True when every required input is Found, and at least one input of each group.
    static bool ready(const QList<ResolvedInput>& inputs);

private:
    const gamestores::GameFinder* m_finder;
    QString m_filesRoot;
};
