#include "installer/ApkFetcher.h"
#include "installer/ReleaseLookup.h"

#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>
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

// A stand-in for GitHub's API: answers every request with the releases above, or with an error
// status, and records the requested paths.
class FakeGitHub : public QObject
{
public:
    FakeGitHub()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    QByteArray& request = m_requests[socket];
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    paths.append(QString::fromLatin1(request.split(' ').value(1)));
                    m_requests.remove(socket);
                    const QByteArray body = status == 200 ? QJsonDocument(releases()).toJson() : QByteArray("{}");
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Whatever\r\n"
                                  "Content-Type: application/json\r\nConnection: close\r\n"
                                  "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }

    int status = 200;
    QStringList paths;

private:
    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_requests;
};

struct Answer
{
    int calls = 0;
    std::optional<ReleaseAsset> asset;
    QString error;
};

ReleaseLookup::Callback recordInto(Answer& answer)
{
    return [&answer](const std::optional<ReleaseAsset>& asset, const QString& error) {
        ++answer.calls;
        answer.asset = asset;
        answer.error = error;
    };
}

ApkSource github(const QString& repo, const QString& asset, const QString& tag = {})
{
    ApkSource s = source(asset, tag);
    s.repo = repo;
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

    void looksUpEachRepositoryOnce()
    {
        FakeGitHub server;
        ReleaseLookup lookup;
        lookup.setApiBaseUrl(server.url());
        const ApkSource cop = github(QStringLiteral("o/port0r"), QStringLiteral("CopVR-.*\\.apk$"), QStringLiteral("^vcop-v"));
        const ApkSource scud = github(QStringLiteral("o/port0r"), QStringLiteral("ScudRaceVR-.*\\.apk$"));

        // Two sources of one repository while the first request runs.
        Answer first, second;
        lookup.latest(cop, this, recordInto(first));
        lookup.latest(scud, this, recordInto(second));
        QTRY_COMPARE(first.calls, 1);
        QTRY_COMPARE(second.calls, 1);
        QCOMPARE(first.asset->tag, QStringLiteral("vcop-v0.2.4"));
        QCOMPARE(second.asset->tag, QStringLiteral("scud-v0.2.3"));

        // Then one from the kept answer, on a later event-loop iteration.
        Answer kept;
        lookup.latest(cop, this, recordInto(kept));
        QCOMPARE(kept.calls, 0);
        QTRY_COMPARE(kept.calls, 1);
        QCOMPARE(kept.asset->tag, QStringLiteral("vcop-v0.2.4"));
        QCOMPARE(server.paths, QStringList{QStringLiteral("/repos/o/port0r/releases?per_page=100")});

        // After clear(), GitHub is asked again.
        lookup.clear();
        Answer again;
        lookup.latest(cop, this, recordInto(again));
        QTRY_COMPARE(again.calls, 1);
        QCOMPARE(server.paths.size(), 2);
    }

    void reportsWhatItCannotFind()
    {
        FakeGitHub server;
        ReleaseLookup lookup;
        lookup.setApiBaseUrl(server.url());

        Answer none;
        lookup.latest(github(QStringLiteral("o/a"), QStringLiteral("pico\\.apk$")), this, recordInto(none));
        QTRY_COMPARE(none.calls, 1);
        QVERIFY(!none.asset);
        QVERIFY(none.error.contains(QStringLiteral("No release of o/a")));

        server.status = 403; // GitHub's answer to a used-up rate limit.
        Answer failed, keptFailure;
        lookup.latest(github(QStringLiteral("o/b"), QStringLiteral("\\.apk$")), this, recordInto(failed));
        QTRY_COMPARE(failed.calls, 1);
        QVERIFY(!failed.asset);
        QVERIFY(failed.error.contains(QStringLiteral("Could not list the releases of o/b")));
        // The failure is kept for a while too.
        lookup.latest(github(QStringLiteral("o/b"), QStringLiteral("\\.apk$")), this, recordInto(keptFailure));
        QTRY_COMPARE(keptFailure.calls, 1);
        QVERIFY(!keptFailure.asset);
        QCOMPARE(server.paths.size(), 2);

        Answer notGitHub;
        ApkSource url;
        url.kind = ApkSource::Kind::Url;
        url.url = QStringLiteral("https://x.test/a.apk");
        lookup.latest(url, this, recordInto(notGitHub));
        QTRY_COMPARE(notGitHub.calls, 1);
        QVERIFY(!notGitHub.asset);
        QCOMPARE(server.paths.size(), 2);
    }

    void dropsAnswersForDeletedContexts()
    {
        FakeGitHub server;
        ReleaseLookup lookup;
        lookup.setApiBaseUrl(server.url());
        const ApkSource game = github(QStringLiteral("o/a"), QStringLiteral("quest\\.apk$"));
        Answer dropped, kept;
        auto* context = new QObject;
        lookup.latest(game, context, recordInto(dropped));
        lookup.latest(game, this, recordInto(kept));
        delete context;
        QTRY_COMPARE(kept.calls, 1);
        QCOMPARE(dropped.calls, 0);
    }
};

QTEST_GUILESS_MAIN(TestReleases)
#include "tst_releases.moc"
