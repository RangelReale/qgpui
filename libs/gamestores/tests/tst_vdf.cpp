#include "gamestores/Vdf.h"

#include <QTest>

using namespace gamestores;

class TestVdf : public QObject
{
    Q_OBJECT

private slots:
    void readsNestedObjectsWithKeysInAnyCase()
    {
        QString error;
        const auto vdf = parseVdf(R"("AppState"
{
	"appid"		"546560"
	"StateFlags"		"4"
	"UserConfig"
	{
		"BetaKey"		"steam_legacy"
	}
}
)",
                                  &error);
        QVERIFY2(vdf, qPrintable(error));
        QCOMPARE(vdfString(*vdf, {QStringLiteral("appstate"), QStringLiteral("APPID")}), QStringLiteral("546560"));
        QCOMPARE(vdfString(*vdf, {QStringLiteral("AppState"), QStringLiteral("stateflags")}), QStringLiteral("4"));
        QCOMPARE(vdfString(*vdf, {QStringLiteral("AppState"), QStringLiteral("UserConfig"), QStringLiteral("betakey")}),
                 QStringLiteral("steam_legacy"));
        QCOMPARE(vdfObject(*vdf, {QStringLiteral("AppState"), QStringLiteral("userconfig")}).size(), 1);
    }

    void readsEscapesCommentsUnquotedTokensAndConditionals()
    {
        QString error;
        // An ordinary literal, not a raw string: moc reads // inside a raw string as a comment.
        const auto vdf = parseVdf("// a comment\n"
                                  "root {\n"
                                  "    \"path\" \"C:\\\\Jogos\\\\Steam\"   // trailing comment\n"
                                  "    \"quote\" \"say \\\"hi\\\"\\tnow\"\n"
                                  "    bare value\n"
                                  "    \"win\" \"1\" [$WIN32]\n"
                                  "    \"dup\" \"first\"\n"
                                  "    \"DUP\" \"second\"\n"
                                  "}\n",
                                  &error);
        QVERIFY2(vdf, qPrintable(error));
        const VdfObject root = vdfObject(*vdf, {QStringLiteral("root")});
        QCOMPARE(root.value(QStringLiteral("path")).toString(), QStringLiteral("C:\\Jogos\\Steam"));
        QCOMPARE(root.value(QStringLiteral("quote")).toString(), QStringLiteral("say \"hi\"\tnow"));
        QCOMPARE(root.value(QStringLiteral("bare")).toString(), QStringLiteral("value"));
        QCOMPARE(root.value(QStringLiteral("win")).toString(), QStringLiteral("1"));
        QCOMPARE(root.value(QStringLiteral("dup")).toString(), QStringLiteral("second"));
    }

    void missingKeysGiveEmptyResults()
    {
        const auto vdf = parseVdf(R"("a" { "b" "c" })");
        QVERIFY(vdf);
        QVERIFY(vdfString(*vdf, {QStringLiteral("a"), QStringLiteral("x")}).isEmpty());
        QVERIFY(vdfString(*vdf, {QStringLiteral("a")}).isEmpty()); // An object, not a string.
        QVERIFY(vdfObject(*vdf, {QStringLiteral("a"), QStringLiteral("b")}).isEmpty()); // A string, not an object.
    }

    void rejectsBrokenText_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("message");
        // Ordinary literals: moc counts braces inside raw strings, and these are unbalanced on purpose.
        QTest::newRow("unterminated string") << QByteArray("\"a\" \"b") << QStringLiteral("Unterminated string");
        QTest::newRow("missing brace") << QByteArray("\"a\" \x7b \"b\" \"c\"") << QStringLiteral("Missing }");
        QTest::newRow("extra brace") << QByteArray("\"a\" \"b\" \x7d") << QStringLiteral("Unexpected }");
        QTest::newRow("missing value") << QByteArray("\"a\"") << QStringLiteral("Missing value");
    }

    void rejectsBrokenText()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, message);
        QString error;
        QVERIFY(!parseVdf(text, &error));
        QVERIFY2(error.contains(message), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestVdf)
#include "tst_vdf.moc"
