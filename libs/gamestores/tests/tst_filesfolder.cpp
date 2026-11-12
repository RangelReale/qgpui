#include "gamestores/FilesFolder.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

using namespace gamestores;

namespace {

void touch(const QString& path)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

// JSON written with single quotes, in ordinary literals: moc misreads // and /* inside raw strings.
QJsonArray inputs(QByteArray json)
{
    return QJsonDocument::fromJson(json.replace('\'', '"')).array();
}

const QByteArray naomiInputs = "[{'id':'roms','kind':'file','names':['mok.zip','naomi.zip']}]";
const QByteArray discInputs = "[{'id':'disc','kind':'file','extensions':['.iso','.rvz']}]";
const QByteArray cueInputs = "[{'id':'disc','kind':'file','extensions':['.cue','.bin'],'multiple':true}]";
const QByteArray languageInputs = "[{'id':'english','kind':'file','extensions':['.cue','.bin'],'multiple':true},"
                                  "{'id':'spanish','kind':'file','extensions':['.cue','.bin'],'multiple':true}]";
const QByteArray quakeInputs = "[{'id':'quake','kind':'pc-install','root':'id1','detect':['pak0.pak','pak1.pak']}]";

} // namespace

class TestFilesFolder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;

    QString root() const { return m_tmp.path(); }
    QString path(const QString& relative) const { return QDir::cleanPath(m_tmp.filePath(relative)); }

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        QDir(root()).removeRecursively();
        QDir().mkpath(root());
    }

    void folders()
    {
        const FilesFolder folder(root() + QStringLiteral("/"));
        QCOMPARE(folder.gameFolder(QStringLiteral("mok-vr")), path(QStringLiteral("mok-vr")));
        QCOMPARE(folder.inputFolder(QStringLiteral("mok-vr"), QStringLiteral("roms")), path(QStringLiteral("mok-vr/roms")));
        QVERIFY(FilesFolder::defaultRoot().endsWith(QStringLiteral("/qgp/files")));
    }

    void nothingThere()
    {
        const FilesFolder folder(root());
        QCOMPARE(folder.find(QStringLiteral("mok-vr"), inputs(naomiInputs), QStringLiteral("roms")).status,
                 FilesMatch::Status::NotFound);
        QCOMPARE(folder.find(QStringLiteral("quakequest"), inputs(quakeInputs), QStringLiteral("quake")).status,
                 FilesMatch::Status::NotFound);
        QCOMPARE(folder.find(QStringLiteral("mok-vr"), inputs(naomiInputs), QStringLiteral("nosuch")).status,
                 FilesMatch::Status::NotFound);
    }

    // Names are found in the game folder first, and a shared file anywhere else.
    void namesInGameFolderAndShared()
    {
        touch(path(QStringLiteral("mok-vr/MOK.zip")));
        touch(path(QStringLiteral("other-game/naomi.zip")));
        touch(path(QStringLiteral("shared/naomi.zip")));
        const FilesFolder folder(root());
        const FilesMatch match = folder.find(QStringLiteral("mok-vr"), inputs(naomiInputs), QStringLiteral("roms"));
        QCOMPARE(match.status, FilesMatch::Status::Found);
        QCOMPARE(match.paths, QStringList({path(QStringLiteral("mok-vr/MOK.zip")),
                                           path(QStringLiteral("other-game/naomi.zip"))}));
    }

    void namesPreferGameFolder()
    {
        touch(path(QStringLiteral("a-shared/naomi.zip")));
        touch(path(QStringLiteral("mok-vr/sub/naomi.zip")));
        touch(path(QStringLiteral("mok-vr/mok.zip")));
        const FilesMatch match =
            FilesFolder(root()).find(QStringLiteral("mok-vr"), inputs(naomiInputs), QStringLiteral("roms"));
        QCOMPARE(match.status, FilesMatch::Status::Found);
        QCOMPARE(match.paths.at(1), path(QStringLiteral("mok-vr/sub/naomi.zip")));
    }

    void namesPartial()
    {
        touch(path(QStringLiteral("shared/naomi.zip")));
        const FilesMatch match =
            FilesFolder(root()).find(QStringLiteral("mok-vr"), inputs(naomiInputs), QStringLiteral("roms"));
        QCOMPARE(match.status, FilesMatch::Status::Partial);
        QCOMPARE(match.paths, QStringList{path(QStringLiteral("shared/naomi.zip"))});
        QCOMPARE(match.missing, QStringList{QStringLiteral("mok.zip")});
    }

    // Extensions are matched only in the game's own folders, never in the rest of the files folder.
    void extensions()
    {
        touch(path(QStringLiteral("other-game/other.iso")));
        const FilesFolder folder(root());
        QCOMPARE(folder.find(QStringLiteral("xelee"), inputs(discInputs), QStringLiteral("disc")).status,
                 FilesMatch::Status::NotFound);

        touch(path(QStringLiteral("xelee/Melee.ISO")));
        FilesMatch match = folder.find(QStringLiteral("xelee"), inputs(discInputs), QStringLiteral("disc"));
        QCOMPARE(match.status, FilesMatch::Status::Found);
        QCOMPARE(match.paths, QStringList{path(QStringLiteral("xelee/Melee.ISO"))});

        touch(path(QStringLiteral("xelee/old/melee.rvz")));
        match = folder.find(QStringLiteral("xelee"), inputs(discInputs), QStringLiteral("disc"));
        QCOMPARE(match.status, FilesMatch::Status::Ambiguous);
        QCOMPARE(match.paths.size(), 2);
    }

    void extensionsMultiple()
    {
        touch(path(QStringLiteral("game/disc.cue")));
        touch(path(QStringLiteral("game/disc (Track 1).bin")));
        touch(path(QStringLiteral("game/readme.txt")));
        const FilesMatch match = FilesFolder(root()).find(QStringLiteral("game"), inputs(cueInputs), QStringLiteral("disc"));
        QCOMPARE(match.status, FilesMatch::Status::Found);
        QCOMPARE(match.paths.size(), 2);
    }

    // Two inputs that accept the same extensions need their own folders.
    void overlappingInputs()
    {
        touch(path(QStringLiteral("mgs/english.cue")));
        const FilesFolder folder(root());
        QCOMPARE(folder.find(QStringLiteral("mgs"), inputs(languageInputs), QStringLiteral("english")).status,
                 FilesMatch::Status::Ambiguous);

        QDir(path(QStringLiteral("mgs"))).removeRecursively();
        touch(path(QStringLiteral("mgs/english/disc.cue")));
        touch(path(QStringLiteral("mgs/english/disc.bin")));
        touch(path(QStringLiteral("mgs/spanish/disco.cue")));
        FilesMatch match = folder.find(QStringLiteral("mgs"), inputs(languageInputs), QStringLiteral("english"));
        QCOMPARE(match.status, FilesMatch::Status::Found);
        QCOMPARE(match.paths, QStringList({path(QStringLiteral("mgs/english/disc.bin")),
                                           path(QStringLiteral("mgs/english/disc.cue"))}));
        match = folder.find(QStringLiteral("mgs"), inputs(languageInputs), QStringLiteral("spanish"));
        QCOMPARE(match.paths, QStringList{path(QStringLiteral("mgs/spanish/disco.cue"))});
    }

    void create()
    {
        const FilesFolder folder(path(QStringLiteral("files")));
        QCOMPARE(FilesFolder::inputsNeedingFolders(inputs(languageInputs)),
                 QStringList({QStringLiteral("english"), QStringLiteral("spanish")}));
        QVERIFY(FilesFolder::inputsNeedingFolders(inputs(naomiInputs)).isEmpty());
        QVERIFY(FilesFolder::inputsNeedingFolders(inputs(quakeInputs)).isEmpty());

        const QString mgs = QStringLiteral("Metal Gear Solid VR");
        QVERIFY(folder.create(QStringLiteral("mgs"), mgs, inputs(languageInputs)));
        QVERIFY(QFileInfo(path(QStringLiteral("files/mgs/english"))).isDir());
        QVERIFY(QFileInfo(path(QStringLiteral("files/mgs/spanish"))).isDir());
        QVERIFY(folder.create(QStringLiteral("mgs"), mgs, inputs(languageInputs))); // Already there.

        // Every game with inputs gets the guide, even when its files are already there.
        touch(path(QStringLiteral("files/quakequest/id1/pak0.pak")));
        touch(path(QStringLiteral("files/quakequest/id1/pak1.pak")));
        QVERIFY(folder.create(QStringLiteral("quakequest"), QStringLiteral("QuakeQuest"), inputs(quakeInputs)));
        QCOMPARE(QDir(path(QStringLiteral("files/quakequest"))).entryList(QDir::Files),
                 QStringList{FilesFolder::kGuideFileName});

        // A game without inputs gets an empty folder.
        QVERIFY(folder.create(QStringLiteral("apk-only"), QStringLiteral("APK only"), QJsonArray()));
        QVERIFY(QDir(path(QStringLiteral("files/apk-only"))).isEmpty());
    }

    void refusesNamesThatLeaveTheFolder()
    {
        // Slugs and input ids come from the catalog.
        const FilesFolder folder(path(QStringLiteral("files")));
        QString error;
        QVERIFY(!folder.create(QStringLiteral("../outside"), QStringLiteral("T"), QJsonArray(), &error));
        QVERIFY2(error.contains(QStringLiteral("folder name")), qPrintable(error));
        QVERIFY(!folder.create(QStringLiteral("a/b"), QStringLiteral("T"), QJsonArray()));
        QVERIFY(!QFileInfo::exists(path(QStringLiteral("outside"))));

        // Two inputs with the same extensions get folders of their own, named by their ids.
        const QJsonArray climbing = inputs("[{'id':'..','kind':'file','label':'A','extensions':['.cue']},"
                                           "{'id':'b','kind':'file','label':'B','extensions':['.cue']}]");
        QVERIFY(!folder.create(QStringLiteral("game"), QStringLiteral("T"), climbing, &error));
        QVERIFY2(error.contains(QStringLiteral("input id")), qPrintable(error));

        touch(path(QStringLiteral("x.cue")));
        QVERIFY(folder.find(QStringLiteral(".."), inputs(languageInputs), QStringLiteral("english")).paths.isEmpty());
    }

    void guide()
    {
        const FilesFolder folder(root());
        const QString text = folder.guideText(
            QStringLiteral("Some Game"),
            inputs("[{'id':'english','kind':'file','label':'English disc','help':'Your own disc.',"
                   "'extensions':['.cue','.bin'],'multiple':true},"
                   "{'id':'spanish','kind':'file','label':'Spanish disc','extensions':['.cue','.bin'],'required':false},"
                   "{'id':'roms','kind':'file','label':'ROM sets','names':['a.zip','naomi.zip'],'keepArchived':true},"
                   "{'id':'o2r','kind':'file','label':'Archive','names':['x.o2r'],"
                   "'preparedWith':{'tool':'Tool','url':'https:\\/\\/example.com'}},"
                   "{'id':'game','kind':'pc-install','label':'The game','root':'id1','detect':['pak0.pak','pak1.pak']}]"));
        QVERIFY2(text.startsWith(QStringLiteral("Some Game\n=========\n")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("1. English disc\n    Your own disc.\n"
                                              "    Put in the folder \"english\": the files ending in .cue or .bin.\n")),
                 qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("2. Spanish disc (optional)\n"
                                              "    Put in the folder \"spanish\": one file ending in .cue or .bin.\n")),
                 qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("    Put here: a.zip and naomi.zip.\n    Keep zip files zipped.\n")),
                 qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("    Made with Tool: https://example.com\n")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("    Put here: the game's folder, or the folder \"id1\" inside it. "
                                              "qgp recognises it by pak0.pak and pak1.pak.\n")),
                 qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("anywhere in ")), qPrintable(text));
    }

    // The game dialog shows the same lines, naming the game's folder.
    void inputGuideWithSlug()
    {
        const FilesFolder folder(root());
        const QJsonArray list = inputs("[{'id':'english','kind':'file','label':'English disc','help':'Your own disc.',"
                                       "'extensions':['.cue','.bin'],'multiple':true},"
                                       "{'id':'spanish','kind':'file','label':'Spanish disc','extensions':['.cue','.bin']},"
                                       "{'id':'roms','kind':'file','label':'ROM sets','names':['a.zip']}]");
        const QString sep = QDir::toNativeSeparators(QStringLiteral("/"));
        QCOMPARE(folder.inputGuide(list, 0, QStringLiteral("mgs")),
                 (QStringList{QStringLiteral("Your own disc."),
                              QStringLiteral("Put in the folder \"mgs%1english\": the files ending in .cue or .bin.").arg(sep)}));
        QCOMPARE(folder.inputGuide(list, 2, QStringLiteral("mgs")),
                 QStringList{QStringLiteral("Put in the folder \"mgs\": a.zip.")});
        QVERIFY(folder.inputGuide(list, 3).isEmpty());
        QVERIFY(!folder.sharedFilesNote(list).isEmpty());
        QVERIFY(folder.sharedFilesNote(QJsonArray{list.at(0)}).isEmpty());
    }

    // The guide is never one of an input's files.
    void guideIsNotAFile()
    {
        touch(path(QStringLiteral("game/") + FilesFolder::kGuideFileName));
        const QByteArray txtInputs = "[{'id':'any','kind':'file','extensions':['.txt']}]";
        QCOMPARE(FilesFolder(root()).find(QStringLiteral("game"), inputs(txtInputs), QStringLiteral("any")).status,
                 FilesMatch::Status::NotFound);
    }

    // An input folder is used alone: files beside it in the game folder do not count.
    void inputFolderAlone()
    {
        touch(path(QStringLiteral("xelee/melee.iso")));
        QDir().mkpath(path(QStringLiteral("xelee/disc")));
        QCOMPARE(FilesFolder(root()).find(QStringLiteral("xelee"), inputs(discInputs), QStringLiteral("disc")).status,
                 FilesMatch::Status::NotFound);
    }

    void pcInstall_data()
    {
        QTest::addColumn<QStringList>("files");
        QTest::addColumn<QString>("expected");
        QTest::newRow("contents of root") << QStringList{QStringLiteral("quakequest/pak0.pak"), QStringLiteral("quakequest/pak1.pak")}
                                          << QStringLiteral("quakequest");
        QTest::newRow("root folder") << QStringList{QStringLiteral("quakequest/id1/pak0.pak"), QStringLiteral("quakequest/id1/PAK1.PAK")}
                                     << QStringLiteral("quakequest/id1");
        QTest::newRow("whole install") << QStringList{QStringLiteral("quakequest/Quake/id1/pak0.pak"),
                                                      QStringLiteral("quakequest/Quake/id1/pak1.pak")}
                                       << QStringLiteral("quakequest/Quake/id1");
        QTest::newRow("input folder") << QStringList{QStringLiteral("quakequest/quake/id1/pak0.pak"),
                                                     QStringLiteral("quakequest/quake/id1/pak1.pak")}
                                      << QStringLiteral("quakequest/quake/id1");
        QTest::newRow("incomplete") << QStringList{QStringLiteral("quakequest/id1/pak0.pak")} << QString();
    }

    void pcInstall()
    {
        QFETCH(QStringList, files);
        QFETCH(QString, expected);
        for (const QString& file : files)
            touch(path(file));
        const FilesMatch match =
            FilesFolder(root()).find(QStringLiteral("quakequest"), inputs(quakeInputs), QStringLiteral("quake"));
        if (expected.isEmpty()) {
            QCOMPARE(match.status, FilesMatch::Status::NotFound);
        } else {
            QCOMPARE(match.status, FilesMatch::Status::Found);
            QCOMPARE(match.paths.size(), 1);
            QCOMPARE(match.paths.first().toLower(), path(expected).toLower());
        }
    }

    void pcInstallAmbiguous()
    {
        for (const char* copy : {"quakequest/a/id1/", "quakequest/b/id1/"}) {
            touch(path(QLatin1StringView(copy) + QStringLiteral("pak0.pak")));
            touch(path(QLatin1StringView(copy) + QStringLiteral("pak1.pak")));
        }
        const FilesMatch match =
            FilesFolder(root()).find(QStringLiteral("quakequest"), inputs(quakeInputs), QStringLiteral("quake"));
        QCOMPARE(match.status, FilesMatch::Status::Ambiguous);
        QCOMPARE(match.paths.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestFilesFolder)
#include "tst_filesfolder.moc"
