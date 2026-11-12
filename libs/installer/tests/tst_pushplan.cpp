#include "installer/PushPlan.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace installer;

namespace {

void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
}

Step push(const QList<PushMapping>& map)
{
    Step step;
    step.kind = Step::Kind::Push;
    step.input = QStringLiteral("in");
    step.map = map;
    return step;
}

QStringList remotes(const PushPlan& plan)
{
    QStringList list;
    for (const PushFile& f : plan.files)
        list.append(f.remotePath);
    list.sort();
    return list;
}

} // namespace

class TestPushPlan : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString m_game; // A pc-install folder.

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_game = m_tmp.filePath(QStringLiteral("game"));
        touch(m_game + QStringLiteral("/pak0.pak"));
        touch(m_game + QStringLiteral("/PAK1.PAK"));
        touch(m_game + QStringLiteral("/misc/a.txt"));
        touch(m_game + QStringLiteral("/misc/deep/b.txt"));
        touch(m_game + QStringLiteral("/data/x.dat"));
    }

    void globsKeepThePathBelowTheirFixedPrefix()
    {
        Input input;
        input.kind = Input::Kind::PcInstall;
        PushPlan plan;
        QVERIFY(planPush(push({{QStringLiteral("*.pak"), QStringLiteral("/sdcard/Q/id1/")},
                               {QStringLiteral("misc/**"), QStringLiteral("/sdcard/Q/misc/")}}),
                         input, {m_game, {}}, plan));
        QCOMPARE(remotes(plan), (QStringList{QStringLiteral("/sdcard/Q/id1/PAK1.PAK"), QStringLiteral("/sdcard/Q/id1/pak0.pak"),
                                             QStringLiteral("/sdcard/Q/misc/a.txt"), QStringLiteral("/sdcard/Q/misc/deep/b.txt")}));
        QVERIFY(plan.unmatched.isEmpty());
    }

    void withoutFromEveryFileIsCopied()
    {
        Input input;
        input.kind = Input::Kind::PcInstall;
        PushPlan plan;
        QVERIFY(planPush(push({{{}, QStringLiteral("/sdcard/G/")}}), input, {m_game, {}}, plan));
        QCOMPARE(plan.files.size(), 5);
        QVERIFY(remotes(plan).contains(QStringLiteral("/sdcard/G/misc/deep/b.txt")));
    }

    void aFixedPathKeepsTheRecipesSpelling()
    {
        Input input;
        input.kind = Input::Kind::PcInstall;
        PushPlan plan;
        QVERIFY(planPush(push({{QStringLiteral("pak1.pak"), QStringLiteral("/sdcard/Q/")},
                               {QStringLiteral("data/x.dat"), QStringLiteral("/sdcard/Q/renamed.dat")},
                               {QStringLiteral("nope.pak"), QStringLiteral("/sdcard/Q/")}}),
                         input, {m_game, {}}, plan));
        QCOMPARE(remotes(plan), (QStringList{QStringLiteral("/sdcard/Q/pak1.pak"), QStringLiteral("/sdcard/Q/renamed.dat")}));
        QCOMPARE(plan.unmatched, QStringList{QStringLiteral("nope.pak -> /sdcard/Q/")});
    }

    void fileInputsMatchByName()
    {
        Input input;
        input.kind = Input::Kind::File;
        input.names = {QStringLiteral("vcop.zip"), QStringLiteral("model1io2.zip")};
        const QStringList picked{m_tmp.filePath(QStringLiteral("dl/VCOP.ZIP")), m_tmp.filePath(QStringLiteral("dl/model1io2.zip"))};
        PushPlan plan;
        QVERIFY(planPush(push({{QStringLiteral("vcop.zip"), QStringLiteral("/sdcard/Download/")},
                               {{}, QStringLiteral("/sdcard/All/")}}),
                         input, {{}, picked}, plan));
        QCOMPARE(remotes(plan), (QStringList{QStringLiteral("/sdcard/All/VCOP.ZIP"), QStringLiteral("/sdcard/All/model1io2.zip"),
                                             QStringLiteral("/sdcard/Download/vcop.zip")}));
    }

    void aSingleFileTargetTakesOneFile()
    {
        Input input;
        input.kind = Input::Kind::File;
        PushPlan plan;
        QVERIFY(planPush(push({{{}, QStringLiteral("/sdcard/CTR/assets/ctr-u.bin")}}), input,
                         {{}, {m_tmp.filePath(QStringLiteral("CTR (USA).bin"))}}, plan));
        QCOMPARE(remotes(plan), QStringList{QStringLiteral("/sdcard/CTR/assets/ctr-u.bin")});

        PushPlan two;
        QString error;
        QVERIFY(!planPush(push({{{}, QStringLiteral("/sdcard/x.bin")}}), input,
                          {{}, {QStringLiteral("C:/a.bin"), QStringLiteral("C:/b.bin")}}, two, &error));
        QVERIFY2(error.contains(QStringLiteral("names one file")), qPrintable(error));
    }

    void findsAppPrivateFoldersParentsFirst()
    {
        QVERIFY(isAppPrivatePath(QStringLiteral("/sdcard/Android/data/com.a/files/x")));
        QVERIFY(isAppPrivatePath(QStringLiteral("/sdcard/Android/obb/com.a/x.zip")));
        QVERIFY(!isAppPrivatePath(QStringLiteral("/sdcard/Download/x")));
        QVERIFY(!isAppPrivatePath(QStringLiteral("/sdcard/Android/data/")));

        const QList<PushFile> files{
            {{}, QStringLiteral("/sdcard/Android/data/com.a/files/game/sys/a.bin")},
            {{}, QStringLiteral("/sdcard/Android/data/com.a/files/game/files/b.bin")},
            {{}, QStringLiteral("/sdcard/Android/data/com.a/files/top.bin")},
            {{}, QStringLiteral("/sdcard/Android/obb/com.a/import.zip")}, // Directly in <package>/: no folder.
            {{}, QStringLiteral("/sdcard/Download/x.bin")},
        };
        QCOMPARE(appPrivateFolders(files),
                 (QStringList{QStringLiteral("/sdcard/Android/data/com.a/files"),
                              QStringLiteral("/sdcard/Android/data/com.a/files/game"),
                              QStringLiteral("/sdcard/Android/data/com.a/files/game/files"),
                              QStringLiteral("/sdcard/Android/data/com.a/files/game/sys")}));
    }
};

QTEST_GUILESS_MAIN(TestPushPlan)
#include "tst_pushplan.moc"
