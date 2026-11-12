#include "installer/InstallPlan.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace installer;

class TestInstallPlan : public QObject
{
    Q_OBJECT

private:
    static Recipe recipe()
    {
        Recipe r;
        r.packageName = QStringLiteral("com.example.game");
        r.freeSpaceMB = 500;
        ApkSource apk;
        apk.repo = QStringLiteral("owner/game");
        apk.asset = QStringLiteral("\\.apk$");
        r.apks = {apk};
        Input input;
        input.kind = Input::Kind::PcInstall;
        input.id = QStringLiteral("data");
        input.label = QStringLiteral("Game data");
        r.inputs = {input};

        Step install;
        install.kind = Step::Kind::InstallApk;
        install.grantAll = true;
        Step launch;
        launch.kind = Step::Kind::Launch;
        launch.waitSeconds = 5;
        Step push;
        push.kind = Step::Kind::Push;
        push.input = QStringLiteral("data");
        push.map = {{QStringLiteral("DATA/**"), QStringLiteral("/sdcard/Android/data/com.example.game/files/DATA/")}};
        Step tell;
        tell.kind = Step::Kind::Tell;
        tell.text = QStringLiteral("Put the headset on.");
        r.steps = {install, launch, push, tell};
        return r;
    }

private slots:
    void listsTheCommandsOfEachStep()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(QDir().mkpath(tmp.filePath(QStringLiteral("DATA/sub"))));
        QFile file(tmp.filePath(QStringLiteral("DATA/sub/a.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        const QString apk = tmp.filePath(QStringLiteral("game.apk"));
        const InstallPlan plan = planInstall(recipe(), {apk}, {{QStringLiteral("data"), {tmp.path(), {}}}});
        QCOMPARE(plan.steps.size(), 6); // Get the APK, free space, then the four steps.
        QVERIFY(plan.steps.at(0).lines.join(u'\n').contains(QStringLiteral("https://github.com/owner/game")));
        QVERIFY(plan.steps.at(2).lines.contains(
            QStringLiteral("adb install -r -g \"%1\"").arg(QDir::toNativeSeparators(apk))));
        QVERIFY(plan.steps.at(3).lines.contains(QStringLiteral("adb shell am force-stop com.example.game")));

        const QStringList push = plan.steps.at(4).lines;
        // The missing app-private folders, parents first, then the copy.
        const QString sub = QStringLiteral("/sdcard/Android/data/com.example.game/files/DATA/sub");
        const qsizetype subLine = push.indexOf(
            QStringLiteral("adb shell \"[ -d %1 ] || { mkdir %1 && chmod 2777 %1; }\"").arg(sub));
        QVERIFY2(subLine > 0, qPrintable(push.join(u'\n')));
        QVERIFY(push.at(subLine - 1).endsWith(QStringLiteral("files/DATA; }\"")));
        QVERIFY(push.last().startsWith(QStringLiteral("adb push \"")));
        QVERIFY2(push.last().endsWith(
                     QStringLiteral(" /sdcard/Android/data/com.example.game/files/DATA/sub/a.txt  (mode 0666)")),
                 qPrintable(push.last()));

        QVERIFY(plan.steps.at(5).lines.contains(QStringLiteral("Put the headset on.")));
        QVERIFY(plan.toText().startsWith(QStringLiteral("1. Get the APK\n    Download")));
    }

    void withoutTheFilesSaysWhatIsMissing()
    {
        const InstallPlan plan = planInstall(recipe(), {}, {});
        QVERIFY(plan.steps.at(2).lines.contains(QStringLiteral("adb install -r -g <APK 1>")));
        QVERIFY(plan.steps.at(4).lines.first().contains(QStringLiteral("Game data has not been found yet")));
        QVERIFY(plan.steps.at(4).lines.contains(
            QStringLiteral("Copies DATA/** to /sdcard/Android/data/com.example.game/files/DATA/")));
    }
};

QTEST_GUILESS_MAIN(TestInstallPlan)
#include "tst_installplan.moc"
