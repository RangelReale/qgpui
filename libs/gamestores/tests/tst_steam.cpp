#include "gamestores/Steam.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace gamestores;

namespace {

void writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

QByteArray manifest(const QByteArray& appId, const QByteArray& name, const QByteArray& dir,
                    const QByteArray& flags, const QByteArray& extra = {})
{
    return "\"AppState\"\n{\n\t\"appid\"\t\t\"" + appId + "\"\n\t\"name\"\t\t\"" + name +
           "\"\n\t\"StateFlags\"\t\t\"" + flags + "\"\n\t\"installdir\"\t\t\"" + dir +
           "\"\n\t\"buildid\"\t\t\"123\"\n" + extra + "}\n";
}

QByteArray escaped(const QString& path)
{
    return QDir::toNativeSeparators(path).replace(QStringLiteral("\\"), QStringLiteral("\\\\")).toUtf8();
}

} // namespace

class TestSteam : public QObject
{
    Q_OBJECT

private slots:
    void readsEveryListedLibrary()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.filePath(QStringLiteral("Steam"));
        const QString second = tmp.filePath(QStringLiteral("D/Games/Steam"));
        const QString third = tmp.filePath(QStringLiteral("E/Games/Steam"));
        const QString gone = tmp.filePath(QStringLiteral("X/Steam")); // A drive that is not connected.

        writeFile(root + QStringLiteral("/steamapps/libraryfolders.vdf"),
                  "\"libraryfolders\"\n{\n"
                  "\t\"0\" { \"path\" \"" + escaped(root) + "\" \"apps\" { \"546560\" \"1\" } }\n"
                  "\t\"1\" { \"path\" \"" + escaped(second) + "\" }\n"
                  "\t\"2\" { \"path\" \"" + escaped(third) + "\" }\n"
                  "\t\"3\" { \"path\" \"" + escaped(gone) + "\" }\n}\n");
        writeFile(root + QStringLiteral("/steamapps/appmanifest_546560.acf"),
                  manifest("546560", "Half-Life: Alyx", "Half-Life Alyx", "4"));
        writeFile(second + QStringLiteral("/steamapps/appmanifest_70.acf"),
                  manifest("70", "Half-Life", "Half-Life", "6",
                           "\t\"UserConfig\" { \"BetaKey\" \"steam_legacy\" }\n"));
        writeFile(third + QStringLiteral("/steamapps/appmanifest_2310.acf"),
                  manifest("2310", "Quake", "Quake", "1026")); // Updating: bit 4 not set.
        writeFile(third + QStringLiteral("/steamapps/appmanifest_9.acf"), "\"AppState\" {"); // Broken: skipped.

        QString error;
        const auto steam = SteamLibrary::load(root, &error);
        QVERIFY2(steam, qPrintable(error));
        QCOMPARE(steam->libraries(), (QStringList{QDir::cleanPath(root), QDir::cleanPath(second), QDir::cleanPath(third)}));
        QCOMPARE(steam->apps().size(), 3);

        const auto alyx = steam->app(QStringLiteral("546560"));
        QVERIFY(alyx);
        QCOMPARE(alyx->name, QStringLiteral("Half-Life: Alyx"));
        QCOMPARE(alyx->installPath, QDir::cleanPath(root + QStringLiteral("/steamapps/common/Half-Life Alyx")));
        QVERIFY(alyx->fullyInstalled);
        QCOMPARE(alyx->buildId, QStringLiteral("123"));
        QVERIFY(alyx->betaBranch.isEmpty());

        const auto hl = steam->app(QStringLiteral("70"));
        QVERIFY(hl);
        QCOMPARE(hl->libraryPath, QDir::cleanPath(second));
        QVERIFY(hl->fullyInstalled);
        QCOMPARE(hl->betaBranch, QStringLiteral("steam_legacy"));

        const auto quake = steam->app(QStringLiteral("2310"));
        QVERIFY(quake);
        QVERIFY(!quake->fullyInstalled);
        QVERIFY(!steam->app(QStringLiteral("9")));
    }

    void readsTheOldLibraryLayoutAndSkipsDuplicates()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString root = tmp.filePath(QStringLiteral("Steam"));
        const QString other = tmp.filePath(QStringLiteral("Games"));
        writeFile(root + QStringLiteral("/steamapps/libraryfolders.vdf"),
                  "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\" \"1\"\n\t\"ContentStatsID\" \"2\"\n"
                  "\t\"1\" \"" + escaped(other) + "\"\n\t\"2\" \"" + escaped(other + QStringLiteral("/")) + "\"\n}\n");
        writeFile(other + QStringLiteral("/steamapps/appmanifest_10.acf"), manifest("10", "Counter-Strike", "Half-Life", "4"));

        const auto steam = SteamLibrary::load(root);
        QVERIFY(steam);
        QCOMPARE(steam->libraries().size(), 2);
        QVERIFY(steam->app(QStringLiteral("10")));
    }

    void worksWithoutLibraryFoldersFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.filePath(QStringLiteral("steamapps/appmanifest_10.acf")), manifest("10", "CS", "Half-Life", "4"));
        const auto steam = SteamLibrary::load(tmp.path());
        QVERIFY(steam);
        QCOMPARE(steam->libraries().size(), 1);
        QVERIFY(steam->app(QStringLiteral("10")));
    }

    void rejectsAFolderWithoutSteamapps()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QString error;
        QVERIFY(!SteamLibrary::load(tmp.path(), &error));
        QVERIFY2(error.contains(QStringLiteral("not a Steam folder")), qPrintable(error));
    }

    void rejectsABrokenLibraryFoldersFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        writeFile(tmp.filePath(QStringLiteral("steamapps/libraryfolders.vdf")), "\"libraryfolders\" {");
        QString error;
        QVERIFY(!SteamLibrary::load(tmp.path(), &error));
        QVERIFY2(error.contains(QStringLiteral("Missing }")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestSteam)
#include "tst_steam.moc"
