#include "installer/ApkFetcher.h"
#include "installer/Runner.h"

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
//   QGP_INSTALL_LIVE_SERIAL  the headset's adb serial
//   QGP_INSTALL_LIVE_SLUG    the recipe, such as urbanterror-vr
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
        m_serial = qEnvironmentVariable("QGP_INSTALL_LIVE_SERIAL");
        m_slug = qEnvironmentVariable("QGP_INSTALL_LIVE_SLUG");
        if (m_serial.isEmpty() || m_slug.isEmpty())
            QSKIP("Set QGP_INSTALL_LIVE_SERIAL and QGP_INSTALL_LIVE_SLUG to install a recipe on a headset.");
    }

    void installsTheRecipe()
    {
        QFile file(QDir(QStringLiteral(QGP_RECIPES_DIR)).filePath(m_slug + QStringLiteral(".json")));
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
        connect(&fetcher, &ApkFetcher::resolved, this, [](const QString& tag, const QString& name) {
            qInfo().noquote() << "release" << tag << "asset" << name;
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
    }
};

QTEST_GUILESS_MAIN(TestLive)
#include "tst_live.moc"
