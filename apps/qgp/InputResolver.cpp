#include "InputResolver.h"

#include "gamestores/FilesFolder.h"
#include "gamestores/Finder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <utility>

namespace {

QString tr(const char* text)
{
    return QCoreApplication::translate("InputResolver", text);
}

QString storeName(const QString& store)
{
    if (store == QLatin1StringView("steam"))
        return QStringLiteral("Steam");
    if (store == QLatin1StringView("gog"))
        return QStringLiteral("GOG");
    return store;
}

QStringList cleaned(const QStringList& paths)
{
    QStringList result;
    for (const QString& path : paths)
        result.append(QDir::cleanPath(QDir::fromNativeSeparators(path)));
    return result;
}

} // namespace

InputResolver::InputResolver(const gamestores::GameFinder* finder, QString filesRoot)
    : m_finder(finder)
    , m_filesRoot(std::move(filesRoot))
{}

QList<ResolvedInput> InputResolver::resolve(const QString& slug, const installer::Recipe& recipe) const
{
    QJsonArray inputs;
    for (const installer::Input& input : recipe.inputs)
        inputs.append(input.json);
    const gamestores::FilesFolder files(m_filesRoot);

    QList<ResolvedInput> result;
    for (const installer::Input& input : recipe.inputs) {
        ResolvedInput resolved;
        resolved.input = input;

        if (input.kind == installer::Input::Kind::PcInstall && m_finder) {
            for (const gamestores::Location& location : m_finder->find(input.json)) {
                if (location.status == gamestores::Location::Status::Installed) {
                    resolved.state = ResolvedInput::State::Found;
                    resolved.source.folder = location.path;
                    resolved.origin = storeName(location.store);
                    break;
                }
            }
            if (resolved.state == ResolvedInput::State::Found) {
                result.append(resolved);
                continue;
            }
        }

        const gamestores::FilesMatch match = files.find(slug, inputs, input.id);
        switch (match.status) {
        case gamestores::FilesMatch::Status::Found:
            resolved.state = ResolvedInput::State::Found;
            resolved.origin = tr("your files folder");
            if (input.kind == installer::Input::Kind::PcInstall)
                resolved.source.folder = match.paths.first();
            else
                resolved.source.files = match.paths;
            break;
        case gamestores::FilesMatch::Status::Ambiguous:
            resolved.state = ResolvedInput::State::Ambiguous;
            resolved.candidates = match.paths;
            break;
        case gamestores::FilesMatch::Status::Partial:
            resolved.partial = match.paths;
            resolved.missingNames = match.missing;
            break;
        case gamestores::FilesMatch::Status::NotFound:
            resolved.missingNames = input.names;
            break;
        }
        result.append(resolved);
    }
    return result;
}

bool InputResolver::chooseFolder(ResolvedInput& resolved, const QString& folder, QString* errorMessage)
{
    const QString rooted = gamestores::GameFinder::applyRoot(QDir::fromNativeSeparators(folder), resolved.input.root);
    if (!gamestores::GameFinder::matchesDetect(rooted, resolved.input.detect)) {
        if (errorMessage) {
            *errorMessage = tr("This is not the right folder. It must hold %1.")
                                .arg(resolved.input.detect.join(QStringLiteral(", ")));
        }
        return false;
    }
    resolved.state = ResolvedInput::State::Found;
    resolved.source = {rooted, {}};
    resolved.origin = tr("your choice");
    resolved.candidates.clear();
    return true;
}

bool InputResolver::chooseFiles(ResolvedInput& resolved, const QStringList& picked, QString* errorMessage)
{
    const installer::Input& input = resolved.input;
    const QStringList files = cleaned(picked);
    auto fail = [&](const QString& message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (files.isEmpty())
        return fail(tr("No file was chosen."));

    QStringList taken;
    if (!input.names.isEmpty()) {
        // Each name from the picked files, else from the files found before.
        QStringList missing;
        for (const QString& name : input.names) {
            QString found;
            for (const QStringList* list : {&files, &std::as_const(resolved.partial)}) {
                for (const QString& path : *list) {
                    if (QFileInfo(path).fileName().compare(name, Qt::CaseInsensitive) == 0) {
                        found = path;
                        break;
                    }
                }
                if (!found.isEmpty())
                    break;
            }
            if (found.isEmpty())
                missing.append(name);
            else
                taken.append(found);
        }
        if (!missing.isEmpty())
            return fail(tr("Still missing: %1.").arg(missing.join(QStringLiteral(", "))));
    } else {
        for (const QString& path : files) {
            const QString name = QFileInfo(path).fileName();
            const bool fits = std::any_of(input.extensions.cbegin(), input.extensions.cend(), [&](const QString& extension) {
                return name.endsWith(extension, Qt::CaseInsensitive);
            });
            if (!fits)
                return fail(tr("%1 does not end in %2.").arg(name, input.extensions.join(QStringLiteral(", "))));
        }
        if (!input.multiple && files.size() > 1)
            return fail(tr("Choose one file."));
        taken = files;
    }

    resolved.state = ResolvedInput::State::Found;
    resolved.source = {{}, taken};
    resolved.origin = tr("your choice");
    resolved.candidates.clear();
    resolved.partial.clear();
    resolved.missingNames.clear();
    return true;
}

QHash<QString, installer::InputSource> InputResolver::sources(const QList<ResolvedInput>& inputs)
{
    QHash<QString, installer::InputSource> result;
    for (const ResolvedInput& resolved : inputs) {
        if (resolved.state == ResolvedInput::State::Found)
            result.insert(resolved.input.id, resolved.source);
    }
    return result;
}

bool InputResolver::ready(const QList<ResolvedInput>& inputs)
{
    return std::all_of(inputs.cbegin(), inputs.cend(), [](const ResolvedInput& resolved) {
        return !resolved.input.required || resolved.state == ResolvedInput::State::Found;
    });
}
