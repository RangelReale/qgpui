#pragma once

#include "questports/Catalog.h"

#include <QCollator>
#include <QList>
#include <QString>

namespace questports {

// The orders a game list can be shown in.
enum class GameOrder {
    Title,             // A to Z.
    RecentlyAdded,     // Newest addition to QuestPorts first.
    RecentlyReleased,  // Newest GitHub release first; games without one last.
};

// Every order, in the order a menu should list them.
QList<GameOrder> allGameOrders();

// A stable name for settings files, such as "recently-added". gameOrderFromKey returns fallback
// for an unknown key.
QString gameOrderKey(GameOrder order);
GameOrder gameOrderFromKey(const QString& key, GameOrder fallback = GameOrder::Title);

// Compares games for one order. Text compares as people expect: ignoring case, and with numbers in
// numeric order ("Doom 2" before "Doom 10"). Ties fall back to the title, then the slug, so the
// order is total.
class GameComparator
{
public:
    explicit GameComparator(GameOrder order);

    bool operator()(const Game& a, const Game& b) const { return lessThan(a, b); }
    bool lessThan(const Game& a, const Game& b) const;

private:
    int compareText(const QString& a, const QString& b) const;
    int compareTitles(const Game& a, const Game& b) const;

    GameOrder m_order;
    QCollator m_collator;
};

} // namespace questports
