#include "installer/Updates.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace installer;

namespace {

qadb::PackageInfo installed(qint64 versionCode, const QString& versionName)
{
    qadb::PackageInfo info;
    info.packageName = QStringLiteral("com.bilko.urtquest");
    info.versionCode = versionCode;
    info.versionName = versionName;
    return info;
}

InstallRecord record(const QString& tag, qint64 versionCode)
{
    InstallRecord r;
    r.packageName = QStringLiteral("com.bilko.urtquest");
    r.slug = QStringLiteral("urbanterror-vr");
    r.releaseTag = tag;
    r.asset = QStringLiteral("UrbanTerror4-VR-v1.0.1.apk");
    r.versionCode = versionCode;
    r.versionName = QStringLiteral("1.0.1");
    r.installedAt = QDateTime(QDate(2026, 11, 12), QTime(17, 11, 11), QTimeZone::UTC);
    return r;
}

} // namespace

class TestUpdates : public QObject
{
    Q_OBJECT

private slots:
    void readsVersionNumbers_data()
    {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QList<int>>("numbers");
        QTest::newRow("tag") << QStringLiteral("v1.0.1") << QList<int>{1, 0, 1};
        QTest::newRow("name") << QStringLiteral("1.0.1") << QList<int>{1, 0, 1};
        QTest::newRow("game prefix") << QStringLiteral("vcop2-v0.1.0") << QList<int>{0, 1, 0};
        QTest::newRow("suffix") << QStringLiteral("quest-v0.3-beta") << QList<int>{0, 3};
        QTest::newRow("build") << QStringLiteral("b003") << QList<int>{};
        QTest::newRow("platform") << QStringLiteral("n64") << QList<int>{};
        QTest::newRow("empty") << QString() << QList<int>{};
    }

    void readsVersionNumbers()
    {
        QFETCH(QString, label);
        QFETCH(QList<int>, numbers);
        QCOMPARE(versionNumbers(label), numbers);
    }

    void comparesVersions()
    {
        QCOMPARE(compareVersions(QStringLiteral("v1.0.2"), QStringLiteral("1.0.1")), VersionOrder::Newer);
        QCOMPARE(compareVersions(QStringLiteral("v1.0.1"), QStringLiteral("1.0.1")), VersionOrder::Same);
        QCOMPARE(compareVersions(QStringLiteral("v1.2"), QStringLiteral("1.2.0")), VersionOrder::Same);
        QCOMPARE(compareVersions(QStringLiteral("v1.10"), QStringLiteral("1.9")), VersionOrder::Newer);
        QCOMPARE(compareVersions(QStringLiteral("vcop-v0.2.3"), QStringLiteral("0.2.4")), VersionOrder::Older);
        QCOMPARE(compareVersions(QStringLiteral("b004"), QStringLiteral("1.0")), VersionOrder::Unknown);
    }

    void storesRecordsPerHeadset()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("installs.json"));

        QString error;
        InstallRecords records = InstallRecords::load(path, &error); // No file yet: empty, no error.
        QVERIFY(error.isEmpty());
        QVERIFY(!records.find(QStringLiteral("2G0YC5ZG4D040F"), QStringLiteral("com.bilko.urtquest")));
        records.set(QStringLiteral("2G0YC5ZG4D040F"), record(QStringLiteral("v1.0.1"), 101));
        records.set(QStringLiteral("OTHERQUEST"), record(QStringLiteral("v1.0.0"), 100));
        QVERIFY2(records.save(&error), qPrintable(error));

        const InstallRecords loaded = InstallRecords::load(path, &error);
        QVERIFY(error.isEmpty());
        const auto mine = loaded.find(QStringLiteral("2G0YC5ZG4D040F"), QStringLiteral("com.bilko.urtquest"));
        QVERIFY(mine);
        QCOMPARE(mine->slug, QStringLiteral("urbanterror-vr"));
        QCOMPARE(mine->releaseTag, QStringLiteral("v1.0.1"));
        QCOMPARE(mine->versionCode, 101);
        QCOMPARE(mine->installedAt, QDateTime(QDate(2026, 11, 12), QTime(17, 11, 11), QTimeZone::UTC));
        QCOMPARE(loaded.find(QStringLiteral("OTHERQUEST"), QStringLiteral("com.bilko.urtquest"))->versionCode, 100);

        InstallRecords edited = loaded;
        edited.remove(QStringLiteral("OTHERQUEST"), QStringLiteral("com.bilko.urtquest"));
        QVERIFY(!edited.find(QStringLiteral("OTHERQUEST"), QStringLiteral("com.bilko.urtquest")));
    }

    // The first save on a PC creates the folder (AppData/Roaming/qgp does not exist yet).
    void savesIntoAMissingFolder()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("qgp/new/installs.json"));
        InstallRecords records = InstallRecords::load(path);
        records.set(QStringLiteral("2G0YC5ZG4D040F"), record(QStringLiteral("v1.0.1"), 101));
        QString error;
        QVERIFY2(records.save(&error), qPrintable(error));
        QVERIFY(InstallRecords::load(path).find(QStringLiteral("2G0YC5ZG4D040F"), QStringLiteral("com.bilko.urtquest")));
    }

    void reportsAFileThatIsNotARecordFile()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("other.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"format\":\"something-else\"}");
        file.close();
        QString error;
        const InstallRecords records = InstallRecords::load(path, &error);
        QVERIFY(error.contains(QStringLiteral("not a qgp install record")));
        QVERIFY(!records.find(QStringLiteral("x"), QStringLiteral("y")));
    }

    void decidesTheUpdateStatus()
    {
        const QString v101 = QStringLiteral("v1.0.1");
        const QString v102 = QStringLiteral("v1.0.2");

        QCOMPARE(updateStatus(std::nullopt, std::nullopt, v101), UpdateStatus::NotInstalled);

        // Installed by qgp and unchanged: the tag decides.
        QCOMPARE(updateStatus(installed(101, QStringLiteral("1.0.1")), record(v101, 101), v101), UpdateStatus::UpToDate);
        QCOMPARE(updateStatus(installed(101, QStringLiteral("1.0.1")), record(v101, 101), v102),
                 UpdateStatus::UpdateAvailable);
        // Even when the tags carry no comparable version.
        QCOMPARE(updateStatus(installed(3, QStringLiteral("x")), record(QStringLiteral("b003"), 3), QStringLiteral("b004")),
                 UpdateStatus::UpdateAvailable);

        // The app updated itself since qgp installed it: compare with what the headset has.
        QCOMPARE(updateStatus(installed(103, QStringLiteral("1.0.3")), record(v101, 101), v102), UpdateStatus::UpToDate);

        // Installed elsewhere: compare versions, or say Unknown.
        QCOMPARE(updateStatus(installed(29, QStringLiteral("1.5.6")), std::nullopt, QStringLiteral("v1.6.0")),
                 UpdateStatus::UpdateAvailable);
        QCOMPARE(updateStatus(installed(29, QStringLiteral("1.6.0")), std::nullopt, QStringLiteral("v1.6.0")),
                 UpdateStatus::UpToDate);
        QCOMPARE(updateStatus(installed(29, QStringLiteral("dev")), std::nullopt, QStringLiteral("b003")),
                 UpdateStatus::Unknown);
    }
};

QTEST_GUILESS_MAIN(TestUpdates)
#include "tst_updates.moc"
