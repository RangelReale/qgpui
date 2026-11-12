#include "GameSortModel.h"

#include "GameListModel.h"

GameSortModel::GameSortModel(GameListModel* source, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_source(source)
{
    setSourceModel(source);
    // Covers arriving change only the decoration; do not re-sort for them.
    setDynamicSortFilter(false);
    sort(0);
    // A reset (a new catalog) clears the sort; apply it again.
    connect(source, &QAbstractItemModel::modelReset, this, [this] { sort(0); });
}

void GameSortModel::setOrder(questports::GameOrder order)
{
    if (order == m_order)
        return;
    m_order = order;
    m_comparator = questports::GameComparator(order);
    invalidate();
    sort(0);
}

bool GameSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return m_comparator.lessThan(m_source->game(left.row()), m_source->game(right.row()));
}

void GameSortModel::setFilter(GameFlags filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    refilter();
}

void GameSortModel::refilter()
{
    invalidateFilter();
    sort(0);
}

bool GameSortModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
    return !m_filter || (m_source->flags(sourceRow) & m_filter);
}
