#include "gamestores/FilesFolder.h"

#include "gamestores/Finder.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>

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
        if (!isBelowAny(path, skip))
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

QString FilesFolder::defaultRoot()
{
    return clean(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/qgp/files"));
}

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
    if (input.isEmpty() || m_root.isEmpty() || slug.isEmpty())
        return {};

    // Steps 1 and 2: the input folder alone, or the game folder without the other inputs' folders.
    const QString ownFolder = inputFolder(slug, inputId);
    const bool inOwnFolder = QFileInfo(ownFolder).isDir();
    const QString area = inOwnFolder ? ownFolder : gameFolder(slug);
    QStringList skip;
    if (!inOwnFolder) {
        for (const QJsonObject& other : std::as_const(others))
            skip.append(inputFolder(slug, other.value(QLatin1StringView("id")).toString()));
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

bool FilesFolder::create(const QString& slug, const QJsonArray& inputs, QString* errorMessage) const
{
    QStringList folders{gameFolder(slug)};
    for (const QString& id : inputsNeedingFolders(inputs))
        folders.append(inputFolder(slug, id));
    for (const QString& folder : std::as_const(folders)) {
        if (!QDir().mkpath(folder)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Cannot create the folder %1.").arg(QDir::toNativeSeparators(folder));
            return false;
        }
    }
    return true;
}

} // namespace gamestores
