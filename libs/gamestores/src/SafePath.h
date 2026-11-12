#pragma once

#include <QString>
#include <QStringList>

#include <algorithm>

namespace gamestores::detail {

// Recipes and slugs come from the catalog, which comes from outside QGPUI. These checks keep the
// names and paths taken from it inside the folder they are joined to.

// True for one folder or file name: not empty, not "." or "..", no separator, drive colon or
// control character.
inline bool isPlainName(const QString& name)
{
    if (name.isEmpty() || name == u'.' || name == QLatin1StringView(".."))
        return false;
    return std::none_of(name.cbegin(), name.cend(), [](QChar c) {
        return c == u'/' || c == u'\\' || c == u':' || c.unicode() < 0x20 || c.unicode() == 0x7f;
    });
}

// True for a path relative to a folder that cannot leave it: not absolute, no drive, no ".."
// segment and no control character. Either separator and wildcards are allowed.
inline bool isSafeRelativePath(const QString& path)
{
    if (path.isEmpty() || path.contains(u':'))
        return false;
    if (std::any_of(path.cbegin(), path.cend(), [](QChar c) { return c.unicode() < 0x20 || c.unicode() == 0x7f; }))
        return false;
    const QString normal = QString(path).replace(u'\\', u'/');
    return !normal.startsWith(u'/') && !normal.split(u'/').contains(QLatin1StringView(".."));
}

} // namespace gamestores::detail
