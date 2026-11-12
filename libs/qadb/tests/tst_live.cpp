// Checks against a real adb server, and optionally a real device. Skipped unless enabled:
//
//   QADB_LIVE=1            server requests (version, devices, tracker, mDNS)
//   QADB_LIVE_SERIAL=<s>   also read-only device requests on that device
//   QADB_LIVE_WRITE=1      also push, pull and remove a small file in /data/local/tmp
//
// The adb server must already be running (`adb start-server`).

#include "qadb/Client.h"
#include "qadb/DeviceTracker.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace qadb;

// QSKIP only leaves the function it is in, so these are macros.
#define REQUIRE_SERVER()                                                                              do {                                                                                                  if (qEnvironmentVariable("QADB_LIVE") != QLatin1String("1") && serial.isEmpty())                     QSKIP("Set QADB_LIVE=1 to run against the real adb server");                              } while (false)

#define REQUIRE_DEVICE()                                                                              do {                                                                                                  REQUIRE_SERVER();                                                                                 if (serial.isEmpty())                                                                                 QSKIP("Set QADB_LIVE_SERIAL to run device requests");                                     } while (false)

class TestLive : public QObject
{
    Q_OBJECT

private:
    Client client;
    QString serial = qEnvironmentVariable("QADB_LIVE_SERIAL");

private slots:
    void version()
    {
        REQUIRE_SERVER();
        std::unique_ptr<Reply> reply(client.serverVersion());
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        qInfo("server version %s", qPrintable(reply->outputText()));
        QVERIFY(reply->outputText().toInt() > 30);
    }

    void devices()
    {
        REQUIRE_SERVER();
        std::unique_ptr<DevicesReply> reply(client.devices());
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        for (const DeviceInfo& d : reply->devices())
            qInfo("device %s %s %s", qPrintable(d.serial), qPrintable(d.stateText), qPrintable(d.model));
    }

    void tracker()
    {
        REQUIRE_SERVER();
        DeviceTracker tracker;
        QSignalSpy lists(&tracker, &DeviceTracker::connectedChanged);
        tracker.start();
        QTRY_VERIFY_WITH_TIMEOUT(tracker.isConnected(), 5000);
        // Give the first list time to arrive; it may be empty.
        QTest::qWait(500);
        qInfo("tracker sees %d device(s)", int(tracker.devices().size()));
    }

    void mdns()
    {
        REQUIRE_SERVER();
        std::unique_ptr<MdnsReply> reply(client.mdnsServices());
        QVERIFY(reply->waitForFinished(5000));
        // Servers built without mDNS answer FAIL; that is not a library error.
        if (!reply->isSuccess())
            QSKIP(qPrintable(QStringLiteral("mDNS unavailable: ") + reply->errorString()));
        qInfo("%d mDNS service(s)", int(reply->services().size()));
    }

    // A wrong code against an address with nothing listening: checks that the real server's
    // answer is reported as CommandFailed. Pairs with nothing.
    void pairFailure()
    {
        REQUIRE_SERVER();
        std::unique_ptr<Reply> reply(client.pair(QStringLiteral("127.0.0.1:1"), QStringLiteral("000000")));
        QVERIFY(reply->waitForFinished(30000));
        qInfo("pair answered: %s", qPrintable(reply->errorString()));
        QCOMPARE(reply->error(), Reply::Error::CommandFailed);
    }

