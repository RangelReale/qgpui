#pragma once

#include "installer/PushPlan.h"
#include "installer/Recipe.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace installer {

// What an install will do, for the user to read before starting it. The Runner builds its device
// commands with the functions below, so the plan shows the commands it runs.

struct PlannedStep
{
    QString title;
    // One action per line. Lines that start with "adb " are commands as the adb command-line tool
    // would take them; the others say what qgp does without one.
    QStringList lines;
};

struct InstallPlan
{
    QList<PlannedStep> steps;

    // The whole plan as text, with numbered steps and indented actions.
    QString toText() const;
};

// Plans an install of the recipe. apkFiles are the APKs on the PC when they are known; when it is
// empty, the plan says where each APK will come from instead. inputs are the user's files, as
// for the Runner; an input that is missing shows as such.
InstallPlan planInstall(const Recipe& recipe, const QStringList& apkFiles, const QHash<QString, InputSource>& inputs);

// Where an APK comes from, in one sentence.
QString describeApkSource(const ApkSource& source);

// The device shell commands the Runner uses.
QStringList grantCommand(const QString& packageName, const Step& step);
QStringList launchCommand(const QString& packageName);
QStringList forceStopCommand(const QString& packageName);
// Creates each missing folder and opens it to the app. quotedFolders: shell-quoted folders, each
// preceded by a space, parents first.
QString makeFoldersScript(const QString& quotedFolders);

} // namespace installer
