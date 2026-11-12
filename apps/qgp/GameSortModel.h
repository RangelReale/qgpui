#pragma once

#include "GameFlags.h"

#include "questports/GameOrder.h"

#include <QSortFilterProxyModel>

class GameListModel;

// Shows a GameListModel in one of the questports::GameOrder orders, optionally only the games
// with a given flag.
class GameSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GameSortModel(GameListModel* source, QObject* parent = nullptr);

    questports::GameOrder order() const { return m_order; }
    void setOrder(questports::GameOrder order);

    // Shows only games with this flag; no flag shows every game.
    GameFlags filter() const { return m_filter; }
    void setFilter(GameFlags filter);
    // Applies the filter again, after the source's store flags changed.
    void refilter();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    GameListModel* m_source;
    questports::GameOrder m_order = questports::GameOrder::Title;
    questports::GameComparator m_comparator{m_order};
    GameFlags m_filter;
};
