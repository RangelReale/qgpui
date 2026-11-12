#include "installer/KeepAwake.h"

#include "FakeAdbServer.h"

#include <qadb/Client.h>

#include <QSignalSpy>
#include <QTest>
#include <QTimer>

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
    QByteArray m_proximity; // What dumpsys vrpowermanager reports.
    bool m_failBroadcasts = false;

    int count(const char* action) const
    {
        return int(m_commands.count(QStringLiteral("am broadcast -a %1").arg(QString::fromLatin1(action))));
    }

    // Starts awake and waits until it has read the headset's state.
    static void startAndSettle(KeepAwake& awake)
    {
        awake.start();
        QTRY_VERIFY_WITH_TIMEOUT(awake.state() != KeepAwake::State::Checking, 5000);
    }

private slots:
    void init()
    {
        m_commands.clear();
        m_proximity = "DISABLED";
        m_failBroadcasts = false;
        m_server = new FakeAdbServer(this);
        QVERIFY(m_server->listen());
        m_server->setDevice(kSerial, {});
        m_server->setShellHandler([this](const QString&, const QString& command) {
            m_commands.append(command);
            if (command == QStringLiteral("dumpsys vrpowermanager"))
                return FakeAdbServer::ShellResult{"Virtual proximity state: " + m_proximity
                                                      + "\nisAutosleepDisabled: false\n",
                                                  {}, 0};
            if (m_failBroadcasts)
                return FakeAdbServer::ShellResult{{}, "Security exception", 255};
            return FakeAdbServer::ShellResult{};
        });
        m_client = new qadb::Client(qadb::ServerAddress{QStringLiteral("127.0.0.1"), m_server->port()}, this);
    }

    void cleanup()
    {
        delete m_client;
        delete m_server;
    }

    void readsTheProximityState()
    {
        QVERIFY(isProximityOverridden("Virtual proximity state: CLOSE\nState: HEADSET_MOUNTED\n"));
        QVERIFY(isProximityOverridden("Virtual proximity state: CLOSE"));
        QVERIFY(!isProximityOverridden("Virtual proximity state: DISABLED\n"));
        QVERIFY(!isProximityOverridden("Virtual proximity state: FAR\n"));
        QVERIFY(!isProximityOverridden("Can't find service: vrpowermanager\n"));
    }

    void startSendsTheOverrideAndRenewsIt()
    {
        KeepAwake awake(m_client, kSerial);
        awake.setRenewInterval(50);
        startAndSettle(awake);
        QCOMPARE(awake.state(), KeepAwake::State::Holding);
        QTRY_VERIFY_WITH_TIMEOUT(count(kKeepAwakeAction) >= 3, 5000);
        QCOMPARE(count(kReleaseAwakeAction), 0);
        QCOMPARE(m_commands.first(), QStringLiteral("dumpsys vrpowermanager"));
    }

    void stopReleasesTheSensor()
    {
        KeepAwake awake(m_client, kSerial);
        startAndSettle(awake);
        QVERIFY(awake.stop());
        QVERIFY(!awake.isActive());
        QTRY_COMPARE_WITH_TIMEOUT(count(kReleaseAwakeAction), 1, 5000);
        QCOMPARE(count(kKeepAwakeAction), 1);
        QVERIFY(!awake.stop()); // A second stop sends nothing.
        QTest::qWait(100);
        QCOMPARE(count(kReleaseAwakeAction), 1);
    }

    void destroyingReleasesTheSensor()
    {
        {
            KeepAwake awake(m_client, kSerial);
            startAndSettle(awake);
        }
        QTRY_COMPARE_WITH_TIMEOUT(count(kReleaseAwakeAction), 1, 5000);
    }

    void defersToAnotherAppAndTakesOverWhenItStops()
    {
        m_proximity = "CLOSE";
        KeepAwake awake(m_client, kSerial);
        awake.setRenewInterval(50);
        startAndSettle(awake);
        QCOMPARE(awake.state(), KeepAwake::State::Deferring);
        QTest::qWait(200); // Several checks, still deferring.
        QCOMPARE(awake.state(), KeepAwake::State::Deferring);
        QCOMPARE(count(kKeepAwakeAction), 0);

        m_proximity = "DISABLED"; // The other app let go.
        QTRY_COMPARE_WITH_TIMEOUT(awake.state(), KeepAwake::State::Holding, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(count(kKeepAwakeAction) >= 1, 5000);
    }

    void stopWhileDeferringLeavesTheOtherAppAlone()
    {
        m_proximity = "CLOSE";
        {
            KeepAwake awake(m_client, kSerial);
            startAndSettle(awake);
            QCOMPARE(awake.state(), KeepAwake::State::Deferring);
            QVERIFY(!awake.stop());
        }
        QTest::qWait(200);
        QCOMPARE(count(kKeepAwakeAction), 0);
        QCOMPARE(count(kReleaseAwakeAction), 0);
    }

    void stopWhileCheckingSendsNothing()
    {
        {
            KeepAwake awake(m_client, kSerial);
            awake.start();
            QCOMPARE(awake.state(), KeepAwake::State::Checking);
        }
        QTest::qWait(200);
        QCOMPARE(count(kKeepAwakeAction), 0);
        QCOMPARE(count(kReleaseAwakeAction), 0);
    }

    void abandonSendsNoRelease()
    {
        {
            KeepAwake awake(m_client, kSerial);
            startAndSettle(awake);
            awake.abandon();
            QVERIFY(!awake.isActive());
        }
        QTRY_COMPARE_WITH_TIMEOUT(count(kKeepAwakeAction), 1, 5000);
        QTest::qWait(100);
        QCOMPARE(count(kReleaseAwakeAction), 0);
    }

    void releaseAllReleasesEveryHeadsetAndWaits()
    {
        KeepAwake first(m_client, kSerial);
        KeepAwake second(m_client, kSerial);
        KeepAwake idle(m_client, kSerial);
        startAndSettle(first);
        startAndSettle(second);
        KeepAwake::releaseAll(5000);
        // releaseAll returns once the headset answered, so no waiting here.
        QCOMPARE(count(kReleaseAwakeAction), 2);
        QVERIFY(!first.isActive());
        QVERIFY(!second.isActive());
    }

    void releaseAllWorksAfterTheEventLoopEnded()
    {
        KeepAwake awake(m_client, kSerial);
        startAndSettle(awake);
        // As in qgp's main(): the application's loop has run and quit.
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        QCoreApplication::exec();
        KeepAwake::releaseAll(5000);
        QCOMPARE(count(kReleaseAwakeAction), 1);
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
        m_failBroadcasts = true;
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
