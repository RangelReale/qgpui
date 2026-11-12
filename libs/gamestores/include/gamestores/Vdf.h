#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace gamestores {

// A KeyValues tree, the text format of Valve's .vdf and .acf files: each key holds a string or a
// nested object. Keys are stored in lower case, because KeyValues compares keys without regard to
// case ("AppState" and "appstate" are the same key). A key that appears twice keeps its last value.
using VdfObject = QVariantMap;

// Parses KeyValues text (UTF-8). Handles quoted and unquoted tokens, the escapes \\ \" \n \t,
// // comments and conditionals such as [$WIN32], which are ignored. Returns nothing, and sets
// *errorMessage when it is given, on an unterminated string or unbalanced braces.
std::optional<VdfObject> parseVdf(const QByteArray& text, QString* errorMessage = nullptr);

// Follows path (keys in any case) from object. Returns the string there, or an empty string when a
// key is missing or the value is an object.
QString vdfString(const VdfObject& object, const QStringList& path);

// Follows path from object. Returns the object there, or an empty object.
VdfObject vdfObject(const VdfObject& object, const QStringList& path);

} // namespace gamestores
