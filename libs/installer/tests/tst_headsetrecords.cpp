#include "installer/HeadsetRecords.h"

#include "FakeAdbServer.h"

#include <qadb/Client.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace installer;

namespace {

const QString kAdbSerial = QStringLiteral("192.168.1.20:5555");
const QString kHardware = QStringLiteral("2G0YC00000000B");
const QString kPath = QString::fromLatin1(kHeadsetRecordsPath);

InstallRecord record(const QString& package, const QString& tag, int day)
{
    InstallRecord r;
    r.packageName = package;
    r.slug = package.section(u'.', -1);
    r.releaseTag = tag;
    r.asset = r.slug + QStringLiteral(".apk");
    r.versionCode = day;
    r.versionName = tag.mid(1);
    r.installedAt = QDateTime(QDate(2026, 11, day), QTime(12, 0), QTimeZone::UTC);
    return r;
}

} // namespace

class TestHeadsetRecords : public QObject
{
    Q_OBJECT

private:
    FakeAdbServer* m_server = nullptr;
    qadb::Client* m_client = nullptr;
    QTemporaryDir m_tmp;
    QString m_cachePath;

    bool run(InstallRecords& cache, const std::optional<InstallRecord>& added, QString* error = nullptr,
             const QString& serial = kAdbSerial, QList<InstallRecord>* records = nullptr)
    {
        HeadsetRecordSync sync(m_client, serial, kHardware, &cache);
        QSignalSpy finished(&sync, &HeadsetRecordSync::finished);
        sync.run(added);
        if (!finished.wait(5000))
            return false;
        if (error)
            *error = finished.at(0).at(1).toString();
        if (records)
            *records = sync.records();
        return finished.at(0).at(0).toBool();
    }

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_server = new FakeAdbServer(this);
        QVERIFY(m_server->listen());
        m_server->setDevice(kAdbSerial, {});
        m_client = new qadb::Client(qadb::ServerAddress{QStringLiteral("127.0.0.1"), m_server->port()}, this);
        m_cachePath = m_tmp.filePath(QStringLiteral("cache-%1.json").arg(QDateTime::currentMSecsSinceEpoch()));
    }

    void cleanup()
    {
        delete m_client;
        delete m_server;
    }

    void writesTheFirstRecordToTheHeadsetAndTheCache()
    {
        InstallRecords cache = InstallRecords::load(m_cachePath);
        QString error;
        QVERIFY2(run(cache, record(QStringLiteral("com.bilko.urtquest"), QStringLiteral("v1.0.1"), 12), &error),
                 qPrintable(error));

        const auto onHeadset = parseHeadsetRecords(m_server->files.value(kPath), &error);
        QVERIFY2(onHeadset, qPrintable(error));
        QCOMPARE(onHeadset->size(), 1);
        QCOMPARE(onHeadset->at(0).releaseTag, QStringLiteral("v1.0.1"));
        QVERIFY(m_server->files.value(kPath).contains(kHardware.toUtf8()));

        const InstallRecords saved = InstallRecords::load(m_cachePath);
        QVERIFY(saved.find(kHardware, QStringLiteral("com.bilko.urtquest")));
    }

    void joinsTheHeadsetAndTheCache()
    {
        // The headset knows a newer urtquest; the cache knows an older one and a game the headset
        // does not list, for example from a PC that wrote while the file was being replaced.
        m_server->files.insert(kPath, headsetRecordsJson(kHardware, {record(QStringLiteral("com.bilko.urtquest"),
                                                                            QStringLiteral("v1.0.2"), 14)}));
        InstallRecords cache = InstallRecords::load(m_cachePath);
        cache.set(kHardware, record(QStringLiteral("com.bilko.urtquest"), QStringLiteral("v1.0.1"), 12));
        cache.set(kHardware, record(QStringLiteral("com.drbeef.quakequest"), QStringLiteral("v1.6.0"), 13));

        QList<InstallRecord> records;
        QString error;
        QVERIFY2(run(cache, std::nullopt, &error, kAdbSerial, &records), qPrintable(error));
        QCOMPARE(records.size(), 2);
        QCOMPARE(records.at(0).packageName, QStringLiteral("com.bilko.urtquest"));
        QCOMPARE(records.at(0).releaseTag, QStringLiteral("v1.0.2"));

        const auto onHeadset = parseHeadsetRecords(m_server->files.value(kPath));
        QCOMPARE(onHeadset->size(), 2); // The cache's extra game was written back.
        QCOMPARE(cache.find(kHardware, QStringLiteral("com.bilko.urtquest"))->releaseTag, QStringLiteral("v1.0.2"));
    }

    void anAddedRecordReplacesTheOldOne()
    {
        m_server->files.insert(kPath, headsetRecordsJson(kHardware, {record(QStringLiteral("com.bilko.urtquest"),
                                                                            QStringLiteral("v1.0.2"), 14)}));
        InstallRecords cache = InstallRecords::load(m_cachePath);
        // Even with an older timestamp (a PC clock behind the other's), the new install wins.
        QVERIFY(run(cache, record(QStringLiteral("com.bilko.urtquest"), QStringLiteral("v1.0.3"), 13)));
        QCOMPARE(parseHeadsetRecords(m_server->files.value(kPath))->at(0).releaseTag, QStringLiteral("v1.0.3"));
    }

    void leavesAnUnchangedListAlone()
    {
        // Formatted differently from what QGPUI writes: a rewrite would change the bytes.
        const QByteArray compact = headsetRecordsJson(kHardware, {record(QStringLiteral("com.a.b"), QStringLiteral("v1.0"), 1)})
                                       .simplified();
        m_server->files.insert(kPath, compact);
        InstallRecords cache = InstallRecords::load(m_cachePath);
        QVERIFY(run(cache, std::nullopt));
        QCOMPARE(m_server->files.value(kPath), compact);
        QVERIFY(cache.find(kHardware, QStringLiteral("com.a.b"))); // The cache learned it.
    }

    void removesARecordFromTheHeadsetAndTheCache()
    {
        // The headset lists urtquest; the cache lists it and quakequest.
        m_server->files.insert(kPath, headsetRecordsJson(kHardware, {record(QStringLiteral("com.bilko.urtquest"),
                                                                            QStringLiteral("v1.0.2"), 14)}));
        InstallRecords cache = InstallRecords::load(m_cachePath);
        cache.set(kHardware, record(QStringLiteral("com.bilko.urtquest"), QStringLiteral("v1.0.1"), 12));
        cache.set(kHardware, record(QStringLiteral("com.drbeef.quakequest"), QStringLiteral("v1.6.0"), 13));

        HeadsetRecordSync sync(m_client, kAdbSerial, kHardware, &cache);
        QSignalSpy finished(&sync, &HeadsetRecordSync::finished);
        sync.remove(QStringLiteral("com.bilko.urtquest"));
        QVERIFY(finished.wait(5000));
        QVERIFY2(finished.at(0).at(0).toBool(), qPrintable(finished.at(0).at(1).toString()));

        QCOMPARE(sync.records().size(), 1);
        const auto onHeadset = parseHeadsetRecords(m_server->files.value(kPath));
        QCOMPARE(onHeadset->size(), 1);
        QCOMPARE(onHeadset->at(0).packageName, QStringLiteral("com.drbeef.quakequest"));
        QVERIFY(!cache.find(kHardware, QStringLiteral("com.bilko.urtquest")));
        QVERIFY(!InstallRecords::load(m_cachePath).find(kHardware, QStringLiteral("com.bilko.urtquest")));
    }

    void removingAnUnknownPackageChangesNothing()
    {
        const QByteArray compact = headsetRecordsJson(kHardware, {record(QStringLiteral("com.a.b"), QStringLiteral("v1.0"), 1)})
                                       .simplified();
        m_server->files.insert(kPath, compact);
        InstallRecords cache = InstallRecords::load(m_cachePath);
        HeadsetRecordSync sync(m_client, kAdbSerial, kHardware, &cache);
        QSignalSpy finished(&sync, &HeadsetRecordSync::finished);
        sync.remove(QStringLiteral("com.not.recorded"));
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.at(0).at(0).toBool());
        QCOMPARE(m_server->files.value(kPath), compact);
    }

    void neverOverwritesAFileItCannotRead()
    {
        m_server->files.insert(kPath, "not json");
        InstallRecords cache = InstallRecords::load(m_cachePath);
        QString error;
        QVERIFY(!run(cache, record(QStringLiteral("com.a.b"), QStringLiteral("v1.0"), 1), &error));
        QVERIFY2(error.contains(QStringLiteral("not a QGPUI install list")), qPrintable(error));
        QCOMPARE(m_server->files.value(kPath), QByteArray("not json"));
    }

    void failsSoftlyWithoutAHeadset()
    {
        InstallRecords cache = InstallRecords::load(m_cachePath);
        cache.set(kHardware, record(QStringLiteral("com.a.b"), QStringLiteral("v1.0"), 1));
        QString error;
        QVERIFY(!run(cache, std::nullopt, &error, QStringLiteral("no-such-headset")));
        QVERIFY(!error.isEmpty());
        QVERIFY(cache.find(kHardware, QStringLiteral("com.a.b"))); // The cache keeps its last state.
    }
};

QTEST_GUILESS_MAIN(TestHeadsetRecords)
#include "tst_headsetrecords.moc"
