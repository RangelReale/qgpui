#include "questports/Catalog.h"

#include <QTemporaryDir>
#include <QTest>

using namespace questports;

namespace {

QByteArray catalogJson(const QByteArray& games, const QByteArray& version = "1")
{
    return R"({"format":"qgpui-catalog","version":)" + version +
           R"(,"originPlatforms":[{"id":"pc","label":"PC"},{"id":"n64","label":"Nintendo 64"}])" +
           R"(,"vrStyles":[{"id":"motion","label":"Full VR"},{"id":"seated","label":"Seated"}])" +
           R"(,"headsets":["Quest 2","Quest 3"])" +
           R"(,"generatedAt":"2026-11-12T10:00:00.000Z",)"
           R"("sources":{"payload":{"generatedAt":"2026-11-11T12:00:00.000Z"},"recipes":{"count":1}},)"
           R"("games":)" + games + "}";
}

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    void readsGames()
    {
        QString error;
        // The URL is written "https:\/\/" because moc reads "//" inside a raw string as a comment.
        const auto catalog = parseCatalog(catalogJson(R"([
            {"slug":"avp-vr","port":{"title":"AvP VR","cover_image_url":"https:\/\/x.test/avp.jpg",
                                     "developer":"Bassquake","category":"source_port","status":"released","created_at":"2026-10-11T16:35:37.471366+00:00",
                                     "last_github_update":"2026-10-18T16:07:19+00:00"},
             "install":{"packageName":"com.example.avp"},"campaigns":null,
             "resolved":{"installType":"apk_and_assets","originPlatform":"pc","vrStyle":"motion",
                         "headsets":["Quest 2","Quest 3"],"noPcInstall":"yes"},
             "recipe":{"status":"draft","steps":[{"do":"install-apk"}]},
             "drift":[{"field":"version","recipe":"v1","payload":"v2"},
                      {"field":"targetPath","recipe":null,"payload":"/sdcard/AvP/"}]},
            {"slug":"bare","port":{"title":"Bare","cover_image_url":null},"install":null,"campaigns":[{"id":"a"}],
             "recipe":null,"drift":[]}
        ])"), &error);
        QVERIFY2(catalog, qPrintable(error));
        QCOMPARE(catalog->generatedAt, QDateTime(QDate(2026, 11, 12), QTime(10, 0), QTimeZone::UTC));
        QCOMPARE(catalog->payloadGeneratedAt, QDateTime(QDate(2026, 11, 11), QTime(12, 0), QTimeZone::UTC));
        QCOMPARE(catalog->games.size(), 2);
        QCOMPARE(catalog->originPlatforms.size(), 2);
        QCOMPARE(catalog->originPlatforms[1].id, QStringLiteral("n64"));
        QCOMPARE(catalog->originPlatforms[1].label, QStringLiteral("Nintendo 64"));
        QCOMPARE(catalog->vrStyles.size(), 2);
        QCOMPARE(catalog->vrStyles[1].id, QStringLiteral("seated"));
        QCOMPARE(catalog->vrStyles[1].label, QStringLiteral("Seated"));
        QCOMPARE(catalog->headsets, QStringList({QStringLiteral("Quest 2"), QStringLiteral("Quest 3")}));

        const Game& avp = catalog->games[0];
        QCOMPARE(avp.slug, QStringLiteral("avp-vr"));
        QCOMPARE(avp.title, QStringLiteral("AvP VR"));
        QCOMPARE(avp.coverUrl, QUrl(QStringLiteral("https://x.test/avp.jpg")));
        QCOMPARE(avp.developer, QStringLiteral("Bassquake"));
        QCOMPARE(avp.addedAt, QDateTime(QDate(2026, 10, 11), QTime(16, 35, 37, 471), QTimeZone::UTC));
        QCOMPARE(avp.releasedAt, QDateTime(QDate(2026, 10, 18), QTime(16, 7, 19), QTimeZone::UTC));
        QVERIFY(avp.hasInstall());
        QCOMPARE(avp.install.value(QLatin1StringView("packageName")).toString(), QStringLiteral("com.example.avp"));
        QVERIFY(avp.campaigns.isEmpty());
        QCOMPARE(avp.resolved.value(QLatin1StringView("installType")).toString(), QStringLiteral("apk_and_assets"));
        QCOMPARE(avp.originPlatform, QStringLiteral("pc"));
        QCOMPARE(avp.vrStyle, QStringLiteral("motion"));
        QCOMPARE(avp.headsets, QStringList({QStringLiteral("Quest 2"), QStringLiteral("Quest 3")}));
        QVERIFY(avp.noPcNeeded);
        QCOMPARE(avp.category, QStringLiteral("source_port"));
        QCOMPARE(avp.status, QStringLiteral("released"));
        QVERIFY(avp.hasRecipe());
        QCOMPARE(avp.recipe.value(QLatin1StringView("status")).toString(), QStringLiteral("draft"));
        QCOMPARE(avp.drift.size(), 2);
        QCOMPARE(avp.drift[0].field, QStringLiteral("version"));
        QCOMPARE(avp.drift[0].recipe, QStringLiteral("v1"));
        QCOMPARE(avp.drift[0].payload, QStringLiteral("v2"));
        QVERIFY(!avp.drift[0].isSerious());
        QVERIFY(avp.drift[1].recipe.isNull());
        QCOMPARE(avp.drift[1].payload, QStringLiteral("/sdcard/AvP/"));
        QVERIFY(avp.drift[1].isSerious());

        const Game& bare = catalog->games[1];
        QVERIFY(bare.coverUrl.isEmpty());
        QVERIFY(!bare.addedAt.isValid());
        QVERIFY(!bare.releasedAt.isValid());
        QVERIFY(!bare.hasInstall());
        QCOMPARE(bare.campaigns.size(), 1);
        QVERIFY(!bare.hasRecipe());
        QVERIFY(bare.drift.isEmpty());
        QVERIFY(bare.originPlatform.isEmpty());
        QVERIFY(bare.vrStyle.isEmpty());
        QVERIFY(bare.headsets.isEmpty());
        QVERIFY(!bare.noPcNeeded);
        QVERIFY(bare.category.isEmpty());
    }

