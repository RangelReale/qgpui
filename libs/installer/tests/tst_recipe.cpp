#include "installer/Recipe.h"

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
            json("{'status':'draft','support':'guided','requirements':{'freeSpaceMB':900},"
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
    }

    void rejectsWhatTheRunnerCannotRun()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, message);
        QString error;
        QVERIFY(!parseRecipe(json(text), QStringLiteral("p.q"), &error));
        QVERIFY2(error.contains(message), qPrintable(error));
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
