#include "installer/KeepAwake.h"

#include "FakeAdbServer.h"

#include <qadb/Client.h>

#include <QSignalSpy>
#include <QTest>

using namespace installer;

namespace {
const QString kSerial = QStringLiteral("192.168.68.57:5555");
} // namespace

class TestKeepAwake : public QObject
{
    Q_OBJECT

private:
    FakeAdbServer* m_server = nullptr;
    qadb::Client* m_client = nullptr;
    QStringList m_commands;

    int count(const char* action) const
    {
        return int(m_commands.count(QStringLiteral("am broadcast -a %1").arg(QString::fromLatin1(action))));
    }

private slots:
    void init()
    {
        m_commands.clear();
        m_server = new FakeAdbServer(this);
        QVERIFY(m_server->listen());
        m_server->setDevice(kSerial, {});
        m_server->setShellHandler([this](const QString&, const QString& command) {
            m_commands.append(command);
            return FakeAdbServer::ShellResult{};
        });
        m_client = new qadb::Client(qadb::ServerAddress{QStringLiteral("127.0.0.1"), m_server->port()}, this);
    }

    void cleanup()
    {
        delete m_client;
        delete m_server;
    }

    void startSendsTheOverrideAndRenewsIt()
    {
        KeepAwake awake(m_client, kSerial);
        awake.setRenewInterval(50);
        awake.start();
        QVERIFY(awake.isActive());
        QTRY_VERIFY_WITH_TIMEOUT(count(kKeepAwakeAction) >= 3, 5000);
        QCOMPARE(count(kReleaseAwakeAction), 0);
    }

    void stopReleasesTheSensor()
    {
        KeepAwake awake(m_client, kSerial);
        awake.start();
        awake.stop();
        QVERIFY(!awake.isActive());
        QTRY_COMPARE_WITH_TIMEOUT(count(kReleaseAwakeAction), 1, 5000);
        QCOMPARE(count(kKeepAwakeAction), 1);
        awake.stop(); // A second stop sends nothing.
        QTest::qWait(100);
        QCOMPARE(count(kReleaseAwakeAction), 1);
    }

    void destroyingReleasesTheSensor()
    {
        {
            KeepAwake awake(m_client, kSerial);
            awake.start();
        }
        QTRY_COMPARE_WITH_TIMEOUT(count(kReleaseAwakeAction), 1, 5000);
    }

    void stopWithoutStartSendsNothing()
    {
        {
            KeepAwake awake(m_client, kSerial);
        }
        QTest::qWait(100);
        QVERIFY(m_commands.isEmpty());
    }

    void reportsAFailureOnce()
    {
        m_server->setShellHandler([](const QString&, const QString&) {
            return FakeAdbServer::ShellResult{{}, "Security exception", 255};
        });
        KeepAwake awake(m_client, kSerial);
        awake.setRenewInterval(50);
        QSignalSpy failed(&awake, &KeepAwake::failed);
        awake.start();
        QVERIFY(failed.wait(5000));
        QTest::qWait(300);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Security exception"));
    }
};

QTEST_GUILESS_MAIN(TestKeepAwake)
#include "tst_keepawake.moc"
