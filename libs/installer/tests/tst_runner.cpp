#include "installer/Runner.h"

#include "FakeAdbServer.h"

#include <qadb/Client.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace installer;

namespace {

const QString kSerial = QStringLiteral("Q1");
const QString kPackage = QStringLiteral("com.example.game");

// JSON with single quotes, in ordinary literals (moc misreads // and /* inside raw strings).
Recipe recipe(QByteArray text)
{
    QString error;
    auto r = parseRecipe(QJsonDocument::fromJson(text.replace('\'', '"')).object(), kPackage, &error);
    if (!r)
        qFatal("%s", qPrintable(error));
    return *r;
}

void write(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

class TestRunner : public QObject
{
    Q_OBJECT

private:
    FakeAdbServer* m_server = nullptr;
    qadb::Client* m_client = nullptr;
    QTemporaryDir m_tmp;
    QStringList m_commands;
    QString m_apk;

    Runner* makeRunner(const Recipe& r, const QHash<QString, InputSource>& inputs)
    {
        auto* runner = new Runner(m_client, kSerial, r, {m_apk}, inputs, this);
        runner->setLaunchWaitOverride(std::chrono::milliseconds(0));
        return runner;
    }

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_server = new FakeAdbServer(this);
        QVERIFY(m_server->listen());
        m_server->setDevice(kSerial, {});
        m_commands.clear();
        m_server->setShellHandler([this](const QString&, const QString& command) {
            m_commands.append(command);
            FakeAdbServer::ShellResult result;
            if (command.startsWith(QStringLiteral("false")))
                result.exitCode = 1, result.err = "it failed";
            else if (command.startsWith(QStringLiteral("stat -f")))
                result.out = "4096 1000 512\n"; // 2 MB free.
            return result;
        });
        m_client = new qadb::Client(qadb::ServerAddress{QStringLiteral("127.0.0.1"), m_server->port()}, this);
        m_apk = m_tmp.filePath(QStringLiteral("game.apk"));
        write(m_apk, "PK-apk");
    }

    void cleanup()
    {
        delete m_client;
        delete m_server;
        m_client = nullptr;
        m_server = nullptr;
    }

    void runsEveryStepInOrder()
    {
        const QString disc = m_tmp.filePath(QStringLiteral("pick/Game (USA).iso"));
        write(disc, "DISC");
        const QString game = m_tmp.filePath(QStringLiteral("pc/Game"));
        write(game + QStringLiteral("/id1/pak0.pak"), "P0");
        write(game + QStringLiteral("/id1/pak1.pak"), "P1");

        const Recipe r = recipe(
            "{'apks':[{'source':'url','url':'https:\\/\\/x.test\\/g.apk'}],"
            " 'inputs':[{'id':'disc','kind':'file','label':'Disc','extensions':['.iso']},"
            "           {'id':'pc','kind':'pc-install','label':'PC game','root':'id1','detect':['pak0.pak']}],"
            " 'steps':[{'do':'install-apk','grantAll':true},"
            "          {'do':'grant','appop':'MANAGE_EXTERNAL_STORAGE'},"
            "          {'do':'launch','why':'creates its folder'},"
            "          {'do':'push','input':'disc','map':[{'to':'\\/sdcard\\/Android\\/data\\/com.example.game\\/files\\/data\\/disc.iso'}]},"
            "          {'do':'push','input':'pc','map':[{'from':'*.pak','to':'\\/sdcard\\/Game\\/id1\\/'}]},"
            "          {'do':'verify','files':['\\/sdcard\\/Game\\/id1\\/pak1.pak']},"
            "          {'do':'shell','args':['am','force-stop','com.example.game'],'why':'w'},"
            "          {'do':'tell','text':'Open the game.'}]}");
        Runner* runner = makeRunner(r, {{QStringLiteral("disc"), {{}, {disc}}},
                                        {QStringLiteral("pc"), {game + QStringLiteral("/id1"), {}}}});
        QSignalSpy steps(runner, &Runner::stepStarted);
        QSignalSpy waiting(runner, &Runner::waitingForUser);
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();

        QTRY_COMPARE_WITH_TIMEOUT(waiting.size(), 1, 10000);
        QCOMPARE(waiting.at(0).at(0).toString(), QStringLiteral("Open the game."));
        QCOMPARE(finished.size(), 0);
        runner->continueAfterTell();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY2(finished.at(0).at(0).toBool(), qPrintable(finished.at(0).at(1).toString()));
        QCOMPARE(steps.size(), 8);
        QCOMPARE(steps.at(3).at(2).toString(),
                 QStringLiteral("Copy disc to /sdcard/Android/data/com.example.game/files/data/disc.iso"));

        QCOMPARE(m_server->installedApks, QList<QByteArray>{"PK-apk"});
        QCOMPARE(m_server->files.value(QStringLiteral("/sdcard/Android/data/com.example.game/files/data/disc.iso")),
                 QByteArray("DISC"));
        QCOMPARE(m_server->files.value(QStringLiteral("/sdcard/Game/id1/pak0.pak")), QByteArray("P0"));
        QCOMPARE(m_server->files.value(QStringLiteral("/sdcard/Game/id1/pak1.pak")), QByteArray("P1"));

        const QString all = m_commands.join(u'\n');
        QVERIFY2(all.contains(QStringLiteral("appops set com.example.game MANAGE_EXTERNAL_STORAGE allow")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("monkey -p com.example.game")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("am force-stop com.example.game")), qPrintable(all));
        // The app-private folders are created and opened before the push; /sdcard/Game is not.
        QVERIFY2(all.contains(QStringLiteral("for d in /sdcard/Android/data/com.example.game/files "
                                             "/sdcard/Android/data/com.example.game/files/data;")),
                 qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("chmod 2777")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("/sdcard/Game;")), qPrintable(all));
    }

