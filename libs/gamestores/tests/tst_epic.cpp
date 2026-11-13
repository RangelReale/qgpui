#include "gamestores/Epic.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace gamestores;

namespace {

void write(const QString& path, const QByteArray& content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

// A manifest as the launcher writes it, with the fields EpicLibrary reads and a few it does not.
// Backslashes are doubled, as in the launcher's JSON.
QByteArray item(const char* appName, const char* name, const char* location, const char* mainGame,
                bool incomplete = false)
{
    return QByteArray("{\"FormatVersion\": 0, \"bIsIncompleteInstall\": ") + (incomplete ? "true" : "false") +
           ", \"LaunchExecutable\": \"Game.exe\", \"DisplayName\": \"" + name + "\", \"InstallLocation\": \"" +
           location + "\", \"AppName\": \"" + appName + "\", \"MainGameAppName\": \"" + mainGame +
           "\", \"bIsApplication\": true}";
}

} // namespace

class TestEpic : public QObject
{
    Q_OBJECT

private slots:
    void readsManifests()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QDir dir(tmp.path());
#ifdef Q_OS_WIN
        const char* morrowindPath = "N:\\\\Epic Games\\\\Morrowind"; // Native separators.
#else
        const char* morrowindPath = "N:/Epic Games/Morrowind";
#endif
        write(dir.filePath(QStringLiteral("A1.item")), item("Morrowind", "Morrowind GOTY", morrowindPath, ""));
        // Older launchers set MainGameAppName to the game's own name.
        write(dir.filePath(QStringLiteral("B2.item")), item("Amnesia", "Amnesia", "C:/Games/Amnesia", "Amnesia", true));
        // An add-on of Morrowind: skipped.
        write(dir.filePath(QStringLiteral("C3.item")), item("Bloodmoon", "Bloodmoon", morrowindPath, "Morrowind"));
        // No install folder, not JSON, not an .item file: skipped.
        write(dir.filePath(QStringLiteral("D4.item")), item("Nothing", "Nothing", "", ""));
        write(dir.filePath(QStringLiteral("E5.item")), "not json");
        write(dir.filePath(QStringLiteral("F6.txt")), item("Text", "Text", "C:/Text", ""));

        QString error;
        const auto library = EpicLibrary::load(tmp.path(), &error);
        QVERIFY2(library, qPrintable(error));
        QCOMPARE(library->games().size(), 2);

        const EpicGame morrowind = library->games().value(QStringLiteral("Morrowind"));
        QCOMPARE(morrowind.name, QStringLiteral("Morrowind GOTY"));
        QCOMPARE(morrowind.installPath, QStringLiteral("N:/Epic Games/Morrowind"));
        QVERIFY(morrowind.complete);

        const EpicGame amnesia = library->games().value(QStringLiteral("Amnesia"));
        QCOMPARE(amnesia.installPath, QStringLiteral("C:/Games/Amnesia"));
        QVERIFY(!amnesia.complete);
    }

    void failsWithoutTheFolder()
    {
        QTemporaryDir tmp;
        QString error;
        QVERIFY(!EpicLibrary::load(tmp.filePath(QStringLiteral("missing")), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!EpicLibrary::load(QString()));
    }
};

QTEST_GUILESS_MAIN(TestEpic)
#include "tst_epic.moc"
