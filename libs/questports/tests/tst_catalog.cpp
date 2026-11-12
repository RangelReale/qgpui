#include "questports/Catalog.h"

#include <QTemporaryDir>
#include <QTest>

using namespace questports;

namespace {

QByteArray payload(const QByteArray& games, const QByteArray& version = "1")
{
    return R"({"format":"qgp-questports-payload","version":)" + version +
           R"(,"generatedAt":"2026-11-11T12:00:00.000Z","games":)" + games + "}";
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
        const auto catalog = parseCatalog(payload(R"([
            {"slug":"avp-vr","port":{"title":"AvP VR","cover_image_url":"https:\/\/x.test/avp.jpg"},
             "install":{"packageName":"com.example.avp"},"campaigns":null,
             "resolved":{"installType":"apk_and_assets"}},
            {"slug":"bare","port":{"title":"Bare","cover_image_url":null},"install":null,"campaigns":[{"id":"a"}]}
        ])"), &error);
        QVERIFY2(catalog, qPrintable(error));
        QCOMPARE(catalog->generatedAt, QDateTime(QDate(2026, 11, 11), QTime(12, 0), QTimeZone::UTC));
        QCOMPARE(catalog->games.size(), 2);

        const Game& avp = catalog->games[0];
        QCOMPARE(avp.slug, QStringLiteral("avp-vr"));
        QCOMPARE(avp.title, QStringLiteral("AvP VR"));
        QCOMPARE(avp.coverUrl, QUrl(QStringLiteral("https://x.test/avp.jpg")));
        QVERIFY(avp.hasInstall());
        QCOMPARE(avp.install.value(QLatin1StringView("packageName")).toString(), QStringLiteral("com.example.avp"));
        QVERIFY(avp.campaigns.isEmpty());
        QCOMPARE(avp.resolved.value(QLatin1StringView("installType")).toString(), QStringLiteral("apk_and_assets"));

        const Game& bare = catalog->games[1];
        QVERIFY(bare.coverUrl.isEmpty());
        QVERIFY(!bare.hasInstall());
        QCOMPARE(bare.campaigns.size(), 1);
    }

    void rejectsBadPayloads_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("message");

        QTest::newRow("not json") << QByteArray("{") << QStringLiteral("Not valid JSON");
        QTest::newRow("not object") << QByteArray("[]") << QStringLiteral("not a JSON object");
        QTest::newRow("format") << QByteArray(R"({"format":"other","version":1,"games":[]})")
                                << QStringLiteral("format \"other\"");
        QTest::newRow("version") << payload("[]", "2") << QStringLiteral("Unsupported payload version 2");
        QTest::newRow("no games") << QByteArray(R"({"format":"qgp-questports-payload","version":1})")
                                  << QStringLiteral("no games list");
        QTest::newRow("no slug") << payload(R"([{"port":{"title":"T"}}])") << QStringLiteral("Game 0 has no slug");
        QTest::newRow("no title") << payload(R"([{"slug":"s","port":{}}])")
                                  << QStringLiteral("\"s\" has no title");
    }

    void rejectsBadPayloads()
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
        const QString path = dir.filePath(QStringLiteral("payload.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(payload(R"([{"slug":"s","port":{"title":"T"}}])"));
        file.close();

        QVERIFY(loadCatalog(path));
        QString error;
        QVERIFY(!loadCatalog(dir.filePath(QStringLiteral("missing.json")), &error));
        QVERIFY2(error.startsWith(QStringLiteral("Cannot read")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
