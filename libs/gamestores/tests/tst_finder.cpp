#include "gamestores/Finder.h"

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
QJsonObject input(QByteArray json)
{
    return QJsonDocument::fromJson(json.replace('\'', '"')).object();
}

} // namespace

class TestFinder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString m_steamRoot;
    QString m_quake;   // Steam 2310, complete.
    QString m_doom3;   // Steam 9050, not fully installed.
    QString m_mohaa;   // Steam 999, a game no recipe names by ID.
    QString m_gogQuake; // GOG pack install, no ID in the recipe.
    QString m_fs2;      // GOG 5, files missing.
    QString m_epicMorrowind; // Epic, complete, no ID in the recipe.
    QString m_epicAmnesia;   // Epic, incomplete.

    GameFinder makeFinder()
    {
        QString error;
        auto steam = SteamLibrary::load(m_steamRoot, &error);
        if (!steam)
            qFatal("%s", qPrintable(error));
        GogLibrary gog;
        gog.add({QStringLiteral("1440166133"), QStringLiteral("Quake"), m_gogQuake, {}, true});
        gog.add({QStringLiteral("5"), QStringLiteral("Freespace 2"), m_fs2, {}, true});
        gog.add({QStringLiteral("1207658962"), QStringLiteral("Ultima IV"), {}, {}, true}); // Owned only.
        EpicLibrary epic;
        epic.add({QStringLiteral("Morrowind"), QStringLiteral("Morrowind GOTY"), m_epicMorrowind, true});
        epic.add({QStringLiteral("Amnesia"), QStringLiteral("Amnesia"), m_epicAmnesia, false}); // Still downloading.
        return GameFinder(std::move(steam), gog, epic);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_steamRoot = m_tmp.filePath(QStringLiteral("Steam"));
        const QString apps = m_steamRoot + QStringLiteral("/steamapps/");
        auto manifest = [&](const char* id, const char* dir, const char* flags) {
            QFile file(apps + QStringLiteral("appmanifest_%1.acf").arg(QLatin1StringView(id)));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QByteArray("\"AppState\" { \"appid\" \"") + id + "\" \"name\" \"" + dir + "\" \"installdir\" \"" +
                       dir + "\" \"StateFlags\" \"" + flags + "\" }");
        };
        QDir().mkpath(apps);
        manifest("2310", "Quake", "4");
        manifest("9050", "Doom 3", "1026");
        manifest("999", "MOHAA", "4");

        m_quake = QDir::cleanPath(apps + QStringLiteral("common/Quake"));
        touch(m_quake + QStringLiteral("/id1/pak0.pak"));
        touch(m_quake + QStringLiteral("/id1/pak1.pak"));
        m_doom3 = QDir::cleanPath(apps + QStringLiteral("common/Doom 3"));
        touch(m_doom3 + QStringLiteral("/base/pak000.pk4"));
        m_mohaa = QDir::cleanPath(apps + QStringLiteral("common/MOHAA"));
        touch(m_mohaa + QStringLiteral("/main/Pak0.pk3"));

        m_gogQuake = QDir::cleanPath(m_tmp.filePath(QStringLiteral("GOG Games/Quake")));
        touch(m_gogQuake + QStringLiteral("/id1/pak0.pak"));
        touch(m_gogQuake + QStringLiteral("/id1/pak1.pak"));
        m_fs2 = QDir::cleanPath(m_tmp.filePath(QStringLiteral("GOG Games/Freespace 2")));
        QDir().mkpath(m_fs2);

        m_epicMorrowind = QDir::cleanPath(m_tmp.filePath(QStringLiteral("Epic Games/Morrowind")));
        touch(m_epicMorrowind + QStringLiteral("/Data Files/Morrowind.esm"));
        m_epicAmnesia = QDir::cleanPath(m_tmp.filePath(QStringLiteral("Epic Games/Amnesia")));
        touch(m_epicAmnesia + QStringLiteral("/Amnesia.exe"));
    }

    void findsEpicInstalls()
    {
        const GameFinder finder = makeFinder();
        // By content: the recipe lists Epic without an ID.
        auto found = finder.find(input("{'stores':[{'store':'epic'}],'detect':['Data Files/Morrowind.esm']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::Installed);
        QCOMPARE(found[0].store, QStringLiteral("epic"));
        QCOMPARE(found[0].productId, QStringLiteral("Morrowind"));
        QCOMPARE(found[0].path, m_epicMorrowind);

        // By app name, but the launcher marks the install incomplete.
        found = finder.find(input("{'stores':[{'store':'epic','appId':'Amnesia'}],'detect':['Amnesia.exe']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::Incomplete);

        // Step 3 reports only complete installs.
        QVERIFY(finder.find(input("{'detect':['Amnesia.exe']}")).isEmpty());
    }

    void findsBySteamIdAndByContent()
    {
        const GameFinder finder = makeFinder();
        const auto found = finder.find(input("{'kind':'pc-install','stores':[{'store':'steam','appId':'2310'}],'root':'id1','detect':['pak0.pak','pak1.pak']}"));
        QCOMPARE(found.size(), 2);
        QCOMPARE(found[0].status, Location::Status::Installed);
        QCOMPARE(found[0].store, QStringLiteral("steam"));
        QCOMPARE(found[0].productId, QStringLiteral("2310"));
        QCOMPARE(found[0].path, m_quake + QStringLiteral("/id1"));
        // The GOG pack has no ID in the recipe; its files identify it.
        QCOMPARE(found[1].status, Location::Status::Installed);
        QCOMPARE(found[1].store, QStringLiteral("gog"));
        QCOMPARE(found[1].path, m_gogQuake + QStringLiteral("/id1"));
    }

    void reportsIncompleteInstallsFoundById()
    {
        const GameFinder finder = makeFinder();
        // Doom 3 is listed by Steam but still updating.
        auto found = finder.find(input("{'stores':[{'store':'steam','appId':'9050'}],'root':'base','detect':['pak000.pk4']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::Incomplete);
        QCOMPARE(found[0].path, m_doom3 + QStringLiteral("/base"));

        // Freespace 2 is installed from GOG, but the files are missing.
        found = finder.find(input("{'stores':[{'store':'gog','appId':'5'}],'detect':['root_fs2.vp']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::Incomplete);
        QCOMPARE(found[0].path, m_fs2);
    }

    void reportsOwnedGogGamesThatAreNotInstalled()
    {
        const GameFinder finder = makeFinder();
        const auto found = finder.find(input("{'stores':[{'store':'gog','appId':'1207658962'}],'detect':['AVATAR.EXE']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::OwnedNotInstalled);
        QCOMPARE(found[0].name, QStringLiteral("Ultima IV"));
        QVERIFY(found[0].path.isEmpty());
    }

    void findsByInstallFolderNameAndWildcards()
    {
        const GameFinder finder = makeFinder();
        const auto found = finder.find(input("{'stores':[{'store':'steam','installDir':'mohaa'}],'root':'main','detect':['*.pk3']}"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0].status, Location::Status::Installed);
        QCOMPARE(found[0].productId, QStringLiteral("999"));
    }

    void findsNothingForUnknownGames()
    {
        const GameFinder finder = makeFinder();
        QVERIFY(finder.find(input("{'stores':[{'store':'steam','appId':'1'}],'detect':['nothing.dat']}")).isEmpty());
        QVERIFY(finder.find(input("{'detect':['nothing/*.dat']}")).isEmpty());
    }

    void appliesRootOnlyWhenItExists()
    {
        QCOMPARE(GameFinder::applyRoot(m_quake, QStringLiteral("id1")), m_quake + QStringLiteral("/id1"));
        // The user picked the id1 folder itself.
        QCOMPARE(GameFinder::applyRoot(m_quake + QStringLiteral("/id1"), QStringLiteral("id1")),
                 m_quake + QStringLiteral("/id1"));
        QCOMPARE(GameFinder::applyRoot(m_quake, {}), m_quake);
        // A root from the catalog that would leave the install is ignored, even when it exists.
        QCOMPARE(GameFinder::applyRoot(m_quake + QStringLiteral("/id1"), QStringLiteral("..")),
                 m_quake + QStringLiteral("/id1"));
        QCOMPARE(GameFinder::applyRoot(m_quake, m_tmp.path()), m_quake);
    }

    void matchesDetectPaths()
    {
        QVERIFY(GameFinder::matchesDetect(m_quake, {QStringLiteral("id1/pak0.pak"), QStringLiteral("id1/pak1.pak")}));
        QVERIFY(GameFinder::matchesDetect(m_quake, {QStringLiteral("*/pak?.pak")}));
        QVERIFY(GameFinder::matchesDetect(m_quake, {QStringLiteral("id1")})); // A folder counts.
        QVERIFY(!GameFinder::matchesDetect(m_quake, {QStringLiteral("id1/pak0.pak"), QStringLiteral("id1/pak2.pak")}));
        QVERIFY(!GameFinder::matchesDetect(m_tmp.filePath(QStringLiteral("missing")), {QStringLiteral("x")}));
        // Paths that would leave the install match nothing, even when they exist.
        QVERIFY(!GameFinder::matchesDetect(m_quake + QStringLiteral("/id1"), {QStringLiteral("../id1/pak0.pak")}));
        QVERIFY(!GameFinder::matchesDetect(m_quake, {m_quake + QStringLiteral("/id1/pak0.pak")}));
    }
};

QTEST_GUILESS_MAIN(TestFinder)
#include "tst_finder.moc"
