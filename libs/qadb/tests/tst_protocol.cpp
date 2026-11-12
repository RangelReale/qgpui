#include "qadb/Protocol.h"

#include <QTest>

using namespace qadb;

class TestProtocol : public QObject
{
    Q_OBJECT

private slots:
    void encodeRequest()
    {
        QCOMPARE(protocol::encodeRequest("host:version"), QByteArray("000chost:version"));
        QCOMPARE(protocol::encodeRequest(QByteArray(300, 'x')).left(4), QByteArray("012c"));
    }

    void parseHexLength()
    {
        QCOMPARE(protocol::parseHexLength("001f"), 31);
        QCOMPARE(protocol::parseHexLength("FFFF"), 65535);
        QCOMPARE(protocol::parseHexLength("12"), -1);
        QCOMPARE(protocol::parseHexLength("zz00"), -1);
    }

    void parseShortDeviceList()
    {
        const auto devices =
            protocol::parseDeviceList("2G0YC1ZF8B0SPR\tdevice\n192.168.1.20:5555\toffline\n");
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].serial, QStringLiteral("2G0YC1ZF8B0SPR"));
        QCOMPARE(devices[0].state, DeviceState::Device);
        QVERIFY(!devices[0].isNetwork());
        QCOMPARE(devices[1].state, DeviceState::Offline);
        QVERIFY(devices[1].isNetwork());
    }

    void parseLongDeviceList()
    {
        const auto devices = protocol::parseDeviceList(
            "2G0YC1ZF8B0SPR         device usb:1-4 product:eureka model:Quest_3 device:eureka "
            "transport_id:7\n"
            "R3CN90ABCDE            unauthorized usb:1-2 transport_id:8\n");
        QCOMPARE(devices.size(), 2);
        QCOMPARE(devices[0].model, QStringLiteral("Quest_3"));
        QCOMPARE(devices[0].product, QStringLiteral("eureka"));
        QCOMPARE(devices[0].device, QStringLiteral("eureka"));
        QCOMPARE(devices[0].usb, QStringLiteral("1-4"));
        QCOMPARE(devices[0].transportId, 7);
        QCOMPARE(devices[1].state, DeviceState::Unauthorized);
        QCOMPARE(devices[1].transportId, 8);
    }

    void parseNoPermissions()
    {
        const auto devices = protocol::parseDeviceList(
            "0123456789ABCDEF       no permissions (missing udev rules? user is in the plugdev "
            "group); see [http://developer.android.com/tools/device.html] usb:3-1 "
            "transport_id:2\n");
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices[0].state, DeviceState::NoPermissions);
        QVERIFY(devices[0].stateText.startsWith(QLatin1String("no permissions (")));
        QCOMPARE(devices[0].usb, QStringLiteral("3-1"));
    }

    void parseEmptyDeviceList() { QVERIFY(protocol::parseDeviceList("").isEmpty()); }

    void parseMdnsServices()
    {
        const auto services = protocol::parseMdnsServices(
            "adb-2G0Y-abc\t_adb-tls-connect._tcp\t192.168.1.20:37099\n");
        QCOMPARE(services.size(), 1);
        QCOMPARE(services[0].name, QStringLiteral("adb-2G0Y-abc"));
        QCOMPARE(services[0].type, QStringLiteral("_adb-tls-connect._tcp"));
        QCOMPARE(services[0].address, QStringLiteral("192.168.1.20:37099"));
    }

    void parseFeatures()
    {
        QCOMPARE(protocol::parseFeatures("shell_v2,cmd,,stat_v2"),
                 QStringList({QStringLiteral("shell_v2"), QStringLiteral("cmd"),
                              QStringLiteral("stat_v2")}));
    }

    void shellQuote()
    {
        QCOMPARE(protocol::shellQuote(QStringLiteral("/sdcard/Download/a.apk")),
                 QStringLiteral("/sdcard/Download/a.apk"));
        QCOMPARE(protocol::shellQuote(QStringLiteral("My Game.apk")),
                 QStringLiteral("'My Game.apk'"));
        QCOMPARE(protocol::shellQuote(QStringLiteral("it's")), QStringLiteral("'it'\\''s'"));
        QCOMPARE(protocol::shellQuote(QString()), QStringLiteral("''"));
        QCOMPARE(protocol::shellCommand({QStringLiteral("ls"), QStringLiteral("-l"),
                                         QStringLiteral("a b")}),
                 QStringLiteral("ls -l 'a b'"));
    }

    void syncPacket()
    {
        QCOMPARE(protocol::syncPacket("STAT", "/sdcard"), QByteArray("STAT\x07\0\0\0/sdcard", 15));
        QCOMPARE(protocol::readLittleEndian32("\x01\x02\0\0"), 0x0201u);
        QCOMPARE(protocol::shellPacket(protocol::ShellCloseStdin), QByteArray("\x04\0\0\0\0", 5));
    }

    void parsePackageList()
    {
        const auto packages = protocol::parsePackageList("package:com.bilko.urtquest versionCode:101\r\n"
                                                         "package:com.drbeef.lambda1vr versionCode:29\n"
                                                         "package:com.no.version\n"
                                                         "\n");
        QCOMPARE(packages.size(), 3);
        QCOMPARE(packages[0].packageName, QStringLiteral("com.bilko.urtquest"));
        QCOMPARE(packages[0].versionCode, 101);
        QCOMPARE(packages[1].versionCode, 29);
        QCOMPARE(packages[2].packageName, QStringLiteral("com.no.version"));
        QCOMPARE(packages[2].versionCode, -1);
        QVERIFY(protocol::parsePackageList("").isEmpty());
    }

    void parsePackageDump()
    {
        // Cut from a Quest 3's `dumpsys package com.bilko.urtquest`.
        const QByteArray dump = "Key Set Manager:\n"
                                "  [com.bilko.urtquest]\n"
                                "      Signing KeySets: 61\n"
                                "\n"
                                "Packages:\n"
                                "  Package [com.bilko.urtquest] (cf029d2):\n"
                                "    appId=10301\n"
                                "    versionCode=101 minSdk=25 targetSdk=34\n"
                                "    versionName=1.0.1\n"
                                "    lastUpdateTime=2026-11-12 17:11:11\n"
                                "    User 0: ceDataInode=1 installed=true hidden=false\n"
                                "      firstInstallTime=2026-11-12 17:11:11\n"
                                "  Package [com.other] (abc):\n"
                                "    versionCode=7 minSdk=1 targetSdk=1\n"
                                "\n"
                                "Queries:\n"
                                "  versionCode=999\n";
        const auto info = protocol::parsePackageDump(QStringLiteral("com.bilko.urtquest"), dump);
        QVERIFY(info);
        QCOMPARE(info->versionCode, 101);
        QCOMPARE(info->versionName, QStringLiteral("1.0.1"));
        QCOMPARE(info->lastUpdateTime, QDateTime(QDate(2026, 11, 12), QTime(17, 11, 11)));
        QCOMPARE(info->firstInstallTime, QDateTime(QDate(2026, 11, 12), QTime(17, 11, 11)));

        const auto other = protocol::parsePackageDump(QStringLiteral("com.other"), dump);
        QVERIFY(other);
        QCOMPARE(other->versionCode, 7); // Not the 999 under Queries.

        QVERIFY(!protocol::parsePackageDump(QStringLiteral("com.example.notinstalled"),
                                            "Unable to find package: com.example.notinstalled\n"));
    }

    void parseStatFs()
    {
        const auto info = protocol::parseStatFs("4096 117550775 15477877\n");
        QVERIFY(info);
        QCOMPARE(info->totalBytes, qint64(4096) * 117550775);
        QCOMPARE(info->freeBytes, qint64(4096) * 15477877);
        QVERIFY(!protocol::parseStatFs("stat: /nope: No such file or directory"));
        QVERIFY(!protocol::parseStatFs(""));
    }
};

QTEST_GUILESS_MAIN(TestProtocol)
#include "tst_protocol.moc"
