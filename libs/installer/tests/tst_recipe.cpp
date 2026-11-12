#include "installer/Recipe.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTest>

using namespace installer;

namespace {

// JSON with single quotes, in ordinary literals (moc misreads // and /* inside raw strings).
QJsonObject json(QByteArray text)
{
    return QJsonDocument::fromJson(text.replace('\'', '"')).object();
}

} // namespace

class TestRecipe : public QObject
{
    Q_OBJECT

private slots:
    void readsAllParts()
    {
        QString error;
        const auto recipe = parseRecipe(
            json("{'support':'guided','requirements':{'freeSpaceMB':900},"
                 "'apks':[{'source':'github','repo':'o/r','tag':'^v','prerelease':true,'asset':'q\\\\.apk$'},"
                 "        {'source':'manual','page':'https:\\/\\/x.test','asset':'a'}],"
                 "'inputs':[{'id':'disc','kind':'file','label':'Disc','extensions':['.iso'],'multiple':true},"
                 "          {'id':'game','kind':'pc-install','label':'Game','required':false,'root':'id1','detect':['pak0.pak'],"
                 "           'stores':[{'store':'steam','appId':'2310'}]}],"
                 "'steps':[{'do':'install-apk','grantAll':true},{'do':'grant','appop':'MANAGE_EXTERNAL_STORAGE'},"
                 "         {'do':'launch','why':'w','waitSeconds':3,'stop':false},"
                 "         {'do':'push','input':'disc','map':[{'to':'\\/sdcard\\/Download\\/'}]},"
                 "         {'do':'verify','files':['\\/sdcard\\/a']},{'do':'shell','args':['am','force-stop','p'],'why':'w'},"
                 "         {'do':'tell','text':'Open it.'}]}"),
            QStringLiteral("com.example.game"), &error);
        QVERIFY2(recipe, qPrintable(error));
        QCOMPARE(recipe->support, Support::Guided);
        QCOMPARE(recipe->packageName, QStringLiteral("com.example.game"));
        QCOMPARE(recipe->freeSpaceMB, 900);
        QCOMPARE(recipe->apks.size(), 2);
        QCOMPARE(recipe->apks[0].kind, ApkSource::Kind::GitHub);
        QCOMPARE(recipe->apks[0].asset, QStringLiteral("q\\.apk$"));
        QVERIFY(recipe->apks[0].prerelease);
        QCOMPARE(recipe->apks[1].kind, ApkSource::Kind::Manual);
        QCOMPARE(recipe->inputs.size(), 2);
        QVERIFY(recipe->inputs[0].multiple);
        QVERIFY(!recipe->inputs[1].required);
        QCOMPARE(recipe->inputs[1].root, QStringLiteral("id1"));
        QVERIFY(recipe->inputs[1].json.contains(QStringLiteral("stores")));
        QCOMPARE(recipe->steps.size(), 7);
        QVERIFY(recipe->steps[0].grantAll);
        QCOMPARE(recipe->steps[1].appop, QStringLiteral("MANAGE_EXTERNAL_STORAGE"));
        QCOMPARE(recipe->steps[2].waitSeconds, 3);
        QVERIFY(!recipe->steps[2].stop);
        QCOMPARE(recipe->steps[3].map.size(), 1);
        QVERIFY(recipe->steps[3].map[0].from.isEmpty());
        QCOMPARE(recipe->steps[5].args.size(), 3);
        QCOMPARE(recipe->steps[6].text, QStringLiteral("Open it."));
        QCOMPARE(describeStep(recipe->steps[3]), QStringLiteral("Copy disc to /sdcard/Download/"));
        QVERIFY(recipe->input(QStringLiteral("game")));
        QVERIFY(!recipe->input(QStringLiteral("nope")));
    }

    void usesTheRecipesPackageOverride()
    {
        const auto recipe = parseRecipe(json("{'packageName':'org.openmw.vr','apks':[],'inputs':[],'steps':[]}"),
                                        QStringLiteral("com.openmw.vr"));
        QVERIFY(recipe);
        QCOMPARE(recipe->packageName, QStringLiteral("org.openmw.vr"));
    }

