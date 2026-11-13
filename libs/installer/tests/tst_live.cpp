#include "installer/ApkFetcher.h"
#include "installer/HeadsetRecords.h"
#include "installer/Runner.h"
#include "installer/Updates.h"

#include <qadb/Client.h>
#include <qadb/Reply.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace installer;

// Installs one recipe on a real headset: downloads its APK, runs every step and answers each tell
// step at once. Only recipes without inputs can run this way. Skipped unless both are set:
//   QGPUI_INSTALL_LIVE_SERIAL  the headset's adb serial
//   QGPUI_INSTALL_LIVE_SLUG    the recipe, such as urbanterror-vr
// The APK is cached in the build folder, so a second run does not download it again.
class TestLive : public QObject
{
    Q_OBJECT

private:
    QString m_serial;
    QString m_slug;

private slots:
    void initTestCase()
    {
        m_serial = qEnvironmentVariable("QGPUI_INSTALL_LIVE_SERIAL");
        m_slug = qEnvironmentVariable("QGPUI_INSTALL_LIVE_SLUG");
        if (m_serial.isEmpty() || m_slug.isEmpty())
            QSKIP("Set QGPUI_INSTALL_LIVE_SERIAL and QGPUI_INSTALL_LIVE_SLUG to install a recipe on a headset.");
    }

    void installsTheRecipe()
    {
        QFile file(QDir(QStringLiteral(QGPUI_RECIPES_DIR)).filePath(m_slug + QStringLiteral(".json")));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        const QString package = json.value(QLatin1StringView("checkedAgainst")).toObject()
                                    .value(QLatin1StringView("packageName")).toString();
        QString error;
        const auto recipe = parseRecipe(json, package, &error);
        QVERIFY2(recipe, qPrintable(error));
        QVERIFY2(recipe->inputs.isEmpty(), "This test runs only recipes without inputs.");

        ApkFetcher fetcher(QDir::current().filePath(QStringLiteral("apk-cache")));
        QSignalSpy fetched(&fetcher, &ApkFetcher::finished);
        QString releaseTag;
        QString asset;
        connect(&fetcher, &ApkFetcher::resolved, this, [&](const QString& tag, const QString& name) {
            qInfo().noquote() << "release" << tag << "asset" << name;
            releaseTag = tag;
            asset = name;
        });
        fetcher.fetch(recipe->apks.first());
        QVERIFY(fetched.wait(15 * 60 * 1000));
        QVERIFY2(fetched.at(0).at(0).toBool(), qPrintable(fetched.at(0).at(2).toString()));
        const QString apk = fetched.at(0).at(1).toString();
        qInfo().noquote() << "APK" << apk << QFileInfo(apk).size() << "bytes";

        qadb::Client client;
        Runner runner(&client, m_serial, *recipe, {apk}, {});
        connect(&runner, &Runner::stepStarted, this, [](int index, int count, const QString& text) {
            qInfo().noquote() << QStringLiteral("step %1/%2: %3").arg(index + 1).arg(count).arg(text);
        });
        connect(&runner, &Runner::message, this, [](const QString& text) { qInfo().noquote() << "message:" << text; });
        connect(&runner, &Runner::waitingForUser, &runner, [&runner](const QString& text) {
            qInfo().noquote() << "tell:" << text;
            runner.continueAfterTell();
        });
        connect(&runner, &Runner::afterInstall, this, [](const QString& text) { qInfo().noquote() << "after install:" << text; });
        QSignalSpy finished(&runner, &Runner::finished);
        runner.start();
        QVERIFY(finished.wait(15 * 60 * 1000));
        QVERIFY2(finished.at(0).at(0).toBool(), qPrintable(finished.at(0).at(1).toString()));

        // The package is installed, and any app op the recipe grants is allowed.
        auto* packages = client.shell(m_serial, QStringList{QStringLiteral("pm"), QStringLiteral("path"), recipe->packageName});
        QVERIFY(packages->waitForFinished());
        qInfo().noquote() << "pm path:" << packages->outputText().trimmed();
        QVERIFY(packages->outputText().startsWith(QStringLiteral("package:")));
        for (const Step& step : recipe->steps) {
            if (step.kind != Step::Kind::Grant || step.appop.isEmpty())
                continue;
            auto* op = client.shell(m_serial, QStringList{QStringLiteral("appops"), QStringLiteral("get"),
                                                          recipe->packageName, step.appop});
            QVERIFY(op->waitForFinished());
            qInfo().noquote() << "appops:" << op->outputText().trimmed();
            QVERIFY(op->outputText().contains(QStringLiteral(": allow")));
        }

        // Record the install on the headset and in a PC cache in the build folder.
        auto* hardware = client.getProperty(m_serial, QStringLiteral("ro.serialno"));
        QVERIFY(hardware->waitForFinished());
        auto* info = client.packageInfo(m_serial, recipe->packageName);
        QVERIFY(info->waitForFinished());
        QVERIFY(info->isInstalled());
        InstallRecord record;
        record.packageName = recipe->packageName;
        record.slug = m_slug;
        record.releaseTag = releaseTag;
        record.asset = asset;
        record.versionCode = info->info().versionCode;
        record.versionName = info->info().versionName;
        record.installedAt = QDateTime::currentDateTimeUtc();
        InstallRecords cache = InstallRecords::load(QDir::current().filePath(QStringLiteral("install-records.json")));
        HeadsetRecordSync sync(&client, m_serial, hardware->outputText(), &cache);
        QSignalSpy synced(&sync, &HeadsetRecordSync::finished);
        sync.run(record);
        QVERIFY(synced.wait(30000));
        QVERIFY2(synced.at(0).at(0).toBool(), qPrintable(synced.at(0).at(1).toString()));
        qInfo().noquote() << "recorded on" << hardware->outputText() << "-" << sync.records().size() << "record(s) on the headset";

        // What the update check says now: the release just installed is up to date.
        const auto status = updateStatus(info->info(), cache.find(hardware->outputText(), recipe->packageName), releaseTag);
        QCOMPARE(status, UpdateStatus::UpToDate);
    }
};

QTEST_GUILESS_MAIN(TestLive)
#include "tst_live.moc"
