#include "gamestores/Gog.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace gamestores;

namespace {

// Builds a database with the tables and columns of GOG Galaxy 2 that the reader uses.
bool makeGalaxyDatabase(const QString& path)
{
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("make"));
        db.setDatabaseName(path);
        ok = db.open();
        QSqlQuery q(db);
        const QStringList statements{
            QStringLiteral("CREATE TABLE GamePieceTypes (id INTEGER, type TEXT)"),
            QStringLiteral("CREATE TABLE GamePieces (releaseKey TEXT, gamePieceTypeId INTEGER, userId INTEGER, value TEXT)"),
            QStringLiteral("CREATE TABLE LibraryReleases (id INTEGER, userId INTEGER, releaseKey TEXT)"),
            QStringLiteral("CREATE TABLE LicensedReleases (libraryId INTEGER, isOwned INTEGER)"),
            QStringLiteral("CREATE TABLE InstalledBaseProducts (productId INTEGER, installationPath TEXT, buildId TEXT)"),
            QStringLiteral("INSERT INTO GamePieceTypes VALUES (280, 'title'), (279, 'summary')"),
            QStringLiteral("INSERT INTO GamePieces VALUES ('gog_1', 280, 1, '{\"title\":\"Master of Orion Classic\"}'), "
                           "('gog_2', 280, 1, '{\"title\":\"Ultima IV\"}'), ('gog_2', 279, 1, '{\"summary\":\"x\"}')"),
            QStringLiteral("INSERT INTO LibraryReleases VALUES (10, 1, 'gog_1'), (11, 1, 'gog_2'), (12, 1, 'gog_3'), "
                           "(13, 1, 'steam_70')"),
            QStringLiteral("INSERT INTO LicensedReleases VALUES (10, 1), (11, 1), (12, 0), (13, 1)"),
            QStringLiteral("INSERT INTO InstalledBaseProducts VALUES (1, 'D:/Games/GalaxyClient/Games/Master of Orion', '52')"),
        };
        for (const QString& statement : statements)
            ok = ok && q.exec(statement);
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("make"));
    return ok;
}

} // namespace

class TestGog : public QObject
{
    Q_OBJECT

private slots:
    void readsOwnedAndInstalledGamesFromGalaxy()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("galaxy-2.0.db"));
        QVERIFY(makeGalaxyDatabase(path));

        GogLibrary gog;
        QString error;
        QVERIFY2(gog.addGalaxyDatabase(path, &error), qPrintable(error));
        QCOMPARE(gog.games().size(), 2); // gog_3 is not owned; steam_70 is another platform.

        const auto moo = gog.game(QStringLiteral("1"));
        QVERIFY(moo);
        QCOMPARE(moo->name, QStringLiteral("Master of Orion Classic"));
        QVERIFY(moo->owned);
        QVERIFY(moo->installed());
        QCOMPARE(moo->installPath, QStringLiteral("D:/Games/GalaxyClient/Games/Master of Orion"));
        QCOMPARE(moo->buildId, QStringLiteral("52"));

        const auto u4 = gog.game(QStringLiteral("2"));
        QVERIFY(u4);
        QVERIFY(u4->owned);
        QVERIFY(!u4->installed());
        QVERIFY(QFile::exists(path)); // The original is left in place; only a copy was opened.
    }

    void reportsAMissingOrForeignDatabase()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        GogLibrary gog;
        QString error;
        QVERIFY(!gog.addGalaxyDatabase(tmp.filePath(QStringLiteral("missing.db")), &error));
        QVERIFY2(error.contains(QStringLiteral("Cannot copy")), qPrintable(error));

        // A SQLite file without Galaxy's tables: the schema changed or it is another file.
        const QString other = tmp.filePath(QStringLiteral("other.db"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("other"));
            db.setDatabaseName(other);
            QVERIFY(db.open());
            QVERIFY(QSqlQuery(db).exec(QStringLiteral("CREATE TABLE t (x INTEGER)")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("other"));
        QVERIFY(!gog.addGalaxyDatabase(other, &error));
        QVERIFY2(error.contains(QStringLiteral("GOG Galaxy database")), qPrintable(error));
    }

    void mergesSourcesByProductId()
    {
        GogLibrary gog;
        gog.add({QStringLiteral("5"), {}, QStringLiteral("C:/GOG Games/Freespace 2"), {}, false});
        gog.add({QStringLiteral("5"), QStringLiteral("Freespace 2"), QStringLiteral("D:/elsewhere"), QStringLiteral("7"), true});
        gog.add({{}, QStringLiteral("no id"), {}, {}, true}); // Ignored.
        QCOMPARE(gog.games().size(), 1);
        const auto fs2 = gog.game(QStringLiteral("5"));
        QCOMPARE(fs2->name, QStringLiteral("Freespace 2"));
        QCOMPARE(fs2->installPath, QStringLiteral("C:/GOG Games/Freespace 2")); // The first path stays.
        QCOMPARE(fs2->buildId, QStringLiteral("7"));
        QVERIFY(fs2->owned);
    }

    void readsTheInfoFileOfTheBaseGame()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        auto write = [&](const QString& name, const QByteArray& json) {
            QFile file(tmp.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(json);
        };
        // A DLC's .info sorts first; the reader must skip it for the base game's.
        write(QStringLiteral("goggame-1000.info"), R"json({"gameId":"1000","rootGameId":"1207661623","name":"Some DLC"})json");
        write(QStringLiteral("goggame-1207661623.info"),
              R"json({"gameId":1207661623,"rootGameId":"1207661623","name":"Master of Orion (Classic)","buildId":"52"})json");

        const auto info = GogLibrary::readInfoFile(tmp.path());
        QVERIFY(info);
        QCOMPARE(info->productId, QStringLiteral("1207661623")); // A number in the file is accepted.
        QCOMPARE(info->name, QStringLiteral("Master of Orion (Classic)"));
        QCOMPARE(info->buildId, QStringLiteral("52"));
        QCOMPARE(info->installPath, QDir::cleanPath(tmp.path()));

        QTemporaryDir empty;
        QVERIFY(!GogLibrary::readInfoFile(empty.path()));
    }
};

QTEST_GUILESS_MAIN(TestGog)
#include "tst_gog.moc"
