#pragma once

#include "questports/GameOrder.h"

#include <QSortFilterProxyModel>

class GameListModel;

// Shows a GameListModel in one of the questports::GameOrder orders.
class GameSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit GameSortModel(GameListModel* source, QObject* parent = nullptr);

    questports::GameOrder order() const { return m_order; }
    void setOrder(questports::GameOrder order);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    GameListModel* m_source;
    questports::GameOrder m_order = questports::GameOrder::Title;
    questports::GameComparator m_comparator{m_order};
};