    void rejectsBadCatalogs_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("message");

        QTest::newRow("not json") << QByteArray("{") << QStringLiteral("Not valid JSON");
        QTest::newRow("not object") << QByteArray("[]") << QStringLiteral("not a JSON object");
        QTest::newRow("format") << QByteArray(R"({"format":"other","version":1,"games":[]})")
                                << QStringLiteral("format \"other\"");
        QTest::newRow("payload") << QByteArray(R"({"format":"qgpui-questports-payload","version":1,"games":[]})")
                                 << QStringLiteral("task catalog");
        QTest::newRow("version") << catalogJson("[]", "2") << QStringLiteral("Unsupported catalog version 2");
        QTest::newRow("no games") << QByteArray(R"({"format":"qgpui-catalog","version":1})")
                                  << QStringLiteral("no games list");
        QTest::newRow("no slug") << catalogJson(R"([{"port":{"title":"T"}}])") << QStringLiteral("Game 0 has no slug");
        QTest::newRow("slug climbs") << catalogJson(R"([{"slug":"..\/..\/x","port":{"title":"T"}}])")
                                     << QStringLiteral("unsafe slug");
        QTest::newRow("slug with a folder") << catalogJson(R"([{"slug":"a\\b","port":{"title":"T"}}])")
                                            << QStringLiteral("unsafe slug");
        QTest::newRow("slug upper case") << catalogJson(R"([{"slug":"Quake","port":{"title":"T"}}])")
                                         << QStringLiteral("unsafe slug");
        QTest::newRow("no title") << catalogJson(R"([{"slug":"s","port":{}}])")
                                  << QStringLiteral("\"s\" has no title");
    }

    void rejectsBadCatalogs()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, message);
        QString error;
        QVERIFY(!parseCatalog(json, &error));
        QVERIFY2(error.contains(message), qPrintable(error));
    }

    void loadsFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("catalog.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(catalogJson(R"([{"slug":"s","port":{"title":"T"}}])"));
        file.close();

        QVERIFY(loadCatalog(path));
        QString error;
        QVERIFY(!loadCatalog(dir.filePath(QStringLiteral("missing.json")), &error));
        QVERIFY2(error.startsWith(QStringLiteral("Cannot read")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
