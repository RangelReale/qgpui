#include "installer/PushPlan.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace installer {

namespace {

bool hasWildcard(const QString& s)
{
    return s.contains(u'*') || s.contains(u'?');
}

// * and ? match within one path segment; ** matches across segments.
QRegularExpression globToRegex(const QString& glob)
{
    QString pattern;
    for (qsizetype i = 0; i < glob.size(); ++i) {
        const QChar c = glob.at(i);
        if (c == u'*') {
            if (i + 1 < glob.size() && glob.at(i + 1) == u'*') {
                pattern += QStringLiteral(".*");
                ++i;
            } else {
                pattern += QStringLiteral("[^/]*");
            }
        } else if (c == u'?') {
            pattern += QStringLiteral("[^/]");
        } else {
            pattern += QRegularExpression::escape(QString(c));
        }
    }
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern),
                              QRegularExpression::CaseInsensitiveOption);
}

// The folders of a glob before its first wildcard, with a trailing /; empty when it has none.
QString fixedPrefix(const QString& glob)
{
    const QStringList segments = glob.split(u'/');
    QString prefix;
    for (const QString& segment : segments) {
        if (hasWildcard(segment))
            break;
        prefix += segment + u'/';
    }
    // A glob without wildcards names one file: its prefix is its folder.
    if (!hasWildcard(glob))
        prefix = glob.contains(u'/') ? glob.left(glob.lastIndexOf(u'/') + 1) : QString();
    return prefix;
}

// Every file below folder, as paths relative to it with / separators.
QStringList filesBelow(const QString& folder)
{
    QStringList files;
    if (folder.isEmpty())
        return files;
    const QDir base(folder);
    QDirIterator it(folder, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        // The guide that the app writes into the user's files folder (gamestores::FilesFolder).
        if (it.fileName() != QLatin1StringView("qgp - what goes here.txt"))
            files.append(base.relativeFilePath(path));
    }
    files.sort(Qt::CaseInsensitive);
    return files;
}

bool targetsFolder(const QString& to)
{
    return to.endsWith(u'/');
}

} // namespace

bool planPush(const Step& step, const Input& input, const InputSource& source, PushPlan& plan, QString* errorMessage)
{
    // (local path, path relative to the input) for every file the input offers.
    QList<QPair<QString, QString>> candidates;
    if (input.kind == Input::Kind::PcInstall) {
        for (const QString& relative : filesBelow(source.folder))
            candidates.append({QDir(source.folder).filePath(relative), relative});
    } else {
        for (const QString& file : source.files)
            candidates.append({file, QFileInfo(file).fileName()});
    }

    for (const PushMapping& mapping : step.map) {
        QList<PushFile> matched;
        if (mapping.from.isEmpty()) {
            for (const auto& [local, relative] : std::as_const(candidates))
                matched.append({local, targetsFolder(mapping.to) ? mapping.to + relative : mapping.to});
        } else if (!hasWildcard(mapping.from)) {
            // A name or a fixed relative path. The recipe's spelling is kept on the headset,
            // because the games there are case sensitive.
            for (const auto& [local, relative] : std::as_const(candidates)) {
                if (relative.compare(mapping.from, Qt::CaseInsensitive) == 0) {
                    const QString name = mapping.from.mid(mapping.from.lastIndexOf(u'/') + 1);
                    matched.append({local, targetsFolder(mapping.to) ? mapping.to + name : mapping.to});
                }
            }
        } else {
            const QRegularExpression regex = globToRegex(mapping.from);
            const QString prefix = fixedPrefix(mapping.from);
            for (const auto& [local, relative] : std::as_const(candidates)) {
                if (!regex.match(relative).hasMatch())
                    continue;
                const QString kept = relative.mid(prefix.size());
                matched.append({local, targetsFolder(mapping.to) ? mapping.to + kept : mapping.to});
            }
        }

        if (matched.isEmpty()) {
            plan.unmatched.append(QStringLiteral("%1 -> %2").arg(mapping.from.isEmpty() ? QStringLiteral("*") : mapping.from, mapping.to));
            continue;
        }
        if (!targetsFolder(mapping.to) && matched.size() > 1) {
            if (errorMessage)
                *errorMessage = QStringLiteral("%1 files match \"%2\", but %3 names one file.")
                                    .arg(matched.size()).arg(mapping.from, mapping.to);
            return false;
        }
        plan.files += matched;
    }
    return true;
}

bool isAppPrivatePath(const QString& remotePath)
{
    static const QRegularExpression re(QStringLiteral("^/sdcard/Android/(data|obb)/[^/]+/"));
    return re.match(remotePath).hasMatch();
}

QStringList appPrivateFolders(const QList<PushFile>& files)
{
    static const QRegularExpression re(QStringLiteral("^(/sdcard/Android/(?:data|obb)/[^/]+)/(.+)/[^/]+$"));
    QStringList folders;
    for (const PushFile& file : files) {
        const auto match = re.match(file.remotePath);
        if (!match.hasMatch())
            continue;
        QString folder = match.captured(1);
        for (const QString& segment : match.captured(2).split(u'/', Qt::SkipEmptyParts)) {
            folder += u'/' + segment;
            if (!folders.contains(folder))
                folders.append(folder);
        }
    }
    // Parents before children: a folder sorts before every path it is a prefix of.
    std::sort(folders.begin(), folders.end());
    return folders;
}

} // namespace installer
