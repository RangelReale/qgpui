#include "questports/GameOrder.h"

#include <QTest>

#include <algorithm>

using namespace questports;

namespace {

Game game(const QString& slug, const QString& title, const QString& developer = {},
          const QDateTime& addedAt = {}, const QDateTime& releasedAt = {})
{
    Game g;
    g.slug = slug;
    g.title = title;
    g.developer = developer;
    g.addedAt = addedAt;
    g.releasedAt = releasedAt;
    return g;
}

QDateTime day(int d)
{
    return QDateTime(QDate(2026, 10, d), QTime(12, 0), QTimeZone::UTC);
}

QStringList sortedSlugs(QList<Game> games, GameOrder order)
{
    std::sort(games.begin(), games.end(), GameComparator(order));
    QStringList slugs;
    for (const Game& g : games)
        slugs.append(g.slug);
    return slugs;
}

} // namespace

class TestGameOrder : public QObject
{
    Q_OBJECT

private slots:
    void titleIgnoresCaseAndSortsNumbers()
    {
        const QList<Game> games{game(QStringLiteral("d10"), QStringLiteral("Doom 10")),
                                game(QStringLiteral("d2"), QStringLiteral("Doom 2")),
                                game(QStringLiteral("a"), QStringLiteral("alpha")),
                                game(QStringLiteral("b"), QStringLiteral("Beta"))};
        QCOMPARE(sortedSlugs(games, GameOrder::Title),
                 (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("d2"), QStringLiteral("d10")}));
    }

    void titleTiesFallBackToSlug()
    {
        const QList<Game> games{game(QStringLiteral("z"), QStringLiteral("Same")),
                                game(QStringLiteral("y"), QStringLiteral("Same"))};
        QCOMPARE(sortedSlugs(games, GameOrder::Title), (QStringList{QStringLiteral("y"), QStringLiteral("z")}));
    }

    void recentlyAddedIsNewestFirst()
    {
        const QList<Game> games{game(QStringLiteral("old"), QStringLiteral("A"), {}, day(1)),
                                game(QStringLiteral("new"), QStringLiteral("B"), {}, day(9)),
                                game(QStringLiteral("mid"), QStringLiteral("C"), {}, day(5))};
        QCOMPARE(sortedSlugs(games, GameOrder::RecentlyAdded),
                 (QStringList{QStringLiteral("new"), QStringLiteral("mid"), QStringLiteral("old")}));
    }

    void recentlyReleasedPutsUnknownLast()
    {
        const QList<Game> games{game(QStringLiteral("none-b"), QStringLiteral("B")),
                                game(QStringLiteral("old"), QStringLiteral("Z"), {}, {}, day(1)),
                                game(QStringLiteral("none-a"), QStringLiteral("A")),
                                game(QStringLiteral("new"), QStringLiteral("Y"), {}, {}, day(9))};
        QCOMPARE(sortedSlugs(games, GameOrder::RecentlyReleased),
                 (QStringList{QStringLiteral("new"), QStringLiteral("old"), QStringLiteral("none-a"),
                              QStringLiteral("none-b")}));
    }

    void developerThenTitle()
    {
        const QList<Game> games{game(QStringLiteral("b2"), QStringLiteral("Two"), QStringLiteral("bassquake")),
                                game(QStringLiteral("a"), QStringLiteral("Zed"), QStringLiteral("Alpha")),
                                game(QStringLiteral("b1"), QStringLiteral("One"), QStringLiteral("Bassquake"))};
        QCOMPARE(sortedSlugs(games, GameOrder::Developer),
                 (QStringList{QStringLiteral("a"), QStringLiteral("b1"), QStringLiteral("b2")}));
    }

    void keysRoundTrip()
    {
        for (GameOrder order : allGameOrders())
            QCOMPARE(gameOrderFromKey(gameOrderKey(order)), order);
        QCOMPARE(gameOrderFromKey(QStringLiteral("nonsense"), GameOrder::Developer), GameOrder::Developer);
    }
};

QTEST_GUILESS_MAIN(TestGameOrder)
#include "tst_gameorder.moc"
