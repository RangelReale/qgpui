#include "FakeAdbServer.h"

#include "qadb/Client.h"
#include "qadb/DeviceTracker.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace qadb;

namespace {

const QString kSerial = QStringLiteral("2G0YC1ZF8B0SPR");

QByteArray pattern(qsizetype size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (qsizetype i = 0; i < size; ++i)
        data[i] = char((i * 31 + 7) & 0xff);
    return data;
}

} // namespace

class TestClient : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<FakeAdbServer> server;
    std::unique_ptr<Client> client;

    ServerAddress address() const { return ServerAddress{QStringLiteral("127.0.0.1"), server->port()}; }

private slots:
    void init()
    {
        server = std::make_unique<FakeAdbServer>();
        QVERIFY(server->listen());
        server->setDevice(kSerial, {});
        server->setShellHandler([](const QString&, const QString& command) {
            FakeAdbServer::ShellResult result;
            if (command == QLatin1String("getprop ro.product.model")) {
                result.out = "Quest 3\n";
            } else if (command == QLatin1String("ls /nope")) {
                result.err = "ls: /nope: No such file or directory\n";
                result.exitCode = 1;
            } else if (command.startsWith(QLatin1String("pm install"))) {
                result.out = "Success\n";
            } else if (command.startsWith(QLatin1String("pm uninstall"))) {
                result.out = command.endsWith(QLatin1String("missing.app"))
                                 ? "Failure [DELETE_FAILED_INTERNAL_ERROR]\n"
                                 : "Success\n";
            } else if (command.startsWith(QLatin1String("echo "))) {
                result.out = command.mid(5).toUtf8() + '\n';
            }
            return result;
        });
        client = std::make_unique<Client>(address());
    }

    void cleanup()
    {
        client.reset();
        server.reset();
    }

    void serverVersion()
    {
        std::unique_ptr<Reply> reply(client->serverVersion());
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(reply->outputText(), QStringLiteral("41"));
    }

    void devices()
    {
        FakeAdbServer::Device wireless;
        wireless.state = QStringLiteral("offline");
        server->setDevice(QStringLiteral("192.168.1.20:5555"), wireless);

        std::unique_ptr<DevicesReply> reply(client->devices());
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        const auto devices = reply->devices();
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].serial, QStringLiteral("192.168.1.20:5555"));
        QCOMPARE(devices[0].state, DeviceState::Offline);
        QCOMPARE(devices[1].serial, kSerial);
        QCOMPARE(devices[1].model, QStringLiteral("Quest_3"));
        QCOMPARE(devices[1].state, DeviceState::Device);
    }

    void connectionRefused()
    {
        const quint16 port = server->port();
        server->close();
        Client other(ServerAddress{QStringLiteral("127.0.0.1"), port});
        std::unique_ptr<Reply> reply(other.serverVersion());
        QVERIFY(reply->waitForFinished(10000));
        QCOMPARE(reply->error(), Reply::Error::ConnectionFailed);
    }

    void finishedIsEmittedAfterReturn()
    {
        std::unique_ptr<Reply> reply(client->serverVersion());
        QSignalSpy spy(reply.get(), &Reply::finished);
        QVERIFY(spy.wait(5000));
        QCOMPARE(spy.count(), 1);
    }

    void connectDevice()
    {
        std::unique_ptr<Reply> ok(client->connectDevice(QStringLiteral("192.168.1.20")));
        QVERIFY(ok->waitForFinished(5000));
        QVERIFY2(ok->isSuccess(), qPrintable(ok->errorString()));
        QCOMPARE(server->requests.last(), QStringLiteral("host:connect:192.168.1.20:5555"));

        std::unique_ptr<Reply> bad(client->connectDevice(QStringLiteral("bad.host"), 1234));
        QVERIFY(bad->waitForFinished(5000));
        QCOMPARE(bad->error(), Reply::Error::CommandFailed);
        QVERIFY(bad->errorString().contains(QLatin1String("failed to connect")));
    }

    void pair()
    {
        std::unique_ptr<Reply> ok(client->pair(QStringLiteral("192.168.1.20:41234"), QStringLiteral(" 123456 ")));
        QVERIFY(ok->waitForFinished(5000));
        QVERIFY2(ok->isSuccess(), qPrintable(ok->errorString()));
        QCOMPARE(server->requests.last(), QStringLiteral("host:pair:123456:192.168.1.20:41234"));
        QVERIFY(ok->outputText().contains(QLatin1String("guid=adb-FAKE1234")));

        std::unique_ptr<Reply> wrong(client->pair(QStringLiteral("192.168.1.20:41234"), QStringLiteral("000000")));
        QVERIFY(wrong->waitForFinished(5000));
        QCOMPARE(wrong->error(), Reply::Error::CommandFailed);
        QVERIFY(wrong->errorString().startsWith(QLatin1String("Failed: Wrong password")));

        const qsizetype requestCount = server->requests.size();
        std::unique_ptr<Reply> empty(client->pair(QStringLiteral("192.168.1.20:41234"), QString()));
        QVERIFY(empty->waitForFinished(5000));
        QCOMPARE(empty->error(), Reply::Error::CommandFailed);
        QCOMPARE(server->requests.size(), requestCount);
    }

    void mdnsServices()
    {
        std::unique_ptr<MdnsReply> reply(client->mdnsServices());
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY(reply->isSuccess());
        QCOMPARE(reply->services().size(), 1);
        QCOMPARE(reply->services()[0].address, QStringLiteral("192.168.1.20:37099"));
    }

    void featuresAreCached()
    {
        std::unique_ptr<FeaturesReply> first(client->features(kSerial));
        QVERIFY(first->waitForFinished(5000));
        QVERIFY(first->hasFeature(QStringLiteral("shell_v2")));
        const qsizetype requestCount = server->requests.size();

        std::unique_ptr<FeaturesReply> second(client->features(kSerial));
        QVERIFY(second->waitForFinished(5000));
        QVERIFY(second->hasFeature(QStringLiteral("cmd")));
        QCOMPARE(server->requests.size(), requestCount);
    }

    void shellV2()
    {
        std::unique_ptr<ShellReply> reply(
            client->shell(kSerial, QStringLiteral("getprop ro.product.model")));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(reply->output(), QByteArray("Quest 3\n"));
        QCOMPARE(reply->exitCode(), 0);
        QVERIFY(server->requests.contains(kSerial + QStringLiteral("|shell,v2,raw:getprop ro.product.model")));
    }

    void shellV2ExitCodeAndStderr()
    {
        std::unique_ptr<ShellReply> reply(client->shell(kSerial, QStringLiteral("ls /nope")));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY(reply->isSuccess());
        QCOMPARE(reply->exitCode(), 1);
        QVERIFY(reply->output().isEmpty());
        QVERIFY(reply->standardError().contains("No such file"));
    }

    void shellQuotesArguments()
    {
        std::unique_ptr<ShellReply> reply(
            client->shell(kSerial, QStringList{QStringLiteral("echo"), QStringLiteral("a b")}));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->output(), QByteArray("'a b'\n"));
    }

    void shellLegacy()
    {
        FakeAdbServer::Device old;
        old.features = {};
        server->setDevice(kSerial, old);

        std::unique_ptr<ShellReply> reply(
            client->shell(kSerial, QStringLiteral("getprop ro.product.model")));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(reply->output(), QByteArray("Quest 3\n"));
        QVERIFY(!reply->hasExitCode());
        QVERIFY(server->requests.contains(kSerial + QStringLiteral("|shell:getprop ro.product.model")));
    }

    void unknownDevice()
    {
        std::unique_ptr<Reply> reply(client->exec(QStringLiteral("nope"), QStringLiteral("ls")));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::ServerError);
        QVERIFY(reply->errorString().contains(QLatin1String("not found")));
    }

    void anyDeviceWithEmptySerial()
    {
        std::unique_ptr<ShellReply> reply(client->shell(QString(), QStringLiteral("getprop ro.product.model")));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(reply->output(), QByteArray("Quest 3\n"));
    }

    void enableTcpip()
    {
        std::unique_ptr<Reply> reply(client->enableTcpip(kSerial, 5555));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY(reply->isSuccess());
        QVERIFY(reply->outputText().contains(QLatin1String("5555")));
    }

    void pushAndPull()
    {
        const QByteArray data = pattern(200 * 1024 + 123);
        std::unique_ptr<Reply> push(
            client->pushData(kSerial, data, QStringLiteral("/sdcard/Download/blob.bin")));
        QSignalSpy progress(push.get(), &Reply::progress);
        QVERIFY(push->waitForFinished(5000));
        QVERIFY2(push->isSuccess(), qPrintable(push->errorString()));
        QCOMPARE(server->files.value(QStringLiteral("/sdcard/Download/blob.bin")), data);
        QVERIFY(!progress.isEmpty());
        QCOMPARE(progress.last().at(0).toLongLong(), data.size());
        QCOMPARE(progress.last().at(1).toLongLong(), data.size());

        QTemporaryDir dir;
        const QString local = dir.filePath(QStringLiteral("blob.bin"));
        std::unique_ptr<Reply> pull(
            client->pull(kSerial, QStringLiteral("/sdcard/Download/blob.bin"), local));
        QVERIFY(pull->waitForFinished(5000));
        QVERIFY2(pull->isSuccess(), qPrintable(pull->errorString()));
        QFile file(local);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
    }

    void pushLocalFile()
    {
        QTemporaryDir dir;
        const QString local = dir.filePath(QStringLiteral("save.dat"));
        QFile file(local);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("savegame");
        file.close();

        std::unique_ptr<Reply> reply(client->push(kSerial, local, QStringLiteral("/sdcard/save.dat")));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(server->files.value(QStringLiteral("/sdcard/save.dat")), QByteArray("savegame"));
    }

    void pushMissingLocalFile()
    {
        std::unique_ptr<Reply> reply(
            client->push(kSerial, QStringLiteral("Z:/does/not/exist.bin"), QStringLiteral("/sdcard/x")));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::FileError);
    }

    void pushRejected()
    {
        server->setReadOnlyPath(QStringLiteral("/system/x.bin"));
        std::unique_ptr<Reply> reply(
            client->pushData(kSerial, pattern(300 * 1024), QStringLiteral("/system/x.bin")));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::ServerError);
        QCOMPARE(reply->errorString(), QStringLiteral("Permission denied"));
    }

    void pullMissing()
    {
        QTemporaryDir dir;
        const QString local = dir.filePath(QStringLiteral("missing.bin"));
        std::unique_ptr<Reply> reply(client->pull(kSerial, QStringLiteral("/sdcard/missing"), local));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::ServerError);
        QVERIFY(!QFile::exists(local));
    }

    void statAndList()
    {
        server->files.insert(QStringLiteral("/sdcard/Android/obb/com.game/main.obb"), "obb!");
        server->files.insert(QStringLiteral("/sdcard/Android/obb/com.game/patch.obb"), "patch");
        server->files.insert(QStringLiteral("/sdcard/Android/obb/com.other/x.obb"), "x");

        std::unique_ptr<StatReply> file(
            client->stat(kSerial, QStringLiteral("/sdcard/Android/obb/com.game/main.obb")));
        QVERIFY(file->waitForFinished(5000));
        QVERIFY2(file->isSuccess(), qPrintable(file->errorString()));
        QVERIFY(file->info().isFile());
        QCOMPARE(file->info().size, 4u);

        std::unique_ptr<StatReply> missing(client->stat(kSerial, QStringLiteral("/sdcard/none")));
        QVERIFY(missing->waitForFinished(5000));
        QVERIFY(missing->isSuccess());
        QVERIFY(!missing->info().exists());

        std::unique_ptr<ListReply> list(client->list(kSerial, QStringLiteral("/sdcard/Android/obb")));
        QVERIFY(list->waitForFinished(5000));
        QVERIFY2(list->isSuccess(), qPrintable(list->errorString()));
        QCOMPARE(list->entries().size(), 2);
        QCOMPARE(list->entries()[0].name, QStringLiteral("com.game"));
        QVERIFY(list->entries()[0].isDir());
        QCOMPARE(list->entries()[1].name, QStringLiteral("com.other"));
    }

    void installStreamed()
    {
        QTemporaryDir dir;
        const QString apk = dir.filePath(QStringLiteral("My Game.apk"));
        const QByteArray data = pattern(150 * 1024);
        QFile file(apk);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(data);
        file.close();

        InstallOptions options;
        options.grantPermissions = true;
        std::unique_ptr<Reply> reply(client->install(kSerial, apk, options));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        QCOMPARE(server->installedApks.size(), 1);
        QCOMPARE(server->installedApks[0], data);
        QVERIFY(server->requests.contains(
            kSerial + QStringLiteral("|exec:cmd package install -S 153600 -r -g")));
    }

    void installStreamedFailure()
    {
        QTemporaryDir dir;
        const QString apk = dir.filePath(QStringLiteral("bad.apk"));
        QFile file(apk);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("BAD apk");
        file.close();

        std::unique_ptr<Reply> reply(client->install(kSerial, apk));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::CommandFailed);
        QVERIFY(reply->errorString().contains(QLatin1String("INSTALL_FAILED_INVALID_APK")));
    }

    void installLegacy()
    {
        FakeAdbServer::Device old;
        old.features = {QStringLiteral("shell_v2")};
        server->setDevice(kSerial, old);

        QTemporaryDir dir;
        const QString apk = dir.filePath(QStringLiteral("My Game.apk"));
        QFile file(apk);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(pattern(70 * 1024));
        file.close();

        std::unique_ptr<Reply> reply(client->install(kSerial, apk));
        QVERIFY(reply->waitForFinished(5000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->errorString()));
        const QString remote = QStringLiteral("/data/local/tmp/qadb-My_Game.apk");
        QCOMPARE(server->files.value(remote).size(), 70 * 1024);
        QVERIFY(server->requests.contains(kSerial + QStringLiteral("|shell,v2,raw:pm install -r ") + remote));
        QVERIFY(server->requests.contains(kSerial + QStringLiteral("|shell,v2,raw:rm -f ") + remote));
    }

    void installMissingFile()
    {
        std::unique_ptr<Reply> reply(client->install(kSerial, QStringLiteral("Z:/nope.apk")));
        QVERIFY(reply->waitForFinished(5000));
        QCOMPARE(reply->error(), Reply::Error::FileError);
    }

    void uninstall()
    {
        std::unique_ptr<Reply> ok(client->uninstall(kSerial, QStringLiteral("com.game")));
        QVERIFY(ok->waitForFinished(5000));
        QVERIFY2(ok->isSuccess(), qPrintable(ok->errorString()));

        std::unique_ptr<Reply> bad(client->uninstall(kSerial, QStringLiteral("missing.app")));
        QVERIFY(bad->waitForFinished(5000));
        QCOMPARE(bad->error(), Reply::Error::CommandFailed);
    }

    void abortPush()
    {
        std::unique_ptr<Reply> reply(
            client->pushData(kSerial, pattern(8 * 1024 * 1024), QStringLiteral("/sdcard/big.bin")));
        QSignalSpy finished(reply.get(), &Reply::finished);
        // Abort once the transfer has started.
        connect(reply.get(), &Reply::progress, reply.get(), [&] { reply->abort(); });
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(reply->error(), Reply::Error::Aborted);
    }

    void abortBeforeStart()
    {
        std::unique_ptr<Reply> reply(client->serverVersion());
        reply->abort();
        QVERIFY(reply->isFinished());
        QCOMPARE(reply->error(), Reply::Error::Aborted);
        QTest::qWait(50);
        QVERIFY(!server->requests.contains(QStringLiteral("host:version")));
    }

    void deleteWhileRunning()
    {
        Reply* reply = client->pushData(kSerial, pattern(4 * 1024 * 1024), QStringLiteral("/sdcard/x"));
        connect(reply, &Reply::progress, reply, [reply] { delete reply; });
        QTest::qWait(200);
    }

    void trackerFollowsDevices()
    {
        DeviceTracker tracker(address());
        QSignalSpy connected(&tracker, &DeviceTracker::connectedChanged);
        QSignalSpy added(&tracker, &DeviceTracker::deviceAdded);
        QSignalSpy removed(&tracker, &DeviceTracker::deviceRemoved);
        QSignalSpy changed(&tracker, &DeviceTracker::deviceChanged);
        tracker.start();

        QTRY_VERIFY_WITH_TIMEOUT(tracker.isConnected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(added.count(), 1, 5000);
        QCOMPARE(tracker.devices().value(0).model, QStringLiteral("Quest_3"));

        server->setDevice(QStringLiteral("192.168.1.20:5555"), {});
        QTRY_COMPARE_WITH_TIMEOUT(added.count(), 2, 5000);
        QCOMPARE(tracker.devices().size(), 2);

        FakeAdbServer::Device unauthorized;
        unauthorized.state = QStringLiteral("unauthorized");
        server->setDevice(kSerial, unauthorized);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 5000);
        QCOMPARE(changed.last().at(0).value<DeviceInfo>().state, DeviceState::Unauthorized);

        server->removeDevice(QStringLiteral("192.168.1.20:5555"));
        QTRY_COMPARE_WITH_TIMEOUT(removed.count(), 1, 5000);
        QCOMPARE(tracker.devices().size(), 1);
        QVERIFY(server->requests.contains(QStringLiteral("host:track-devices-l")));
    }

    void trackerFallsBackToShortList()
    {
        server->setTrackLongSupported(false);
        DeviceTracker tracker(address());
        tracker.start();
        QTRY_VERIFY_WITH_TIMEOUT(tracker.isConnected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(tracker.devices().size(), 1, 5000);
        QCOMPARE(tracker.devices()[0].serial, kSerial);
        QVERIFY(server->requests.contains(QStringLiteral("host:track-devices")));
    }

    void trackerReconnects()
    {
        DeviceTracker tracker(address());
        tracker.setRetryInterval(50);
        QSignalSpy errors(&tracker, &DeviceTracker::errorOccurred);
        tracker.start();
        QTRY_VERIFY_WITH_TIMEOUT(tracker.isConnected(), 5000);

        const quint16 port = server->port();
        server->close();
        QTRY_VERIFY_WITH_TIMEOUT(!tracker.isConnected(), 5000);
        QVERIFY(tracker.devices().isEmpty());
        QTest::qWait(300); // Several failed retries...
        QCOMPARE(errors.count(), 1); // ...reported once.

        QVERIFY(server->listen(port));
        QTRY_VERIFY_WITH_TIMEOUT(tracker.isConnected(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(tracker.devices().size(), 1, 5000);

        tracker.stop();
        QVERIFY(!tracker.isConnected());
    }
};

QTEST_GUILESS_MAIN(TestClient)
#include "tst_client.moc"
