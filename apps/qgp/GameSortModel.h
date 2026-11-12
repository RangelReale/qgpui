#pragma once

#include "StoreAvailability.h"

#include "questports/GameOrder.h"

#include <QSortFilterProxyModel>

class GameListModel;

// Shows a GameListModel in one of the questports::GameOrder orders, optionally only the games
// with a given store flag.
class GameSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GameSortModel(GameListModel* source, QObject* parent = nullptr);

    questports::GameOrder order() const { return m_order; }
    void setOrder(questports::GameOrder order);

    // Shows only games with this flag; no flag shows every game.
    StoreFlags storeFilter() const { return m_storeFilter; }
    void setStoreFilter(StoreFlags filter);
    // Applies the filter again, after the source's store flags changed.
    void refilter();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    GameListModel* m_source;
    questports::GameOrder m_order = questports::GameOrder::Title;
    questports::GameComparator m_comparator{m_order};
    StoreFlags m_storeFilter;
};
