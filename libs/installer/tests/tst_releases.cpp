#include "installer/ApkFetcher.h"

#include <QJsonDocument>
#include <QTest>

using namespace installer;

namespace {

QJsonArray releases()
{
    // Single quotes, in an ordinary literal: moc misreads // inside raw strings.
    QByteArray text =
        "["
        " {'tag_name':'vcop2-v0.1.0','draft':false,'prerelease':false,'published_at':'2026-11-11T00:00:00Z',"
        "  'assets':[{'name':'Port0r-Cop2VR-v0.1.0-quest3.apk','browser_download_url':'https:\\/\\/x.test\\/c2.apk','size':10}]},"
        " {'tag_name':'scud-v0.2.3','draft':false,'prerelease':false,'published_at':'2026-11-09T14:30:00Z',"
        "  'assets':[{'name':'Port0r-ScudRaceVR-v0.2.3-quest3.apk','browser_download_url':'https:\\/\\/x.test\\/s.apk','size':20}]},"
        " {'tag_name':'vcop-v0.2.5','draft':true,'prerelease':false,'published_at':'2026-11-12T00:00:00Z',"
        "  'assets':[{'name':'Port0r-CopVR-v0.2.5-quest3.apk','browser_download_url':'https:\\/\\/x.test\\/draft.apk','size':1}]},"
        " {'tag_name':'vcop-v0.2.4','draft':false,'prerelease':false,'published_at':'2026-11-01T00:00:00Z',"
        "  'assets':[{'name':'SHA256SUMS.txt','browser_download_url':'https:\\/\\/x.test\\/sums','size':1},"
        "            {'name':'Port0r-CopVR-v0.2.4-quest3.apk','browser_download_url':'https:\\/\\/x.test\\/c.apk','size':30}]},"
        " {'tag_name':'v1.5-windows','draft':false,'prerelease':false,'published_at':'2026-11-10T00:00:00Z',"
        "  'assets':[{'name':'game-windows.zip','browser_download_url':'https:\\/\\/x.test\\/w.zip','size':5}]},"
        " {'tag_name':'v1.4','draft':false,'prerelease':false,'published_at':'2026-10-01T00:00:00Z',"
        "  'assets':[{'name':'game-quest.apk','browser_download_url':'https:\\/\\/x.test\\/q14.apk','size':6}]},"
        " {'tag_name':'v1.6-beta','draft':false,'prerelease':true,'published_at':'2026-11-11T00:00:00Z',"
        "  'assets':[{'name':'game-quest.apk','browser_download_url':'https:\\/\\/x.test\\/q16.apk','size':7}]}"
        "]";
    return QJsonDocument::fromJson(text.replace('\'', '"')).array();
}

ApkSource source(const QString& asset, const QString& tag = {}, bool prerelease = false)
{
    ApkSource s;
    s.asset = asset;
    s.tag = tag;
    s.prerelease = prerelease;
    return s;
}

} // namespace

class TestReleases : public QObject
{
    Q_OBJECT

private slots:
    void picksTheNewestMatchingTagAndAsset()
    {
        const auto cop = pickRelease(releases(), source(QStringLiteral("^Port0r-CopVR-.*-quest3\\.apk$"), QStringLiteral("^vcop-v")));
        QVERIFY(cop);
        QCOMPARE(cop->tag, QStringLiteral("vcop-v0.2.4")); // The draft v0.2.5 is skipped.
        QCOMPARE(cop->name, QStringLiteral("Port0r-CopVR-v0.2.4-quest3.apk"));
        QCOMPARE(cop->url, QUrl(QStringLiteral("https://x.test/c.apk")));
        QCOMPARE(cop->size, 30);
    }

    void skipsReleasesWithoutAMatchingAsset()
    {
        // v1.5-windows is newer but has no Quest APK.
        const auto game = pickRelease(releases(), source(QStringLiteral("quest\\.apk$")));
        QVERIFY(game);
        QCOMPARE(game->tag, QStringLiteral("v1.4"));
    }

    void takesPreReleasesOnlyWhenAllowed()
    {
        const auto game = pickRelease(releases(), source(QStringLiteral("quest\\.apk$"), {}, true));
        QVERIFY(game);
        QCOMPARE(game->tag, QStringLiteral("v1.6-beta"));
    }

    void findsNothingWhenNothingMatches()
    {
        QVERIFY(!pickRelease(releases(), source(QStringLiteral("pico\\.apk$"))));
        QVERIFY(!pickRelease(releases(), source(QStringLiteral("\\.apk$"), QStringLiteral("^nothing"))));
        QVERIFY(!pickRelease({}, source(QStringLiteral("."))));
    }
};

QTEST_GUILESS_MAIN(TestReleases)
#include "tst_releases.moc"