    void rejectsWhatTheRunnerCannotRun_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("message");
        QTest::newRow("step") << QByteArray("{'steps':[{'do':'reboot'}]}") << QStringLiteral("Unknown step");
        QTest::newRow("source") << QByteArray("{'apks':[{'source':'ftp'}]}") << QStringLiteral("Unknown APK source");
        QTest::newRow("input") << QByteArray("{'inputs':[{'id':'a','kind':'disc'}]}") << QStringLiteral("Unknown input kind");
        QTest::newRow("push") << QByteArray("{'steps':[{'do':'push','input':'x','map':[]}]}") << QStringLiteral("unknown input");
        // The catalog comes from outside qgp: what reaches a file system or a shell is checked again.
        QTest::newRow("input id") << QByteArray("{'inputs':[{'id':'../x','kind':'file'}]}") << QStringLiteral("Bad input id");
        QTest::newRow("root") << QByteArray("{'inputs':[{'id':'a','kind':'pc-install','root':'../..'}]}")
                              << QStringLiteral("unsafe root");
        QTest::newRow("detect") << QByteArray("{'inputs':[{'id':'a','kind':'pc-install','detect':['C:/Windows']}]}")
                                << QStringLiteral("unsafe path");
        const QByteArray input("{'id':'a','kind':'file'}");
        QTest::newRow("push to") << QByteArray("{'inputs':[" + input + "],'steps':[{'do':'push','input':'a','map':[{'to':'/sdcard/../data/x'}]}]}")
                                 << QStringLiteral("unsafe path");
        QTest::newRow("push outside") << QByteArray("{'inputs':[" + input + "],'steps':[{'do':'push','input':'a','map':[{'to':'/data/local/'}]}]}")
                                      << QStringLiteral("unsafe path");
        QTest::newRow("push from") << QByteArray("{'inputs':[" + input + "],'steps':[{'do':'push','input':'a','map':[{'from':'../*.pak','to':'/sdcard/x/'}]}]}")
                                   << QStringLiteral("unsafe path");
        QTest::newRow("verify") << QByteArray("{'steps':[{'do':'verify','files':['/sdcard/a/../../x']}]}")
                                << QStringLiteral("unsafe path");
        QTest::newRow("appop") << QByteArray("{'steps':[{'do':'grant','appop':'X; reboot'}]}") << QStringLiteral("Bad appop");
        QTest::newRow("permission") << QByteArray("{'steps':[{'do':'grant','permission':'android.permission.X Y'}]}")
                                    << QStringLiteral("Bad permission");
        QTest::newRow("grant") << QByteArray("{'steps':[{'do':'grant'}]}") << QStringLiteral("either an appop");
        QTest::newRow("repo") << QByteArray("{'apks':[{'source':'github','repo':'a/../../b'}]}") << QStringLiteral("Bad GitHub");
        QTest::newRow("apk package") << QByteArray("{'apks':[{'source':'url','packageName':'a b'}]}") << QStringLiteral("Bad package");
        QTest::newRow("package") << QByteArray("{'packageName':'com.x;reboot'}") << QStringLiteral("Bad package");
    }

    void rejectsWhatTheRunnerCannotRun()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, message);
        QString error;
        QVERIFY(!parseRecipe(json(text), QStringLiteral("p.q"), &error));
        QVERIFY2(error.contains(message), qPrintable(error));
    }

    void headsetPaths_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("safe");
        QTest::newRow("file") << QStringLiteral("/sdcard/Android/obb/com.x/main.obb") << true;
        QTest::newRow("folder") << QStringLiteral("/sdcard/Download/") << true;
        QTest::newRow("dots in a name") << QStringLiteral("/sdcard/a/..b/c.") << true;
        QTest::newRow("sdcard itself") << QStringLiteral("/sdcard/") << true;
        QTest::newRow("sdcard without the slash") << QStringLiteral("/sdcard") << false;
        QTest::newRow("outside") << QStringLiteral("/data/local/tmp/x") << false;
        QTest::newRow("relative") << QStringLiteral("sdcard/x") << false;
        QTest::newRow("climbs") << QStringLiteral("/sdcard/../data/x") << false;
        QTest::newRow("climbs at the end") << QStringLiteral("/sdcard/a/..") << false;
        QTest::newRow("dot") << QStringLiteral("/sdcard/./x") << false;
        QTest::newRow("empty segment") << QStringLiteral("/sdcard//x") << false;
        QTest::newRow("backslash") << QStringLiteral("/sdcard/a\\..\\x") << false;
        QTest::newRow("newline") << QStringLiteral("/sdcard/a\nb") << false;
    }

    void headsetPaths()
    {
        QFETCH(QString, path);
        QFETCH(bool, safe);
        QCOMPARE(isSafeHeadsetPath(path), safe);
    }

    void relativePaths_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("safe");
        QTest::newRow("name") << QStringLiteral("pak0.pak") << true;
        QTest::newRow("glob") << QStringLiteral("**/*.pak") << true;
        QTest::newRow("backslashes") << QStringLiteral("base\\pak0.pak") << true;
        QTest::newRow("empty") << QString() << false;
        QTest::newRow("absolute") << QStringLiteral("/etc/x") << false;
        QTest::newRow("drive") << QStringLiteral("C:/Windows") << false;
        QTest::newRow("climbs") << QStringLiteral("a/../../b") << false;
        QTest::newRow("climbs with backslashes") << QStringLiteral("..\\b") << false;
    }

    void relativePaths()
    {
        QFETCH(QString, path);
        QFETCH(bool, safe);
        QCOMPARE(isSafeRelativePath(path), safe);
    }

    // The checks must not reject a recipe that build-catalog accepts.
    void readsEveryRecipe()
    {
        const QDir dir(QStringLiteral(QGP_RECIPES_DIR));
        const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files);
        QVERIFY(!files.isEmpty());
        for (const QString& name : files) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QJsonObject recipe = QJsonDocument::fromJson(file.readAll()).object();
            const QString package = recipe.value(QLatin1StringView("checkedAgainst")).toObject()
                                        .value(QLatin1StringView("packageName")).toString();
            QString error;
            QVERIFY2(parseRecipe(recipe, package, &error), qPrintable(name + QStringLiteral(": ") + error));
        }
    }

    void needsAPackageName()
    {
        QString error;
        QVERIFY(!parseRecipe(json("{'steps':[]}"), {}, &error));
        QVERIFY2(error.contains(QStringLiteral("package")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestRecipe)
#include "tst_recipe.moc"
