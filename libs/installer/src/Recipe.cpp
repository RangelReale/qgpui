#include "installer/Recipe.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

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

const QLatin1StringView kIdPattern("^[a-z0-9]+(-[a-z0-9]+)*$");
const QLatin1StringView kPackagePattern(R"(^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$)");
const QLatin1StringView kRepoPattern("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$");
const QLatin1StringView kAppopPattern("^[A-Z_]+$");
const QLatin1StringView kPermissionPattern(R"(^android\.permission\.[A-Z_]+$)");

bool matches(const QString& s, QLatin1StringView pattern)
{
    return QRegularExpression(pattern).match(s).hasMatch();
}

bool hasControlCharacter(const QString& s)
{
    return std::any_of(s.cbegin(), s.cend(), [](QChar c) { return c.unicode() < 0x20 || c.unicode() == 0x7f; });
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
    if (apk.kind == ApkSource::Kind::GitHub && !matches(apk.repo, kRepoPattern))
        return fail(errorMessage, QStringLiteral("Bad GitHub repository \"%1\".").arg(apk.repo));
    if (!apk.packageName.isEmpty() && !isValidPackageName(apk.packageName))
        return fail(errorMessage, QStringLiteral("Bad package name \"%1\".").arg(apk.packageName));
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
    if (!isValidRecipeId(input.id))
        return fail(errorMessage, QStringLiteral("Bad input id \"%1\".").arg(input.id));
    if (!input.root.isEmpty() && !isSafeRelativePath(input.root))
        return fail(errorMessage, QStringLiteral("Input \"%1\" has the unsafe root \"%2\".").arg(input.id, input.root));
    for (const QString& path : std::as_const(input.detect)) {
        if (!isSafeRelativePath(path))
            return fail(errorMessage, QStringLiteral("Input \"%1\" detects the unsafe path \"%2\".").arg(input.id, path));
    }
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
        if (step.appop.isEmpty() == step.permission.isEmpty())
            return fail(errorMessage, QStringLiteral("A grant needs either an appop or a permission."));
        if (!step.appop.isEmpty() && !matches(step.appop, kAppopPattern))
            return fail(errorMessage, QStringLiteral("Bad appop \"%1\".").arg(step.appop));
        if (!step.permission.isEmpty() && !matches(step.permission, kPermissionPattern))
            return fail(errorMessage, QStringLiteral("Bad permission \"%1\".").arg(step.permission));
    } else if (kind == QLatin1StringView("launch")) {
        step.kind = Step::Kind::Launch;
        step.waitSeconds = o.value(QLatin1StringView("waitSeconds")).toInt(10);
        step.stop = o.value(QLatin1StringView("stop")).toBool(true);
    } else if (kind == QLatin1StringView("push")) {
        step.kind = Step::Kind::Push;
        step.input = str(o, "input");
        for (const QJsonValue& v : o.value(QLatin1StringView("map")).toArray()) {
            const QJsonObject m = v.toObject();
            const PushMapping mapping{str(m, "from"), str(m, "to")};
            if (!mapping.from.isEmpty() && !isSafeRelativePath(mapping.from))
                return fail(errorMessage, QStringLiteral("A push copies from the unsafe path \"%1\".").arg(mapping.from));
            if (!isSafeHeadsetPath(mapping.to))
                return fail(errorMessage, QStringLiteral("A push copies to the unsafe path \"%1\".").arg(mapping.to));
            step.map.append(mapping);
        }
    } else if (kind == QLatin1StringView("verify")) {
        step.kind = Step::Kind::Verify;
        step.files = strings(o, "files");
        for (const QString& path : std::as_const(step.files)) {
            if (!isSafeHeadsetPath(path))
                return fail(errorMessage, QStringLiteral("A verify checks the unsafe path \"%1\".").arg(path));
        }
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
    for (const QJsonValue& v : json.value(QLatin1StringView("otherWays")).toArray()) {
        const QJsonObject o = v.toObject();
        OtherWay way{str(o, "label"), str(o, "url"), str(o, "help")};
        // The dialog opens the link in the browser: only a web address, never a file or a program.
        const QUrl url(way.url, QUrl::StrictMode);
        if (!url.isValid() || url.scheme() != QLatin1StringView("https") || url.host().isEmpty()) {
            fail(errorMessage, QStringLiteral("Bad link \"%1\" for \"%2\".").arg(way.url, way.label));
            return std::nullopt;
        }
        recipe.otherWays.append(way);
    }
    if (recipe.packageName.isEmpty()) {
        fail(errorMessage, QStringLiteral("The recipe has no package name."));
        return std::nullopt;
    }
    if (!isValidPackageName(recipe.packageName)) {
        fail(errorMessage, QStringLiteral("Bad package name \"%1\".").arg(recipe.packageName));
        return std::nullopt;
    }
    return recipe;
}

bool isValidRecipeId(const QString& id)
{
    return matches(id, kIdPattern);
}

bool isValidPackageName(const QString& name)
{
    return matches(name, kPackagePattern);
}

bool isSafeHeadsetPath(const QString& path)
{
    static const QLatin1StringView prefix("/sdcard/");
    if (!path.startsWith(prefix) || path.contains(u'\\') || hasControlCharacter(path))
        return false;
    if (path.size() == prefix.size())
        return true; // A push into /sdcard/ itself.
    QStringList segments = path.mid(prefix.size()).split(u'/');
    if (segments.size() > 1 && segments.last().isEmpty())
        segments.removeLast(); // A folder target ends in /.
    for (const QString& segment : std::as_const(segments)) {
        if (segment.isEmpty() || segment == u'.' || segment == QLatin1StringView(".."))
            return false;
    }
    return true;
}

bool isSafeRelativePath(const QString& path)
{
    if (path.isEmpty() || hasControlCharacter(path) || path.contains(u':'))
        return false;
    const QString normal = QString(path).replace(u'\\', u'/');
    if (normal.startsWith(u'/'))
        return false;
    const QStringList segments = normal.split(u'/');
    return !segments.contains(QLatin1StringView(".."));
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
