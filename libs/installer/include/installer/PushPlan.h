#pragma once

#include "installer/Recipe.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace installer {

// What the user supplied for one input.
struct InputSource
{
    QString folder;    // pc-install: the folder, with the recipe's root already applied.
    QStringList files; // file: the files the user picked, as absolute paths.
};

struct PushFile
{
    QString localPath;
    QString remotePath;
};

struct PushPlan
{
    QList<PushFile> files;
    QStringList unmatched; // Mappings whose "from" matched no file, as "from -> to".
};

// The copies one push step makes. A mapping's "from" is a name from the input's names, or a glob
// relative to the input's folder (pc-install) or matched against file names (file): * and ? stay
// within one folder, ** crosses folders. A glob drops the folders before its first wildcard and
// keeps the rest of each path below a "to" that ends in /. Without "from", every file of the input
// is copied, keeping paths below the folder. Matching ignores case. Returns false, and sets
// *errorMessage when it is given, when a "to" that names one file would receive several.
bool planPush(const Step& step, const Input& input, const InputSource& source, PushPlan& plan,
              QString* errorMessage = nullptr);

// True for paths in an app's private folders on shared storage (/sdcard/Android/data/<package>/
// and /sdcard/Android/obb/<package>/), where adb cannot create nested folders and the folders it
// creates are closed to the app.
bool isAppPrivatePath(const QString& remotePath);

// The folders that the copies need inside app-private paths, parents first, from the first level
// below <package>/ down. The runner creates the missing ones with mkdir and opens them to the app.
QStringList appPrivateFolders(const QList<PushFile>& files);

} // namespace installer