    void failsWhenAVerifiedFileIsMissing()
    {
        const Recipe r = recipe("{'steps':[{'do':'verify','files':['\\/sdcard\\/nothing.dat']}]}");
        Runner* runner = makeRunner(r, {});
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!finished.at(0).at(0).toBool());
        QVERIFY(finished.at(0).at(1).toString().contains(QStringLiteral("missing")));
    }

    void failsOnANonZeroShellExit()
    {
        const Recipe r = recipe("{'steps':[{'do':'shell','args':['false'],'why':'w'},{'do':'tell','text':'never'}]}");
        Runner* runner = makeRunner(r, {});
        QSignalSpy finished(runner, &Runner::finished);
        QSignalSpy waiting(runner, &Runner::waitingForUser);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!finished.at(0).at(0).toBool());
        QVERIFY2(finished.at(0).at(1).toString().contains(QStringLiteral("exit code 1")), qPrintable(finished.at(0).at(1).toString()));
        QCOMPARE(waiting.size(), 0);
    }

    void aFailedGrantIsOnlyAMessage()
    {
        m_server->setShellHandler([](const QString&, const QString&) { return FakeAdbServer::ShellResult{{}, "denied", 1}; });
        const Recipe r = recipe("{'steps':[{'do':'grant','appop':'MANAGE_EXTERNAL_STORAGE'}]}");
        Runner* runner = makeRunner(r, {});
        QSignalSpy messages(runner, &Runner::message);
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(finished.at(0).at(0).toBool());
        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.at(0).at(0).toString().contains(QStringLiteral("denied")));
    }

    void skipsMissingOptionalInputsAndFailsOnRequiredOnes()
    {
        const QByteArray text = "{'inputs':[{'id':'extra','kind':'file','label':'Extra','names':['x.zip'],'required':false},"
                                "           {'id':'main','kind':'file','label':'Main','names':['m.zip']}],"
                                " 'steps':[{'do':'push','input':'extra','map':[{'to':'\\/sdcard\\/Download\\/'}]},"
                                "          {'do':'push','input':'main','map':[{'to':'\\/sdcard\\/Download\\/'}]}]}";
        Runner* runner = makeRunner(recipe(text), {});
        QSignalSpy messages(runner, &Runner::message);
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!finished.at(0).at(0).toBool());
        QVERIFY(finished.at(0).at(1).toString().contains(QStringLiteral("Main")));
        QVERIFY(messages.at(0).at(0).toString().contains(QStringLiteral("Skipped: Extra")));
    }

    void keepsANewerBuildOnTheHeadset()
    {
        write(m_apk, "FAIL:INSTALL_FAILED_VERSION_DOWNGRADE");
        Runner* runner = makeRunner(recipe("{'steps':[{'do':'install-apk'},{'do':'shell','args':['true'],'why':'w'}]}"), {});
        QSignalSpy messages(runner, &Runner::message);
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY2(finished.at(0).at(0).toBool(), qPrintable(finished.at(0).at(1).toString()));
        QVERIFY(messages.last().at(0).toString().contains(QStringLiteral("newer version")));
        QVERIFY(m_commands.contains(QStringLiteral("true"))); // The run went on.
    }

    void explainsAChangedSigningKey()
    {
        write(m_apk, "FAIL:INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package signatures do not match");
        Runner* runner = makeRunner(recipe("{'steps':[{'do':'install-apk'}]}"), {});
        QSignalSpy finished(runner, &Runner::finished);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QVERIFY(!finished.at(0).at(0).toBool());
        QVERIFY2(finished.at(0).at(1).toString().contains(QStringLiteral("different key")),
                 qPrintable(finished.at(0).at(1).toString()));
    }

    void checksFreeSpaceBeforeStarting()
    {
        // The fake headset has 2 MB free.
        Runner* tooBig = makeRunner(recipe("{'requirements':{'freeSpaceMB':3},'steps':[{'do':'install-apk'}]}"), {});
        QSignalSpy failed(tooBig, &Runner::finished);
        tooBig->start();
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
        QVERIFY(!failed.at(0).at(0).toBool());
        QVERIFY2(failed.at(0).at(1).toString().contains(QStringLiteral("2 MB free")), qPrintable(failed.at(0).at(1).toString()));
        QVERIFY(m_server->installedApks.isEmpty()); // Nothing was changed.

        Runner* fits = makeRunner(recipe("{'requirements':{'freeSpaceMB':1},'steps':[{'do':'install-apk'}]}"), {});
        QSignalSpy ok(fits, &Runner::finished);
        fits->start();
        QTRY_COMPARE_WITH_TIMEOUT(ok.size(), 1, 5000);
        QVERIFY2(ok.at(0).at(0).toBool(), qPrintable(ok.at(0).at(1).toString()));
    }

    void cancelsWhileWaitingForTheUser()
    {
        Runner* runner = makeRunner(recipe("{'steps':[{'do':'tell','text':'t'},{'do':'shell','args':['true'],'why':'w'}]}"), {});
        QSignalSpy finished(runner, &Runner::finished);
        QSignalSpy waiting(runner, &Runner::waitingForUser);
        runner->start();
        QTRY_COMPARE_WITH_TIMEOUT(waiting.size(), 1, 5000);
        runner->cancel();
        QCOMPARE(finished.size(), 1);
        QVERIFY(!finished.at(0).at(0).toBool());
        runner->continueAfterTell(); // Ignored after cancel.
        QTest::qWait(50);
        QVERIFY(m_commands.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRunner)
#include "tst_runner.moc"
