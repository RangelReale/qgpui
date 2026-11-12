#include "installer/Recipe.h"

#include <QJsonArray>

namespace installer {

namespace {

QString str(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1StringView(key)).toString();
}

QStringList strings(const QJsonObject& o, const char* key)
{
    QStringList list;
    for (const QJsonValue& v : o.value(QLatin1StringView(key)).toArray())
        list.append(v.toString());
    return list;
}

bool fail(QString* errorMessage, const QString& message)
{
    if (errorMessage)
        *errorMessage = message;
    return false;
}

bool parseApk(const QJsonObject& o, ApkSource& apk, QString* errorMessage)
{
    const QString source = str(o, "source");
    if (source == QLatin1StringView("github"))
        apk.kind = ApkSource::Kind::GitHub;
    else if (source == QLatin1StringView("url"))
        apk.kind = ApkSource::Kind::Url;
    else if (source == QLatin1StringView("manual"))
        apk.kind = ApkSource::Kind::Manual;
    else
        return fail(errorMessage, QStringLiteral("Unknown APK source \"%1\".").arg(source));
    apk.repo = str(o, "repo");
    apk.tag = str(o, "tag");
    apk.prerelease = o.value(QLatin1StringView("prerelease")).toBool();
    apk.asset = str(o, "asset");
    apk.url = str(o, "url");
    apk.page = str(o, "page");
    apk.help = str(o, "help");
    apk.inZip = str(o, "inZip");
    apk.packageName = str(o, "packageName");
    return true;
}

bool parseInput(const QJsonObject& o, Input& input, QString* errorMessage)
{
    const QString kind = str(o, "kind");
    if (kind == QLatin1StringView("pc-install"))
        input.kind = Input::Kind::PcInstall;
    else if (kind == QLatin1StringView("file"))
        input.kind = Input::Kind::File;
    else
        return fail(errorMessage, QStringLiteral("Unknown input kind \"%1\".").arg(kind));
    input.id = str(o, "id");
    input.label = str(o, "label");
    input.help = str(o, "help");
    input.required = o.value(QLatin1StringView("required")).toBool(true);
    input.group = str(o, "group");
    input.names = strings(o, "names");
    input.extensions = strings(o, "extensions");
    input.multiple = o.value(QLatin1StringView("multiple")).toBool();
    input.keepArchived = o.value(QLatin1StringView("keepArchived")).toBool();
    input.root = str(o, "root");
    input.detect = strings(o, "detect");
    input.json = o;
    return true;
}

bool parseStep(const QJsonObject& o, Step& step, QString* errorMessage)
{
    const QString kind = str(o, "do");
    step.why = str(o, "why");
    if (kind == QLatin1StringView("install-apk")) {
        step.kind = Step::Kind::InstallApk;
        step.grantAll = o.value(QLatin1StringView("grantAll")).toBool();
    } else if (kind == QLatin1StringView("grant")) {
        step.kind = Step::Kind::Grant;
        step.appop = str(o, "appop");
        step.permission = str(o, "permission");
    } else if (kind == QLatin1StringView("launch")) {
        step.kind = Step::Kind::Launch;
        step.waitSeconds = o.value(QLatin1StringView("waitSeconds")).toInt(10);
        step.stop = o.value(QLatin1StringView("stop")).toBool(true);
    } else if (kind == QLatin1StringView("push")) {
        step.kind = Step::Kind::Push;
        step.input = str(o, "input");
        for (const QJsonValue& v : o.value(QLatin1StringView("map")).toArray()) {
            const QJsonObject m = v.toObject();
            step.map.append({str(m, "from"), str(m, "to")});
        }
    } else if (kind == QLatin1StringView("verify")) {
        step.kind = Step::Kind::Verify;
        step.files = strings(o, "files");
    } else if (kind == QLatin1StringView("shell")) {
        step.kind = Step::Kind::Shell;
        step.args = strings(o, "args");
    } else if (kind == QLatin1StringView("tell")) {
        step.kind = Step::Kind::Tell;
        step.text = str(o, "text");
    } else {
        return fail(errorMessage, QStringLiteral("Unknown step \"%1\".").arg(kind));
    }
    return true;
}

} // namespace

const Input* Recipe::input(const QString& id) const
{
    for (const Input& i : inputs) {
        if (i.id == id)
            return &i;
    }
    return nullptr;
}

std::optional<Recipe> parseRecipe(const QJsonObject& json, const QString& defaultPackageName, QString* errorMessage)
{
    Recipe recipe;
    recipe.status = str(json, "status");
    const QString support = str(json, "support");
    recipe.support = support == QLatin1StringView("manual")   ? Support::Manual
                     : support == QLatin1StringView("guided") ? Support::Guided
                                                              : Support::Automatic;
    recipe.packageName = str(json, "packageName");
    if (recipe.packageName.isEmpty())
        recipe.packageName = defaultPackageName;
    recipe.freeSpaceMB = json.value(QLatin1StringView("requirements")).toObject()
                             .value(QLatin1StringView("freeSpaceMB")).toInteger();

    for (const QJsonValue& v : json.value(QLatin1StringView("apks")).toArray()) {
        ApkSource apk;
        if (!parseApk(v.toObject(), apk, errorMessage))
            return std::nullopt;
        recipe.apks.append(apk);
    }
    for (const QJsonValue& v : json.value(QLatin1StringView("inputs")).toArray()) {
        Input input;
        if (!parseInput(v.toObject(), input, errorMessage))
            return std::nullopt;
        recipe.inputs.append(input);
    }
    for (const QJsonValue& v : json.value(QLatin1StringView("steps")).toArray()) {
        Step step;
        if (!parseStep(v.toObject(), step, errorMessage))
            return std::nullopt;
        if (step.kind == Step::Kind::Push && !recipe.input(step.input)) {
            fail(errorMessage, QStringLiteral("A push names the unknown input \"%1\".").arg(step.input));
            return std::nullopt;
        }
        recipe.steps.append(step);
    }
    if (recipe.packageName.isEmpty()) {
        fail(errorMessage, QStringLiteral("The recipe has no package name."));
        return std::nullopt;
    }
    return recipe;
}

QString describeStep(const Step& step)
{
    switch (step.kind) {
    case Step::Kind::InstallApk:
        return QStringLiteral("Install the app");
    case Step::Kind::Grant:
        return QStringLiteral("Grant %1").arg(step.appop.isEmpty() ? step.permission : step.appop);
    case Step::Kind::Launch:
        return QStringLiteral("Start the app once");
    case Step::Kind::Push: {
        QStringList targets;
        for (const PushMapping& m : step.map) {
            if (!targets.contains(m.to))
                targets.append(m.to);
        }
        return QStringLiteral("Copy %1 to %2").arg(step.input, targets.join(QStringLiteral(", ")));
    }
    case Step::Kind::Verify:
        return QStringLiteral("Check the copied files");
    case Step::Kind::Shell:
        return QStringLiteral("Run %1").arg(step.args.value(0));
    case Step::Kind::Tell:
        return QStringLiteral("Your turn in the headset");
    }
    return {};
}

} // namespace installer
