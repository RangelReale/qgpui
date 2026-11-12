#include "gamestores/Finder.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace gamestores;

namespace {

QString statusName(Location::Status status)
{
    switch (status) {
    case Location::Status::Installed:
        return QStringLiteral("installed");
    case Location::Status::Incomplete:
        return QStringLiteral("incomplete");
    case Location::Status::OwnedNotInstalled:
        return QStringLiteral("owned, not installed");
    }
    return {};
}

} // namespace

// Reads the real Steam and GOG libraries of this PC and matches every recipe's pc-install inputs
// against them. It prints what it finds and fails only when a store cannot be read. Set
// QGP_STORES_LIVE=1 to run it.
class TestLive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("QGP_STORES_LIVE") != QLatin1StringView("1"))
            QSKIP("Set QGP_STORES_LIVE=1 to read this PC's Steam and GOG libraries.");
    }

    void readsTheStoresOfThisPc()
    {
        QStringList warnings;
        const GameFinder finder = GameFinder::forThisPc(&warnings);
        for (const QString& warning : std::as_const(warnings))
            qInfo().noquote() << "warning:" << warning;

        if (finder.steam()) {
            qInfo().noquote() << "Steam:" << finder.steam()->root();
            for (const QString& library : finder.steam()->libraries()) {
                const auto apps = finder.steam()->apps();
                const auto count = std::count_if(apps.cbegin(), apps.cend(),
                                                 [&](const SteamApp& app) { return app.libraryPath == library; });
                qInfo().noquote() << "  library" << library << "-" << count << "apps";
            }
        }
        const QList<GogGame> gog = finder.gog().games();
        const auto installed = std::count_if(gog.cbegin(), gog.cend(), [](const GogGame& g) { return g.installed(); });
        const auto owned = std::count_if(gog.cbegin(), gog.cend(), [](const GogGame& g) { return g.owned; });
        qInfo().noquote() << "GOG:" << gog.size() << "games," << installed << "installed," << owned << "owned";
        for (const GogGame& game : gog) {
            if (game.installed())
                qInfo().noquote() << "  installed" << game.productId << game.name << "-" << game.installPath;
        }

        // Unreadable stores that exist on this PC are failures; absent stores are not.
        for (const QString& warning : std::as_const(warnings))
            QVERIFY2(warning.endsWith(QLatin1StringView("was not found.")), qPrintable(warning));
    }

    void matchesTheRecipes()
    {
        const GameFinder finder = GameFinder::forThisPc();
        const QDir recipes(QStringLiteral(QGP_RECIPES_DIR));
        int inputs = 0;
        for (const QString& name : recipes.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
            QFile file(recipes.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QJsonObject recipe = QJsonDocument::fromJson(file.readAll()).object();
            for (const QJsonValue& value : recipe.value(QLatin1StringView("inputs")).toArray()) {
                const QJsonObject input = value.toObject();
                if (input.value(QLatin1StringView("kind")).toString() != QLatin1StringView("pc-install"))
                    continue;
                ++inputs;
                for (const Location& location : finder.find(input)) {
                    qInfo().noquote() << recipe.value(QLatin1StringView("slug")).toString() + u'/' +
                                             input.value(QLatin1StringView("id")).toString()
                                      << "-" << statusName(location.status) << "on" << location.store
                                      << location.productId << location.name
                                      << (location.path.isEmpty() ? QString() : QStringLiteral("- ") + location.path);
                }
            }
        }
        qInfo().noquote() << "Checked" << inputs << "pc-install inputs.";
        QVERIFY(inputs > 0);
    }
};

QTEST_GUILESS_MAIN(TestLive)
#include "tst_live.moc"
