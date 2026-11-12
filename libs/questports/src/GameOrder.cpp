#include "questports/GameOrder.h"

namespace questports {

namespace {

// Newest first; an invalid date sorts after every valid one. Returns <0, 0 or >0 like compare().
int compareNewestFirst(const QDateTime& a, const QDateTime& b)
{
    if (a.isValid() != b.isValid())
        return a.isValid() ? -1 : 1;
    if (!a.isValid() || a == b)
        return 0;
    return a > b ? -1 : 1;
}

} // namespace

QList<GameOrder> allGameOrders()
{
    return {GameOrder::Title, GameOrder::RecentlyAdded, GameOrder::RecentlyReleased};
}

QString gameOrderKey(GameOrder order)
{
    switch (order) {
    case GameOrder::Title:
        return QStringLiteral("title");
    case GameOrder::RecentlyAdded:
        return QStringLiteral("recently-added");
    case GameOrder::RecentlyReleased:
        return QStringLiteral("recently-released");
    }
    return QStringLiteral("title");
}

GameOrder gameOrderFromKey(const QString& key, GameOrder fallback)
{
    for (GameOrder order : allGameOrders()) {
        if (gameOrderKey(order) == key)
            return order;
    }
    return fallback;
}

GameComparator::GameComparator(GameOrder order)
    : m_order(order)
{
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
}

int GameComparator::compareText(const QString& a, const QString& b) const
{
    return m_collator.compare(a, b);
}

int GameComparator::compareTitles(const Game& a, const Game& b) const
{
    if (const int byTitle = compareText(a.title, b.title))
        return byTitle;
    return a.slug.compare(b.slug);
}

bool GameComparator::lessThan(const Game& a, const Game& b) const
{
    int result = 0;
    switch (m_order) {
    case GameOrder::Title:
        break;
    case GameOrder::RecentlyAdded:
        result = compareNewestFirst(a.addedAt, b.addedAt);
        break;
    case GameOrder::RecentlyReleased:
        result = compareNewestFirst(a.releasedAt, b.releasedAt);
        break;
    }
    if (result == 0)
        result = compareTitles(a, b);
    return result < 0;
}

} // namespace questports