    void unknownDevice()
    {
        REQUIRE_SERVER();
        std::unique_ptr<ShellReply> reply(client.shell(QStringLiteral("qadb-no-such-device"), QStringLiteral("true")));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::ServerError);
        qInfo("server says: %s", qPrintable(reply->errorString()));
    }

    void deviceShell()
    {
        REQUIRE_DEVICE();
        std::unique_ptr<FeaturesReply> features(client.features(serial));
        QVERIFY(features->waitForFinished(5000));
        QVERIFY2(features->isSuccess(), qPrintable(features->errorString()));
        qInfo("features: %s", qPrintable(features->features().join(QLatin1Char(','))));

        std::unique_ptr<ShellReply> model(client.shell(serial, QStringLiteral("getprop ro.product.model")));
        QVERIFY(model->waitForFinished(10000));
        QVERIFY2(model->isSuccess(), qPrintable(model->errorString()));
        qInfo("model: %s", model->output().trimmed().constData());
        QVERIFY(!model->output().trimmed().isEmpty());

        std::unique_ptr<ShellReply> failing(client.shell(serial, QStringLiteral("ls /qadb-does-not-exist")));
        QVERIFY(failing->waitForFinished(10000));
        QVERIFY(failing->isSuccess());
        if (failing->hasExitCode())
            QVERIFY(failing->exitCode() != 0);
    }

    void deviceFiles()
    {
        REQUIRE_DEVICE();
        std::unique_ptr<StatReply> stat(client.stat(serial, QStringLiteral("/sdcard")));
        QVERIFY(stat->waitForFinished(10000));
        QVERIFY2(stat->isSuccess(), qPrintable(stat->errorString()));
        QVERIFY(stat->info().exists());

        std::unique_ptr<ListReply> list(client.list(serial, QStringLiteral("/sdcard")));
        QVERIFY(list->waitForFinished(10000));
        QVERIFY2(list->isSuccess(), qPrintable(list->errorString()));
        qInfo("/sdcard has %d entries", int(list->entries().size()));
    }

    // Streams a file that is not an APK. The package manager must reject it, so nothing changes
    // on the device, but the whole streamed install path runs.
    void deviceInstallRejectsInvalidApk()
    {
        REQUIRE_DEVICE();
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("not-an-apk.apk"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(100 * 1024, 'x'));
        file.close();

        std::unique_ptr<Reply> reply(client.install(serial, path));
        QSignalSpy progress(reply.get(), &Reply::progress);
        QVERIFY(reply->waitForFinished(60000));
        qInfo("install answered: %s", qPrintable(reply->errorString()));
        QCOMPARE(reply->error(), Reply::Error::CommandFailed);
        QVERIFY(reply->errorString().contains(QLatin1String("Failure")));
        QVERIFY(!progress.isEmpty());
        QCOMPARE(progress.last().at(0).toLongLong(), 100 * 1024);
    }

    void deviceUninstallUnknownPackage()
    {
        REQUIRE_DEVICE();
        std::unique_ptr<Reply> reply(client.uninstall(serial, QStringLiteral("com.qadb.does.not.exist")));
        QVERIFY(reply->waitForFinished(30000));
        qInfo("uninstall answered: %s", qPrintable(reply->errorString()));
        QCOMPARE(reply->error(), Reply::Error::CommandFailed);
    }

    void devicePackagesStorageAndSerial()
    {
        REQUIRE_DEVICE();
        std::unique_ptr<PackagesReply> packages(client.packages(serial));
        QVERIFY(packages->waitForFinished(30000));
        QVERIFY2(packages->isSuccess(), qPrintable(packages->errorString()));
        qInfo("%lld third-party packages", qlonglong(packages->packages().size()));
        QVERIFY(!packages->packages().isEmpty());

        // The first package's details agree with the list.
        const PackageInfo first = packages->packages().first();
        std::unique_ptr<PackageInfoReply> info(client.packageInfo(serial, first.packageName));
        QVERIFY(info->waitForFinished(30000));
        QVERIFY(info->isInstalled());
        qInfo("%s versionCode %lld versionName %s, updated %s", qPrintable(first.packageName),
              qlonglong(info->info().versionCode), qPrintable(info->info().versionName),
              qPrintable(info->info().lastUpdateTime.toString(Qt::ISODate)));
        QCOMPARE(info->info().versionCode, first.versionCode);

        std::unique_ptr<PackageInfoReply> missing(client.packageInfo(serial, QStringLiteral("com.qadb.does.not.exist")));
        QVERIFY(missing->waitForFinished(30000));
        QVERIFY(missing->isSuccess());
        QVERIFY(!missing->isInstalled());

        std::unique_ptr<StorageReply> storage(client.storage(serial));
        QVERIFY(storage->waitForFinished(30000));
        QVERIFY2(storage->isSuccess(), qPrintable(storage->errorString()));
        qInfo("storage: %lld MB free of %lld MB", qlonglong(storage->info().freeBytes >> 20),
              qlonglong(storage->info().totalBytes >> 20));
        QVERIFY(storage->info().freeBytes > 0);
        QVERIFY(storage->info().totalBytes >= storage->info().freeBytes);

        std::unique_ptr<Reply> hardware(client.getProperty(serial, QStringLiteral("ro.serialno")));
        QVERIFY(hardware->waitForFinished(30000));
        qInfo("ro.serialno: %s", qPrintable(hardware->outputText()));
        QVERIFY(!hardware->outputText().isEmpty());
    }

    void deviceWrite()
    {
        REQUIRE_DEVICE();
        if (qEnvironmentVariable("QADB_LIVE_WRITE") != QLatin1String("1"))
            QSKIP("Set QADB_LIVE_WRITE=1 to push and pull a test file");

        const QString remote = QStringLiteral("/data/local/tmp/qadb-live-test.bin");
        QByteArray data(300 * 1024, Qt::Uninitialized);
        for (qsizetype i = 0; i < data.size(); ++i)
            data[i] = char(i * 13);

        std::unique_ptr<Reply> push(client.pushData(serial, data, remote));
        QVERIFY(push->waitForFinished(30000));
        QVERIFY2(push->isSuccess(), qPrintable(push->errorString()));

        QTemporaryDir dir;
        const QString local = dir.filePath(QStringLiteral("back.bin"));
        std::unique_ptr<Reply> pull(client.pull(serial, remote, local));
        QVERIFY(pull->waitForFinished(30000));
        QVERIFY2(pull->isSuccess(), qPrintable(pull->errorString()));
        QFile file(local);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);

        std::unique_ptr<ShellReply> rm(client.shell(serial, QStringList{QStringLiteral("rm"), remote}));
        QVERIFY(rm->waitForFinished(10000));
        QCOMPARE(rm->exitCode(), 0);
    }
};

QTEST_GUILESS_MAIN(TestLive)
#include "tst_live.moc"
