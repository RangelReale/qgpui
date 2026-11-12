#include "installer/InstallPlan.h"

#include <qadb/Protocol.h>

#include <QDir>
#include <QFileInfo>

namespace installer {

namespace {

QString localPath(const QString& path)
{
    return u'"' + QDir::toNativeSeparators(path) + u'"';
}

QString adbShell(const QStringList& arguments)
{
    return QStringLiteral("adb shell ") + qadb::protocol::shellCommand(arguments);
}

void planPushStep(const Recipe& recipe, const Step& step, const QHash<QString, InputSource>& inputs,
                  QStringList& lines)
{
    const Input* input = recipe.input(step.input);
    const QString label = input ? input->label : step.input;
    const auto source = inputs.constFind(step.input);
    if (source == inputs.cend()) {
        if (input && !input->required)
            lines.append(QStringLiteral("Skipped unless you give %1.").arg(label));
        else
            lines.append(QStringLiteral("%1 has not been found yet. Choose it to see the copies.").arg(label));
        for (const PushMapping& m : step.map)
            lines.append(QStringLiteral("Copies %1 to %2")
                             .arg(m.from.isEmpty() ? QStringLiteral("every file") : m.from, m.to));
        return;
    }
    if (!source->folder.isEmpty())
        lines.append(QStringLiteral("From %1").arg(QDir::toNativeSeparators(source->folder)));

    PushPlan plan;
    QString error;
    if (!planPush(step, *input, *source, plan, &error)) {
        lines.append(QStringLiteral("This step will fail: %1").arg(error));
        return;
    }
    for (const QString& unmatched : std::as_const(plan.unmatched))
        lines.append(QStringLiteral("No file matches %1.").arg(unmatched));
    if (plan.files.isEmpty()) {
        lines.append(QStringLiteral("Nothing to copy, so this step will fail."));
        return;
    }
    for (const QString& folder : appPrivateFolders(plan.files)) {
        const QString quoted = qadb::protocol::shellQuote(folder);
        lines.append(QStringLiteral("adb shell \"[ -d %1 ] || { mkdir %1 && chmod 2777 %1; }\"").arg(quoted));
    }
    for (const PushFile& file : std::as_const(plan.files)) {
        const QString mode = isAppPrivatePath(file.remotePath) ? QStringLiteral("0666") : QStringLiteral("0644");
        lines.append(QStringLiteral("adb push %1 %2  (mode %3)")
                         .arg(localPath(file.localPath), qadb::protocol::shellQuote(file.remotePath), mode));
    }
}

} // namespace

QString InstallPlan::toText() const
{
    QString text;
    for (qsizetype i = 0; i < steps.size(); ++i) {
        const PlannedStep& step = steps.at(i);
        if (i > 0)
            text += u'\n';
        text += QStringLiteral("%1. %2\n").arg(i + 1).arg(step.title);
        for (const QString& line : step.lines)
            text += QStringLiteral("    ") + line + u'\n';
    }
    return text;
}

QString describeApkSource(const ApkSource& source)
{
    QString text;
    switch (source.kind) {
    case ApkSource::Kind::GitHub:
        text = QStringLiteral("Download the newest %1release of https://github.com/%2")
                   .arg(source.prerelease ? QStringLiteral("") : QStringLiteral("stable "), source.repo);
        if (!source.tag.isEmpty())
            text += QStringLiteral(" whose tag matches %1").arg(source.tag);
        if (!source.asset.isEmpty())
            text += QStringLiteral(", the file matching %1").arg(source.asset);
        break;
    case ApkSource::Kind::Url:
        text = QStringLiteral("Download %1").arg(source.url);
        break;
    case ApkSource::Kind::Manual:
        text = QStringLiteral("You choose the APK, downloaded from %1").arg(source.page);
        if (!source.asset.isEmpty())
            text += QStringLiteral(" (a file matching %1)").arg(source.asset);
        return text + u'.';
    }
    if (!source.inZip.isEmpty())
        text += QStringLiteral(", and take the APK matching %1 out of the zip").arg(source.inZip);
    return text + QStringLiteral(". Downloads are kept in qgp's cache.");
}

QStringList grantCommand(const QString& packageName, const Step& step)
{
    return step.appop.isEmpty()
        ? QStringList{QStringLiteral("pm"), QStringLiteral("grant"), packageName, step.permission}
        : QStringList{QStringLiteral("appops"), QStringLiteral("set"), packageName, step.appop, QStringLiteral("allow")};
}

QStringList launchCommand(const QString& packageName)
{
    return {QStringLiteral("monkey"), QStringLiteral("-p"), packageName, QStringLiteral("-c"),
            QStringLiteral("android.intent.category.LAUNCHER"), QStringLiteral("1")};
}

QStringList forceStopCommand(const QString& packageName)
{
    return {QStringLiteral("am"), QStringLiteral("force-stop"), packageName};
}

QString makeFoldersScript(const QString& quotedFolders)
{
    return QStringLiteral("for d in%1; do [ -d \"$d\" ] || { mkdir \"$d\" && chmod 2777 \"$d\"; } || exit 1; done")
        .arg(quotedFolders);
}

InstallPlan planInstall(const Recipe& recipe, const QStringList& apkFiles, const QHash<QString, InputSource>& inputs)
{
    InstallPlan plan;

    if (!recipe.apks.isEmpty()) {
        PlannedStep get{QStringLiteral("Get the APK"), {}};
        for (qsizetype i = 0; i < recipe.apks.size(); ++i) {
            get.lines.append(describeApkSource(recipe.apks.at(i)));
            if (i < apkFiles.size())
                get.lines.append(QStringLiteral("Ready: %1").arg(QDir::toNativeSeparators(apkFiles.at(i))));
        }
        plan.steps.append(get);
    }
    if (recipe.freeSpaceMB > 0)
        plan.steps.append({QStringLiteral("Check the free space"),
                           {QStringLiteral("Stop unless the headset has at least %1 MB free.").arg(recipe.freeSpaceMB)}});

    for (const Step& step : recipe.steps) {
        PlannedStep planned{describeStep(step), {}};
        QStringList& lines = planned.lines;
        if (!step.why.isEmpty())
            lines.append(QStringLiteral("Why: %1").arg(step.why));
        switch (step.kind) {
        case Step::Kind::InstallApk:
            for (qsizetype i = 0; i < recipe.apks.size(); ++i) {
                const QString apk = i < apkFiles.size() ? localPath(apkFiles.at(i))
                                                        : QStringLiteral("<APK %1>").arg(i + 1);
                lines.append(QStringLiteral("adb install -r%1 %2").arg(step.grantAll ? QStringLiteral(" -g") : QString(), apk));
            }
            lines.append(QStringLiteral("If the headset has a newer version, it is kept."));
            break;
        case Step::Kind::Grant:
            lines.append(adbShell(grantCommand(recipe.packageName, step)));
            lines.append(QStringLiteral("If this fails, the install goes on; the app may ask in the headset."));
            break;
        case Step::Kind::Launch:
            lines.append(adbShell(launchCommand(recipe.packageName)));
            lines.append(QStringLiteral("Wait %1 seconds.").arg(step.waitSeconds));
            if (step.stop)
                lines.append(adbShell(forceStopCommand(recipe.packageName)));
            break;
        case Step::Kind::Push:
            planPushStep(recipe, step, inputs, lines);
            break;
        case Step::Kind::Verify:
            for (const QString& file : step.files)
                lines.append(QStringLiteral("Check that %1 exists.").arg(file));
            break;
        case Step::Kind::Shell:
            lines.append(adbShell(step.args));
            break;
        case Step::Kind::Tell:
            lines.append(QStringLiteral("Wait for you to do this in the headset and press Continue:"));
            lines.append(step.text);
            break;
        }
        plan.steps.append(planned);
    }
    return plan;
}

} // namespace installer
