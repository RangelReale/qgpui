#include "gamestores/FilesFolder.h"

#include "gamestores/Finder.h"

#include "SafePath.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace gamestores {

namespace {

QString clean(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QStringList stringList(const QJsonValue& value)
{
    QStringList list;
    for (const QJsonValue& item : value.toArray())
        list.append(item.toString());
    return list;
}

bool isBelow(const QString& path, const QString& folder)
{
    return path.startsWith(folder + u'/', Qt::CaseInsensitive);
}

bool isBelowAny(const QString& path, const QStringList& folders)
{
    for (const QString& folder : folders) {
        if (isBelow(path, folder))
            return true;
    }
    return false;
}

// Every file below folder, sorted, leaving out the files below the skipped folders.
QStringList filesBelow(const QString& folder, const QStringList& skip)
{
    QStringList files;
    if (!QFileInfo(folder).isDir())
        return files;
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = clean(it.next());
        if (!isBelowAny(path, skip) && QFileInfo(path).fileName() != FilesFolder::kGuideFileName)
            files.append(path);
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}

// The folders below folder, down to depth levels, shallowest first, leaving out the skipped ones.
QStringList foldersBelow(const QString& folder, int depth, const QStringList& skip)
{
    QStringList result;
    QStringList level{folder};
    for (int i = 0; i < depth; ++i) {
        QStringList next;
        for (const QString& base : std::as_const(level)) {
            const QStringList names =
                QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name | QDir::IgnoreCase);
            for (const QString& name : names) {
                const QString path = clean(QDir(base).filePath(name));
                if (!skip.contains(path, Qt::CaseInsensitive))
                    next.append(path);
            }
        }
        result += next;
        level = next;
    }
    return result;
}

bool hasExtension(const QString& path, const QStringList& extensions)
{
    const QString name = QFileInfo(path).fileName();
    for (const QString& extension : extensions) {
        if (name.endsWith(extension, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

FilesMatch matchFolder(const QJsonObject& input, const QString& base, const QStringList& skip)
{
    FilesMatch match;
    const QString root = input.value(QLatin1StringView("root")).toString();
    const QStringList detect = stringList(input.value(QLatin1StringView("detect")));

    QStringList candidates{base};
    // Without detect files, any folder would match; only the base folder counts then.
    if (!detect.isEmpty())
        candidates += foldersBelow(base, 2, skip);

    for (const QString& candidate : std::as_const(candidates)) {
        const QString folder = clean(GameFinder::applyRoot(candidate, root));
        if (GameFinder::matchesDetect(folder, detect) && !match.paths.contains(folder, Qt::CaseInsensitive))
            match.paths.append(folder);
    }
    if (match.paths.size() == 1)
        match.status = FilesMatch::Status::Found;
    else if (match.paths.size() > 1)
        match.status = FilesMatch::Status::Ambiguous;
    return match;
}

FilesMatch matchNames(const QStringList& names, const QStringList& areaFiles, const QStringList& otherFiles)
{
    FilesMatch match;
    for (const QString& name : names) {
        QString found;
        for (const QStringList* files : {&areaFiles, &otherFiles}) {
            for (const QString& path : *files) {
                if (QFileInfo(path).fileName().compare(name, Qt::CaseInsensitive) == 0) {
                    found = path;
                    break;
                }
            }
            if (!found.isEmpty())
                break;
        }
        if (found.isEmpty())
            match.missing.append(name);
        else
            match.paths.append(found);
    }
    if (match.paths.isEmpty())
        match.status = FilesMatch::Status::NotFound;
    else if (match.missing.isEmpty())
        match.status = FilesMatch::Status::Found;
    else
        match.status = FilesMatch::Status::Partial;
    return match;
}

} // namespace

FilesFolder::FilesFolder(QString root)
    : m_root(clean(std::move(root)))
{}

QString FilesFolder::gameFolder(const QString& slug) const
{
    return m_root + u'/' + slug;
}

QString FilesFolder::inputFolder(const QString& slug, const QString& inputId) const
{
    return gameFolder(slug) + u'/' + inputId;
}

FilesMatch FilesFolder::find(const QString& slug, const QJsonArray& inputs, const QString& inputId) const
{
    QJsonObject input;
    QList<QJsonObject> others;
    for (const QJsonValue& value : inputs) {
        const QJsonObject object = value.toObject();
        if (object.value(QLatin1StringView("id")).toString() == inputId)
            input = object;
        else
            others.append(object);
    }
    if (input.isEmpty() || m_root.isEmpty() || !detail::isPlainName(slug) || !detail::isPlainName(inputId))
        return {};

    // Steps 1 and 2: the input folder alone, or the game folder without the other inputs' folders.
    const QString ownFolder = inputFolder(slug, inputId);
    const bool inOwnFolder = QFileInfo(ownFolder).isDir();
    const QString area = inOwnFolder ? ownFolder : gameFolder(slug);
    QStringList skip;
    if (!inOwnFolder) {
        for (const QJsonObject& other : std::as_const(others)) {
            const QString id = other.value(QLatin1StringView("id")).toString();
            if (detail::isPlainName(id))
                skip.append(inputFolder(slug, id));
        }
    }

    if (input.value(QLatin1StringView("kind")).toString() == QLatin1StringView("pc-install")) {
        if (!QFileInfo(area).isDir())
            return {};
        return matchFolder(input, area, skip);
    }

    const QStringList areaFiles = filesBelow(area, skip);

    const QStringList names = stringList(input.value(QLatin1StringView("names")));
    if (!names.isEmpty()) {
        // Step 3: the rest of the files folder, for names the game's own folders lack.
        return matchNames(names, areaFiles, filesBelow(m_root, {area}));
    }

    const QStringList extensions = stringList(input.value(QLatin1StringView("extensions")));
    FilesMatch match;
    for (const QString& path : areaFiles) {
        if (hasExtension(path, extensions))
            match.paths.append(path);
    }
    if (match.paths.isEmpty())
        return match;

    const bool shared = !inOwnFolder && inputsNeedingFolders(inputs).contains(inputId);
    const bool multiple = input.value(QLatin1StringView("multiple")).toBool();
    match.status = (shared || (!multiple && match.paths.size() > 1)) ? FilesMatch::Status::Ambiguous
                                                                      : FilesMatch::Status::Found;
    return match;
}

QStringList FilesFolder::inputsNeedingFolders(const QJsonArray& inputs)
{
    QStringList ids;
    for (qsizetype i = 0; i < inputs.size(); ++i) {
        const QJsonObject input = inputs.at(i).toObject();
        const QStringList extensions = stringList(input.value(QLatin1StringView("extensions")));
        for (qsizetype j = 0; j < inputs.size(); ++j) {
            if (i == j)
                continue;
            const QStringList others = stringList(inputs.at(j).toObject().value(QLatin1StringView("extensions")));
            const bool overlaps = std::any_of(extensions.cbegin(), extensions.cend(), [&](const QString& extension) {
                return others.contains(extension, Qt::CaseInsensitive);
            });
            if (overlaps) {
                ids.append(input.value(QLatin1StringView("id")).toString());
                break;
            }
        }
    }
    return ids;
}

namespace {

QString joinWith(const QStringList& items, const QString& last)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.mid(0, items.size() - 1).join(QStringLiteral(", ")) + u' ' + last + u' ' + items.last();
}

QString indented(const QString& text)
{
    return QStringLiteral("    ") + text + u'\n';
}

} // namespace

QStringList FilesFolder::inputGuide(const QJsonArray& inputs, int index, const QString& slug) const
{
    if (index < 0 || index >= inputs.size())
        return {};
    const QJsonObject input = inputs.at(index).toObject();
    const QString id = input.value(QLatin1StringView("id")).toString();
    QStringList lines;

    const QString help = input.value(QLatin1StringView("help")).toString();
    if (!help.isEmpty())
        lines.append(help);

    QString where;
    if (m_root.isEmpty() && !slug.isEmpty()) {
        where = QStringLiteral("Needed");
    } else if (slug.isEmpty()) {
        where = inputsNeedingFolders(inputs).contains(id) ? QStringLiteral("Put in the folder \"%1\"").arg(id)
                                                          : QStringLiteral("Put here");
    } else {
        const QString folder = inputsNeedingFolders(inputs).contains(id) ? slug + u'/' + id : slug;
        where = QStringLiteral("Put in the folder \"%1\"").arg(QDir::toNativeSeparators(folder));
    }
    const QStringList names = stringList(input.value(QLatin1StringView("names")));
    const QStringList extensions = stringList(input.value(QLatin1StringView("extensions")));
    if (input.value(QLatin1StringView("kind")).toString() == QLatin1StringView("pc-install")) {
        const QString root = input.value(QLatin1StringView("root")).toString();
        const QStringList detect = stringList(input.value(QLatin1StringView("detect")));
        // A folder that a PC tool makes is not a game install: name the tool instead.
        const QString tool =
            input.value(QLatin1StringView("preparedWith")).toObject().value(QLatin1StringView("tool")).toString();
        const QString folder = tool.isEmpty() ? QStringLiteral("the game's folder")
                                              : QStringLiteral("the folder that %1 makes").arg(tool);
        QString line = root.isEmpty()
                           ? QStringLiteral("%1: %2, or a copy of it").arg(where, folder)
                           : QStringLiteral("%1: %2, or the folder \"%3\" inside it").arg(where, folder, root);
        if (!detect.isEmpty())
            line += QStringLiteral(". QGPUI recognises it by %1").arg(joinWith(detect, QStringLiteral("and")));
        lines.append(line + u'.');
    } else if (!names.isEmpty()) {
        lines.append(QStringLiteral("%1: %2.").arg(where, joinWith(names, QStringLiteral("and"))));
    } else if (!extensions.isEmpty()) {
        const QString endings = joinWith(extensions, QStringLiteral("or"));
        lines.append(input.value(QLatin1StringView("multiple")).toBool()
                         ? QStringLiteral("%1: the files ending in %2.").arg(where, endings)
                         : QStringLiteral("%1: one file ending in %2.").arg(where, endings));
    }

    if (input.value(QLatin1StringView("keepArchived")).toBool())
        lines.append(QStringLiteral("Keep zip files zipped."));
    const QJsonObject tool = input.value(QLatin1StringView("preparedWith")).toObject();
    if (!tool.isEmpty()) {
        lines.append(QStringLiteral("Made with %1: %2")
                         .arg(tool.value(QLatin1StringView("tool")).toString(),
                              tool.value(QLatin1StringView("url")).toString()));
        const QString toolHelp = tool.value(QLatin1StringView("help")).toString();
        if (!toolHelp.isEmpty())
            lines.append(toolHelp);
    }
    return lines;
}

QString FilesFolder::sharedFilesNote(const QJsonArray& inputs) const
{
    const bool anyNames = std::any_of(inputs.begin(), inputs.end(), [](const QJsonValue& value) {
        const QJsonObject input = value.toObject();
        return input.value(QLatin1StringView("kind")).toString() != QLatin1StringView("pc-install") &&
               !stringList(input.value(QLatin1StringView("names"))).isEmpty();
    });
    if (!anyNames || m_root.isEmpty())
        return {};
    return QStringLiteral("QGPUI also finds files with these exact names anywhere in %1. "
                          "A file that several games use, such as naomi.zip, is needed only once.")
        .arg(QDir::toNativeSeparators(m_root));
}

QString FilesFolder::guideText(const QString& title, const QJsonArray& inputs) const
{
    QString text = title + u'\n' + QString(title.size(), u'=') + QStringLiteral("\n\n");
    text += QStringLiteral("QGPUI looks in this folder for the files below when it installs the game. "
                           "It also looks in the folders inside this one.\n");

    for (int i = 0; i < inputs.size(); ++i) {
        const QJsonObject input = inputs.at(i).toObject();
        const bool optional = !input.value(QLatin1StringView("required")).toBool(true);
        text += QStringLiteral("\n%1. %2%3\n")
                    .arg(QString::number(i + 1), input.value(QLatin1StringView("label")).toString(),
                         optional ? QStringLiteral(" (optional)") : QString());
        for (const QString& line : inputGuide(inputs, i))
            text += indented(line);
    }

    const QString shared = sharedFilesNote(inputs);
    if (!shared.isEmpty())
        text += u'\n' + shared + u'\n';
    text += QStringLiteral("\nqgpui wrote this file and rewrites it each time you create the folder. "
                           "QGPUI ignores it when it looks for files, and never copies it to the headset.\n");
    return text;
}

bool FilesFolder::create(const QString& slug, const QString& title, const QJsonArray& inputs,
                         QString* errorMessage) const
{
    // The slug and the ids come from the catalog: refuse any that would name another folder.
    if (m_root.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("No files folder is set.");
        return false;
    }
    if (!detail::isPlainName(slug)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("\"%1\" cannot be a folder name.").arg(slug);
        return false;
    }
    QStringList folders{gameFolder(slug)};
    for (const QString& id : inputsNeedingFolders(inputs)) {
        if (!detail::isPlainName(id)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("The input id \"%1\" cannot be a folder name.").arg(id);
            return false;
        }
        folders.append(inputFolder(slug, id));
    }
    for (const QString& folder : std::as_const(folders)) {
        if (!QDir().mkpath(folder)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Cannot create the folder %1.").arg(QDir::toNativeSeparators(folder));
            return false;
        }
    }

    // A game that asks for files always gets the guide, rewritten so it follows the recipe.
    if (inputs.isEmpty())
        return true;

    const QString path = gameFolder(slug) + u'/' + kGuideFileName;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text) ||
        file.write(guideText(title, inputs).toUtf8()) < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
        return false;
    }
    return true;
}

} // namespace gamestores
